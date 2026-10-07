#include <stdio.h>
#include <string.h>
#include <threads.h>
#include "headset/headset_ops.h"
#include "headset/headset_layer.h"
#include "util.h"
#include "headset/headset_openxr.h"

static const HeadsetOps* ops = &lovrHeadsetOpenXROps;
static atomic_flag lifecycleLock = ATOMIC_FLAG_INIT;
static unsigned clients;
static enum { HEADSET_EMPTY, HEADSET_LIVE, HEADSET_CLEANUP } lifecycle;
static HeadsetConfig config;
static HeadsetConfig overlayConfig;
static bool initialized;
static bool xrInitialized;
static bool vrInitialized;
static bool frozen;
static bool pending;
static uint32_t sessionGeneration;

uint32_t lovrHeadsetNextSessionGeneration(void) {
  lovrAssert(sessionGeneration < UINT32_MAX, "Headset session generation exhausted");
  return ++sessionGeneration;
}

bool lovrLayerIsValid(Layer* layer) {
  lovrAssert(layer, "Layer is null");
  LayerHeader* header = (LayerHeader*) layer;
  lovrAssert(header->creator == ops && header->generation != 0 &&
    header->generation == ops->HeadsetGetSessionGeneration(), "Layer belongs to an inactive headset session");
  return true;
}

static void lockLifecycle(void) {
  while (atomic_flag_test_and_set(&lifecycleLock)) thrd_yield();
}

static bool cleanup(void) {
  if (!lovrHeadsetBeforeGraphicsDestroy()) return false;
#ifdef LOVR_ENABLE_OPENVR
  if (vrInitialized) lovrHeadsetOpenVROps.HeadsetDestroy();
#endif
  if (xrInitialized) lovrHeadsetOpenXROps.HeadsetDestroy();
  lovrFree(config.extensions);
  memset(&config, 0, sizeof(config));
  memset(&overlayConfig, 0, sizeof(overlayConfig));
  initialized = xrInitialized = vrInitialized = pending = false;
  ops = &lovrHeadsetOpenXROps;
  lifecycle = HEADSET_EMPTY;
  return true;
}

bool lovrHeadsetInit(HeadsetConfig* incoming) {
  lovrAssert(incoming, "Headset config is null");
  lockLifecycle();
  if (lifecycle == HEADSET_CLEANUP && !cleanup()) {
    char error[1024];
    snprintf(error, sizeof(error), "%s", lovrGetError());
    lovrFree(incoming->extensions);
    lovrSetError("Headset initialization: cleanup is pending: %s", error);
    atomic_flag_clear(&lifecycleLock);
    return false;
  }
  if (lifecycle == HEADSET_LIVE) {
    clients++;
    lovrFree(incoming->extensions);
    atomic_flag_clear(&lifecycleLock);
    return true;
  }
  config = *incoming;
  config.connect = incoming->connectConfigured ? incoming->connect : true;
  overlayConfig = config;
  overlayConfig.overlay = true;
  ops = &lovrHeadsetOpenXROps;
  initialized = true;
  xrInitialized = true;
  lifecycle = HEADSET_CLEANUP;
  if (!ops->HeadsetInit(&config)) {
    char error[1024];
    snprintf(error, sizeof(error), "%s", lovrGetError());
    cleanup();
    lovrSetError("Headset initialization: %s", error);
    atomic_flag_clear(&lifecycleLock);
    return false;
  }
  clients = 1;
  lifecycle = HEADSET_LIVE;
  atomic_flag_clear(&lifecycleLock);
  return true;
}

void lovrHeadsetDestroy(void) {
  lockLifecycle();
  if (lifecycle == HEADSET_LIVE && --clients == 0) lifecycle = HEADSET_CLEANUP;
  if (lifecycle == HEADSET_CLEANUP && !cleanup()) {
    lovrLog(LOG_ERROR, "XR", "Headset destruction deferred: %s", lovrGetError());
  }
  atomic_flag_clear(&lifecycleLock);
}

static void appendDiagnostic(char* errors, size_t size, const char* backend, const char* reason) {
  size_t length = strlen(errors);
  int written = snprintf(errors + length, size - length, "%s%s: %s", length ? "; " : "", backend, reason);
  if (written < 0 || (size_t) written >= size - length) {
    const char marker[] = " [diagnostics truncated]";
    memcpy(errors + size - sizeof(marker), marker, sizeof(marker));
  }
}

static bool tryXR(OpenXRConnectMode mode, char* errors, size_t size) {
  ops = &lovrHeadsetOpenXROps;
  OpenXRConnectResult result = lovrOpenXRConnect(mode);
  if (result == OPENXR_CONNECT_SELECTED) return true;
  pending = result == OPENXR_CONNECT_CLEANUP_PENDING;
  appendDiagnostic(errors, size, mode == OPENXR_CONNECT_REQUIRE_OVERLAY ? "openxr overlay" : "openxr ordinary", lovrGetError());
  return false;
}

static bool tryVR(char* errors, size_t size) {
#ifdef LOVR_ENABLE_OPENVR
  ops = &lovrHeadsetOpenVROps;
  if (!vrInitialized) {
    vrInitialized = true;
    if (!ops->HeadsetInit(&overlayConfig)) goto fail;
  }
  if (ops->HeadsetConnect()) return true;
fail:
  appendDiagnostic(errors, size, "openvr", lovrGetError());
  pending = !ops->HeadsetDisconnect();
#else
  appendDiagnostic(errors, size, "openvr", "backend is not built");
#endif
  return false;
}

bool lovrHeadsetConnect(void) {
  if (lifecycle != HEADSET_LIVE) return false;
  if (pending) {
    if (!ops->HeadsetDisconnect()) return false;
    pending = false;
  }
  if (frozen || ops->HeadsetIsConnected()) return ops->HeadsetIsConnected();
  if (!config.connect) return false;
  char errors[768] = { 0 };
  if (config.backend == HEADSET_BACKEND_OPENVR) {
    if (tryVR(errors, sizeof(errors))) return true;
  } else if (config.backend == HEADSET_BACKEND_AUTO && config.overlay) {
    if (tryXR(OPENXR_CONNECT_REQUIRE_OVERLAY, errors, sizeof(errors))) return true;
    if (!pending && tryVR(errors, sizeof(errors))) return true;
    if (!pending && tryXR(OPENXR_CONNECT_ORDINARY, errors, sizeof(errors))) return true;
  } else {
    if (tryXR(OPENXR_CONNECT_ORDINARY, errors, sizeof(errors))) return true;
  }
  if (!pending) ops = &lovrHeadsetOpenXROps;
  lovrSetError("Headset selection: %s", errors);
  lovrLog(LOG_WARN, "XR", "%s", lovrGetError());
  return false;
}

bool lovrHeadsetPrepareGraphics(void) {
  lovrAssert(!pending, "Headset cleanup is pending; graphics initialization is blocked");
  frozen = true;
  return true;
}

void lovrHeadsetGraphicsDestroyed(void) {
  frozen = false;
}

bool lovrHeadsetRequiresPhysicalDevice(void) {
#ifdef LOVR_ENABLE_OPENVR
  return ops == &lovrHeadsetOpenVROps && ops->HeadsetIsConnected();
#else
  return false;
#endif
}

void lovrHeadsetWillExit(void) {
  if (initialized && ops->WillExit) ops->WillExit();
}

bool lovrHeadsetBeforeGraphicsDestroy(void) {
  bool ok = ops->HeadsetDisconnect();
  pending = !ok;
  return ok;
}

bool lovrHeadsetIsConnected(void) {
  return ops->HeadsetIsConnected();
}

const char* lovrHeadsetGetName(void) {
  return ops->HeadsetGetName();
}

const char* lovrHeadsetGetDriver(void) {
  return ops->HeadsetGetDriver();
}

void lovrHeadsetGetFeatures(HeadsetFeatures* features) {
  ops->HeadsetGetFeatures(features);
}

uint32_t lovrHeadsetGetLayerLimit(void) {
  return ops->HeadsetGetLayerLimit();
}

bool lovrHeadsetIsSeated(void) {
  return ops->HeadsetIsSeated();
}

bool lovrHeadsetStart(void) {
  return ops->HeadsetStart();
}

void lovrHeadsetStop(void) {
  ops->HeadsetStop();
}

bool lovrHeadsetIsActive(void) {
  return ops->HeadsetIsActive();
}

uint32_t lovrHeadsetGetSessionGeneration(void) {
  return ops->HeadsetGetSessionGeneration();
}

bool lovrHeadsetIsMainSessionVisible(void) {
  return ops->HeadsetIsMainSessionVisible();
}

bool lovrHeadsetIsVisible(bool* main) {
  return ops->HeadsetIsVisible(main);
}

bool lovrHeadsetIsFocused(void) {
  return ops->HeadsetIsFocused();
}

bool lovrHeadsetIsMounted(void) {
  return ops->HeadsetIsMounted();
}

bool lovrHeadsetPollEvents(void) {
  return ops->HeadsetPollEvents();
}

bool lovrHeadsetUpdate(void) {
  return ops->HeadsetUpdate();
}

void lovrHeadsetGetDisplayDimensions(uint32_t* width, uint32_t* height) {
  ops->HeadsetGetDisplayDimensions(width, height);
}

float* lovrHeadsetGetRefreshRates(uint32_t* count) {
  return ops->HeadsetGetRefreshRates(count);
}

float lovrHeadsetGetRefreshRate(void) {
  return ops->HeadsetGetRefreshRate();
}

bool lovrHeadsetSetRefreshRate(float refreshRate) {
  return ops->HeadsetSetRefreshRate(refreshRate);
}

void lovrHeadsetGetFoveation(FoveationLevel* level, bool* dynamic) {
  ops->HeadsetGetFoveation(level, dynamic);
}

bool lovrHeadsetSetFoveation(FoveationLevel level, bool dynamic) {
  return ops->HeadsetSetFoveation(level, dynamic);
}

bool lovrHeadsetIsHDR(void) {
  return ops->HeadsetIsHDR();
}

bool lovrHeadsetIsPassthroughSupported(PassthroughMode mode) {
  return ops->HeadsetIsPassthroughSupported(mode);
}

PassthroughMode lovrHeadsetGetPassthrough(void) {
  return ops->HeadsetGetPassthrough();
}

bool lovrHeadsetSetPassthrough(PassthroughMode mode) {
  return ops->HeadsetSetPassthrough(mode);
}

uint32_t lovrHeadsetGetViewCount(void) {
  return ops->HeadsetGetViewCount();
}

bool lovrHeadsetGetViewPose(uint32_t view, float* position, float* orientation) {
  return ops->HeadsetGetViewPose(view, position, orientation);
}

bool lovrHeadsetGetViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  return ops->HeadsetGetViewAngles(view, left, right, up, down);
}

void lovrHeadsetGetClipDistance(float* clipNear, float* clipFar) {
  ops->HeadsetGetClipDistance(clipNear, clipFar);
}

void lovrHeadsetSetClipDistance(float clipNear, float clipFar) {
  ops->HeadsetSetClipDistance(clipNear, clipFar);
}

void lovrHeadsetGetBoundsDimensions(float* width, float* depth) {
  ops->HeadsetGetBoundsDimensions(width, depth);
}

bool lovrHeadsetGetHandPose(Side side, HandPose type, float* position, float* orientation) {
  return ops->HeadsetGetHandPose(side, type, position, orientation);
}

bool lovrHeadsetGetPose(Device device, float* position, float* orientation) {
  return ops->HeadsetGetPose(device, position, orientation);
}

bool lovrHeadsetGetVelocity(Device device, float* velocity, float* angularVelocity) {
  return ops->HeadsetGetVelocity(device, velocity, angularVelocity);
}

bool lovrHeadsetIsDown(Device device, DeviceButton button, bool* down, bool* changed) {
  return ops->HeadsetIsDown(device, button, down, changed);
}

bool lovrHeadsetIsTouched(Device device, DeviceButton button, bool* touched) {
  return ops->HeadsetIsTouched(device, button, touched);
}

bool lovrHeadsetGetAxis(Device device, DeviceAxis axis, float* value) {
  return ops->HeadsetGetAxis(device, axis, value);
}

bool lovrHeadsetGetSkeleton(Device device, float* poses, SkeletonSource* source) {
  return ops->HeadsetGetSkeleton(device, poses, source);
}

bool lovrHeadsetGetBattery(Device device, float* level, bool* charging) {
  return ops->HeadsetGetBattery(device, level, charging);
}

bool lovrHeadsetVibrate(Device device, float strength, float duration, float frequency) {
  return ops->HeadsetVibrate(device, strength, duration, frequency);
}

void lovrHeadsetStopVibration(Device device) {
  ops->HeadsetStopVibration(device);
}

uint64_t* lovrHeadsetGetModelKeys(uint32_t* count) {
  return ops->HeadsetGetModelKeys(count);
}

struct ModelData* lovrHeadsetNewModelData(uint64_t key) {
  return ops->HeadsetNewModelData(key);
}

bool lovrHeadsetGetModelPose(struct Model* model, float* position, float* orientation) {
  return ops->HeadsetGetModelPose(model, position, orientation);
}

bool lovrHeadsetAnimate(struct Model* model) {
  return ops->HeadsetAnimate(model);
}

struct Texture* lovrHeadsetSetBackground(uint32_t width, uint32_t height, uint32_t layers) {
  return ops->HeadsetSetBackground(width, height, layers);
}

Layer** lovrHeadsetGetLayers(uint32_t* count, bool* main) {
  return ops->HeadsetGetLayers(count, main);
}

bool lovrHeadsetSetLayers(Layer** layers, uint32_t count, bool main) {
  return ops->HeadsetSetLayers(layers, count, main);
}

bool lovrHeadsetGetTexture(struct Texture** texture) {
  return ops->HeadsetGetTexture(texture);
}

bool lovrHeadsetGetPass(struct Pass** pass) {
  return ops->HeadsetGetPass(pass);
}

bool lovrHeadsetSubmit(void) {
  return ops->HeadsetSubmit();
}

void lovrHeadsetSetPose(Device device, float* position, float* orientation) {
  ops->HeadsetSetPose(device, position, orientation);
}

void lovrHeadsetSetButton(Device device, DeviceButton button, bool down) {
  ops->HeadsetSetButton(device, button, down);
}

Layer* lovrLayerCreate(const LayerInfo* info) {
  return ops->LayerCreate(info);
}

void lovrLayerDestroy(void* ref) {
  lovrLayerGetCreator(ref)->LayerDestroy(ref);
}

Device lovrLayerGetOrigin(Layer* layer) {
  return lovrLayerGetCreator(layer)->LayerGetOrigin(layer);
}

void lovrLayerSetOrigin(Layer* layer, Device device) {
  lovrLayerGetCreator(layer)->LayerSetOrigin(layer, device);
}

void lovrLayerGetPose(Layer* layer, float* position, float* orientation) {
  lovrLayerGetCreator(layer)->LayerGetPose(layer, position, orientation);
}

void lovrLayerSetPose(Layer* layer, float* position, float* orientation) {
  lovrLayerGetCreator(layer)->LayerSetPose(layer, position, orientation);
}

void lovrLayerGetDimensions(Layer* layer, float* width, float* height) {
  lovrLayerGetCreator(layer)->LayerGetDimensions(layer, width, height);
}

void lovrLayerSetDimensions(Layer* layer, float width, float height) {
  lovrLayerGetCreator(layer)->LayerSetDimensions(layer, width, height);
}

float lovrLayerGetCurve(Layer* layer) {
  return lovrLayerGetCreator(layer)->LayerGetCurve(layer);
}

bool lovrLayerSetCurve(Layer* layer, float curve) {
  return lovrLayerGetCreator(layer)->LayerSetCurve(layer, curve);
}

void lovrLayerGetColor(Layer* layer, float color[4]) {
  lovrLayerGetCreator(layer)->LayerGetColor(layer, color);
}

void lovrLayerSetColor(Layer* layer, float color[4]) {
  lovrLayerGetCreator(layer)->LayerSetColor(layer, color);
}

void lovrLayerGetViewport(Layer* layer, int32_t* viewport) {
  lovrLayerGetCreator(layer)->LayerGetViewport(layer, viewport);
}

void lovrLayerSetViewport(Layer* layer, int32_t* viewport) {
  lovrLayerGetCreator(layer)->LayerSetViewport(layer, viewport);
}

struct Texture* lovrLayerGetTexture(Layer* layer) {
  return lovrLayerGetCreator(layer)->LayerGetTexture(layer);
}

struct Pass* lovrLayerGetPass(Layer* layer) {
  return lovrLayerGetCreator(layer)->LayerGetPass(layer);
}

void lovrHeadsetGetVulkanPhysicalDevice(void* instance, uintptr_t physicalDevice) {
  ops->HeadsetGetVulkanPhysicalDevice(instance, physicalDevice);
}

uint32_t lovrHeadsetCreateVulkanInstance(void* instanceCreateInfo, void* allocator, uintptr_t instance, void* getInstanceProcAddr) {
  return ops->HeadsetCreateVulkanInstance(instanceCreateInfo, allocator, instance, getInstanceProcAddr);
}

uint32_t lovrHeadsetCreateVulkanDevice(void* instance, void* deviceCreateInfo, void* allocatoor, uintptr_t device, void* getInstanceProcAddr) {
  return ops->HeadsetCreateVulkanDevice(instance, deviceCreateInfo, allocatoor, device, getInstanceProcAddr);
}

double lovrHeadsetGetDisplayTime(void) {
  return ops->HeadsetGetDisplayTime();
}

double lovrHeadsetGetDisplayPeriod(void) {
  return ops->HeadsetGetDisplayPeriod();
}

double lovrHeadsetGetDeltaTime(void) {
  return ops->HeadsetGetDeltaTime();
}

bool lovrHeadsetGetDepthTexture(struct Texture** texture) {
  return ops->HeadsetGetDepthTexture(texture);
}
