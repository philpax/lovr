#include "headset/headset_ops.h"
#include "headset/headset_openxr.h"
#include "headset/headset_layer.h"
#include "test.h"
#include "util.h"
#include <stdlib.h>

struct Layer { LayerHeader header; int token; };

static struct {
  HeadsetConfig* config;
  HeadsetConfig snapshot;
  bool result;
  OpenXRConnectResult connectResult;
  OpenXRConnectMode connectMode;
  bool connected;
  bool active;
  char calls[32];
  size_t callCount;
  HeadsetFeatures* features;
  const LayerInfo* info;
  Layer* destroyed;
  Layer** layers;
  uint32_t layerCount;
  bool main;
  unsigned submits;
  Layer* curved;
  float curve;
  Device batteryDevice;
  float* batteryLevel;
  bool* batteryCharging;
  uint64_t modelKey;
  uint32_t* refreshCount;
  void* vkInstance;
  void* vkInfo;
  void* vkAllocator;
  void* vkProc;
  uintptr_t vkOutput;
} fake;

static Layer layer = { .header = { .creator = &lovrHeadsetOpenXROps } };

static void record(char call) {
  if (fake.callCount >= sizeof(fake.calls)) abort();
  fake.calls[fake.callCount++] = call;
}

static bool fakeHeadsetInit(HeadsetConfig* config) {
  record('i');
  fake.config = config;
  fake.snapshot = *config;
  return fake.result;
}

static void fakeHeadsetDestroy(void) {
  record('d');
  fake.connected = false;
  fake.active = false;
}

static bool fakeHeadsetDisconnect(void) {
  record('x');
  fake.connected = false;
  fake.active = false;
  return true;
}

OpenXRConnectResult lovrOpenXRConnect(OpenXRConnectMode mode) {
  record('c');
  fake.connectMode = mode;
  fake.connected = fake.connectResult == OPENXR_CONNECT_SELECTED;
  return fake.connectResult;
}

static bool fakeHeadsetConnect(void) {
  return lovrOpenXRConnect(OPENXR_CONNECT_ORDINARY) == OPENXR_CONNECT_SELECTED;
}

static bool fakeHeadsetIsConnected(void) { return fake.connected; }

static bool fakeHeadsetStart(void) {
  record('a');
  fake.active = fake.result;
  return fake.result;
}

static void fakeHeadsetStop(void) {
  record('s');
  fake.active = false;
}

static bool fakeHeadsetIsActive(void) { return fake.active; }

static void fakeHeadsetGetFeatures(HeadsetFeatures* features) {
  fake.features = features;
  *features = (HeadsetFeatures) { .overlay = true, .layerCurve = true, .depthSubmission = true };
}

static uint32_t fakeHeadsetGetLayerLimit(void) { return 7; }

static Layer* fakeLayerCreate(const LayerInfo* info) {
  fake.info = info;
  return &layer;
}

static void fakeLayerDestroy(void* ref) { fake.destroyed = ref; }

static bool fakeHeadsetSetLayers(Layer** layers, uint32_t count, bool main) {
  fake.layers = layers;
  fake.layerCount = count;
  fake.main = main;
  return fake.result;
}

static Layer** fakeHeadsetGetLayers(uint32_t* count, bool* main) {
  *count = fake.layerCount;
  *main = fake.main;
  return fake.layers;
}

static bool fakeHeadsetSubmit(void) {
  fake.submits++;
  return fake.result;
}

static bool fakeLayerSetCurve(Layer* target, float curve) {
  fake.curved = target;
  fake.curve = curve;
  return false;
}

static bool fakeHeadsetGetBattery(Device device, float* level, bool* charging) {
  fake.batteryDevice = device;
  fake.batteryLevel = level;
  fake.batteryCharging = charging;
  return false;
}

static struct ModelData* fakeHeadsetNewModelData(uint64_t key) {
  fake.modelKey = key;
  return NULL;
}

static float* fakeHeadsetGetRefreshRates(uint32_t* count) {
  fake.refreshCount = count;
  *count = 0;
  return NULL;
}

static void fakeHeadsetGetVulkanPhysicalDevice(void* instance, uintptr_t physicalDevice) {
  fake.vkInstance = instance;
  fake.vkOutput = physicalDevice;
  *(uintptr_t*) physicalDevice = 0x1234;
}

static uint32_t fakeHeadsetCreateVulkanInstance(void* info, void* allocator, uintptr_t instance, void* proc) {
  fake.vkInfo = info;
  fake.vkAllocator = allocator;
  fake.vkOutput = instance;
  fake.vkProc = proc;
  *(uintptr_t*) instance = 0x5678;
  return 19;
}

static uint32_t fakeHeadsetCreateVulkanDevice(void* instance, void* info, void* allocator, uintptr_t device, void* proc) {
  fake.vkInstance = instance;
  fake.vkInfo = info;
  fake.vkAllocator = allocator;
  fake.vkOutput = device;
  fake.vkProc = proc;
  *(uintptr_t*) device = 0x9abc;
  return 23;
}

#define UNUSED_OP(type, name, args, unused) static type fake##name args { unused abort(); }

UNUSED_OP(const char*, HeadsetGetName, (void), )
UNUSED_OP(const char*, HeadsetGetDriver, (void), )
UNUSED_OP(bool, HeadsetIsSeated, (void), )
UNUSED_OP(uint32_t, HeadsetGetSessionGeneration, (void), )
UNUSED_OP(bool, HeadsetIsMainSessionVisible, (void), )
UNUSED_OP(bool, HeadsetIsVisible, (bool* main), (void) main;)
UNUSED_OP(bool, HeadsetIsFocused, (void), )
UNUSED_OP(bool, HeadsetIsMounted, (void), )
UNUSED_OP(bool, HeadsetPollEvents, (void), )
UNUSED_OP(bool, HeadsetUpdate, (void), )
UNUSED_OP(void, HeadsetGetDisplayDimensions, (uint32_t* width, uint32_t* height), (void) width; (void) height;)
UNUSED_OP(float, HeadsetGetRefreshRate, (void), )
UNUSED_OP(bool, HeadsetSetRefreshRate, (float refreshRate), (void) refreshRate;)
UNUSED_OP(void, HeadsetGetFoveation, (FoveationLevel* level, bool* dynamic), (void) level; (void) dynamic;)
UNUSED_OP(bool, HeadsetSetFoveation, (FoveationLevel level, bool dynamic), (void) level; (void) dynamic;)
UNUSED_OP(bool, HeadsetIsHDR, (void), )
UNUSED_OP(bool, HeadsetGetHandPose, (Side side, HandPose type, float* position, float* orientation), (void) side; (void) type; (void) position; (void) orientation;)
UNUSED_OP(bool, HeadsetIsPassthroughSupported, (PassthroughMode mode), (void) mode;)
UNUSED_OP(PassthroughMode, HeadsetGetPassthrough, (void), )
UNUSED_OP(bool, HeadsetSetPassthrough, (PassthroughMode mode), (void) mode;)
UNUSED_OP(uint32_t, HeadsetGetViewCount, (void), )
UNUSED_OP(bool, HeadsetGetViewPose, (uint32_t view, float* position, float* orientation), (void) view; (void) position; (void) orientation;)
UNUSED_OP(bool, HeadsetGetViewAngles, (uint32_t view, float* left, float* right, float* up, float* down), (void) view; (void) left; (void) right; (void) up; (void) down;)
UNUSED_OP(void, HeadsetGetClipDistance, (float* clipNear, float* clipFar), (void) clipNear; (void) clipFar;)
UNUSED_OP(void, HeadsetSetClipDistance, (float clipNear, float clipFar), (void) clipNear; (void) clipFar;)
UNUSED_OP(void, HeadsetGetBoundsDimensions, (float* width, float* depth), (void) width; (void) depth;)
UNUSED_OP(bool, HeadsetGetPose, (Device device, float* position, float* orientation), (void) device; (void) position; (void) orientation;)
UNUSED_OP(bool, HeadsetGetVelocity, (Device device, float* velocity, float* angularVelocity), (void) device; (void) velocity; (void) angularVelocity;)
UNUSED_OP(bool, HeadsetIsDown, (Device device, DeviceButton button, bool* down, bool* changed), (void) device; (void) button; (void) down; (void) changed;)
UNUSED_OP(bool, HeadsetIsTouched, (Device device, DeviceButton button, bool* touched), (void) device; (void) button; (void) touched;)
UNUSED_OP(bool, HeadsetGetAxis, (Device device, DeviceAxis axis, float* value), (void) device; (void) axis; (void) value;)
UNUSED_OP(bool, HeadsetGetSkeleton, (Device device, float* poses, SkeletonSource* source), (void) device; (void) poses; (void) source;)
UNUSED_OP(bool, HeadsetVibrate, (Device device, float strength, float duration, float frequency), (void) device; (void) strength; (void) duration; (void) frequency;)
UNUSED_OP(void, HeadsetStopVibration, (Device device), (void) device;)
UNUSED_OP(uint64_t*, HeadsetGetModelKeys, (uint32_t* count), (void) count;)
UNUSED_OP(bool, HeadsetGetModelPose, (struct Model* model, float* position, float* orientation), (void) model; (void) position; (void) orientation;)
UNUSED_OP(bool, HeadsetAnimate, (struct Model* model), (void) model;)
UNUSED_OP(struct Texture*, HeadsetSetBackground, (uint32_t width, uint32_t height, uint32_t layers), (void) width; (void) height; (void) layers;)
UNUSED_OP(bool, HeadsetGetTexture, (struct Texture** texture), (void) texture;)
UNUSED_OP(bool, HeadsetGetPass, (struct Pass** pass), (void) pass;)
UNUSED_OP(void, HeadsetSetPose, (Device device, float* position, float* orientation), (void) device; (void) position; (void) orientation;)
UNUSED_OP(void, HeadsetSetButton, (Device device, DeviceButton button, bool down), (void) device; (void) button; (void) down;)
UNUSED_OP(Device, LayerGetOrigin, (Layer* layer), (void) layer;)
UNUSED_OP(void, LayerSetOrigin, (Layer* layer, Device device), (void) layer; (void) device;)
UNUSED_OP(void, LayerGetPose, (Layer* layer, float* position, float* orientation), (void) layer; (void) position; (void) orientation;)
UNUSED_OP(void, LayerSetPose, (Layer* layer, float* position, float* orientation), (void) layer; (void) position; (void) orientation;)
UNUSED_OP(void, LayerGetDimensions, (Layer* layer, float* width, float* height), (void) layer; (void) width; (void) height;)
UNUSED_OP(void, LayerSetDimensions, (Layer* layer, float width, float height), (void) layer; (void) width; (void) height;)
UNUSED_OP(float, LayerGetCurve, (Layer* layer), (void) layer;)
UNUSED_OP(void, LayerGetColor, (Layer* layer, float color[4]), (void) layer; (void) color;)
UNUSED_OP(void, LayerSetColor, (Layer* layer, float color[4]), (void) layer; (void) color;)
UNUSED_OP(void, LayerGetViewport, (Layer* layer, int32_t* viewport), (void) layer; (void) viewport;)
UNUSED_OP(void, LayerSetViewport, (Layer* layer, int32_t* viewport), (void) layer; (void) viewport;)
UNUSED_OP(struct Texture*, LayerGetTexture, (Layer* layer), (void) layer;)
UNUSED_OP(struct Pass*, LayerGetPass, (Layer* layer), (void) layer;)
UNUSED_OP(double, HeadsetGetDisplayTime, (void), )
UNUSED_OP(double, HeadsetGetDisplayPeriod, (void), )
UNUSED_OP(double, HeadsetGetDeltaTime, (void), )
UNUSED_OP(bool, HeadsetGetDepthTexture, (struct Texture** texture), (void) texture;)

const HeadsetOps lovrHeadsetOpenXROps = {
  .HeadsetInit = fakeHeadsetInit,
  .HeadsetDestroy = fakeHeadsetDestroy,
  .HeadsetConnect = fakeHeadsetConnect,
  .HeadsetDisconnect = fakeHeadsetDisconnect,
  .HeadsetIsConnected = fakeHeadsetIsConnected,
  .HeadsetGetName = fakeHeadsetGetName,
  .HeadsetGetDriver = fakeHeadsetGetDriver,
  .HeadsetGetFeatures = fakeHeadsetGetFeatures,
  .HeadsetGetLayerLimit = fakeHeadsetGetLayerLimit,
  .HeadsetIsSeated = fakeHeadsetIsSeated,
  .HeadsetStart = fakeHeadsetStart,
  .HeadsetStop = fakeHeadsetStop,
  .HeadsetIsActive = fakeHeadsetIsActive,
  .HeadsetGetSessionGeneration = fakeHeadsetGetSessionGeneration,
  .HeadsetIsMainSessionVisible = fakeHeadsetIsMainSessionVisible,
  .HeadsetIsVisible = fakeHeadsetIsVisible,
  .HeadsetIsFocused = fakeHeadsetIsFocused,
  .HeadsetIsMounted = fakeHeadsetIsMounted,
  .HeadsetPollEvents = fakeHeadsetPollEvents,
  .HeadsetUpdate = fakeHeadsetUpdate,
  .HeadsetGetDisplayDimensions = fakeHeadsetGetDisplayDimensions,
  .HeadsetGetRefreshRates = fakeHeadsetGetRefreshRates,
  .HeadsetGetRefreshRate = fakeHeadsetGetRefreshRate,
  .HeadsetSetRefreshRate = fakeHeadsetSetRefreshRate,
  .HeadsetGetFoveation = fakeHeadsetGetFoveation,
  .HeadsetSetFoveation = fakeHeadsetSetFoveation,
  .HeadsetIsHDR = fakeHeadsetIsHDR,
  .HeadsetGetHandPose = fakeHeadsetGetHandPose,
  .HeadsetIsPassthroughSupported = fakeHeadsetIsPassthroughSupported,
  .HeadsetGetPassthrough = fakeHeadsetGetPassthrough,
  .HeadsetSetPassthrough = fakeHeadsetSetPassthrough,
  .HeadsetGetViewCount = fakeHeadsetGetViewCount,
  .HeadsetGetViewPose = fakeHeadsetGetViewPose,
  .HeadsetGetViewAngles = fakeHeadsetGetViewAngles,
  .HeadsetGetClipDistance = fakeHeadsetGetClipDistance,
  .HeadsetSetClipDistance = fakeHeadsetSetClipDistance,
  .HeadsetGetBoundsDimensions = fakeHeadsetGetBoundsDimensions,
  .HeadsetGetPose = fakeHeadsetGetPose,
  .HeadsetGetVelocity = fakeHeadsetGetVelocity,
  .HeadsetIsDown = fakeHeadsetIsDown,
  .HeadsetIsTouched = fakeHeadsetIsTouched,
  .HeadsetGetAxis = fakeHeadsetGetAxis,
  .HeadsetGetSkeleton = fakeHeadsetGetSkeleton,
  .HeadsetGetBattery = fakeHeadsetGetBattery,
  .HeadsetVibrate = fakeHeadsetVibrate,
  .HeadsetStopVibration = fakeHeadsetStopVibration,
  .HeadsetGetModelKeys = fakeHeadsetGetModelKeys,
  .HeadsetNewModelData = fakeHeadsetNewModelData,
  .HeadsetGetModelPose = fakeHeadsetGetModelPose,
  .HeadsetAnimate = fakeHeadsetAnimate,
  .HeadsetSetBackground = fakeHeadsetSetBackground,
  .HeadsetGetLayers = fakeHeadsetGetLayers,
  .HeadsetSetLayers = fakeHeadsetSetLayers,
  .HeadsetGetTexture = fakeHeadsetGetTexture,
  .HeadsetGetPass = fakeHeadsetGetPass,
  .HeadsetSubmit = fakeHeadsetSubmit,
  .HeadsetSetPose = fakeHeadsetSetPose,
  .HeadsetSetButton = fakeHeadsetSetButton,
  .LayerCreate = fakeLayerCreate,
  .LayerDestroy = fakeLayerDestroy,
  .LayerGetOrigin = fakeLayerGetOrigin,
  .LayerSetOrigin = fakeLayerSetOrigin,
  .LayerGetPose = fakeLayerGetPose,
  .LayerSetPose = fakeLayerSetPose,
  .LayerGetDimensions = fakeLayerGetDimensions,
  .LayerSetDimensions = fakeLayerSetDimensions,
  .LayerGetCurve = fakeLayerGetCurve,
  .LayerSetCurve = fakeLayerSetCurve,
  .LayerGetColor = fakeLayerGetColor,
  .LayerSetColor = fakeLayerSetColor,
  .LayerGetViewport = fakeLayerGetViewport,
  .LayerSetViewport = fakeLayerSetViewport,
  .LayerGetTexture = fakeLayerGetTexture,
  .LayerGetPass = fakeLayerGetPass,
  .HeadsetGetVulkanPhysicalDevice = fakeHeadsetGetVulkanPhysicalDevice,
  .HeadsetCreateVulkanInstance = fakeHeadsetCreateVulkanInstance,
  .HeadsetCreateVulkanDevice = fakeHeadsetCreateVulkanDevice,
  .HeadsetGetDisplayTime = fakeHeadsetGetDisplayTime,
  .HeadsetGetDisplayPeriod = fakeHeadsetGetDisplayPeriod,
  .HeadsetGetDeltaTime = fakeHeadsetGetDeltaTime,
  .HeadsetGetDepthTexture = fakeHeadsetGetDepthTexture,
};

static bool lifecycle(void) {
  lovrHeadsetWillExit();
  CHECK(lovrHeadsetOpenXROps.WillExit == NULL);
  memset(&fake, 0, sizeof(fake));
  HeadsetConfig config = {
    .supersample = 1.25f, .dynamicResolution = true, .debug = true, .seated = true,
    .mask = true, .stencil = true, .antialias = true, .submitDepth = true,
    .overlay = true, .overlayOrder = 42, .controllerSkeleton = SKELETON_NATURAL,
    .connect = true, .backend = HEADSET_BACKEND_OPENXR, .extensionCount = 1
  };
  config.extensions = lovrMalloc(sizeof("XR_test_extension"));
  strcpy(config.extensions, "XR_test_extension");
  HeadsetConfig original = config;
  CHECK(!lovrHeadsetIsConnected());
  CHECK(!lovrHeadsetInit(&config));
  CHECK(fake.config != &config);
  CHECK(!memcmp(&fake.snapshot, &original, sizeof(config)));
  lovrHeadsetDestroy();
  CHECK(fake.callCount == 3 && !memcmp(fake.calls, "ixd", 3));

  memset(&fake, 0, sizeof(fake));
  config.extensions = lovrMalloc(sizeof("XR_test_extension"));
  strcpy(config.extensions, "XR_test_extension");
  original = config;
  fake.result = true;
  fake.connectResult = OPENXR_CONNECT_UNAVAILABLE_CLEANED;
  CHECK(lovrHeadsetInit(&config));
  lovrHeadsetWillExit();
  CHECK(fake.callCount == 1);
  CHECK(fake.config != &config);
  CHECK(!memcmp(fake.config, &original, sizeof(config)));
  HeadsetConfig* canonical = fake.config;
  config.supersample = 2.f;
  config.connect = false;
  HeadsetConfig repeated = { .extensions = lovrMalloc(sizeof("XR_other_extension")), .extensionCount = 1 };
  strcpy(repeated.extensions, "XR_other_extension");
  CHECK(lovrHeadsetInit(&repeated));
  CHECK(fake.callCount == 1 && fake.config == canonical);
  CHECK(!memcmp(canonical, &original, sizeof(original)));
  CHECK(!lovrHeadsetConnect());
  CHECK(fake.connectMode == OPENXR_CONNECT_ORDINARY);
  CHECK(!lovrHeadsetIsConnected());
  fake.connectResult = OPENXR_CONNECT_CLEANUP_PENDING;
  CHECK(!lovrHeadsetConnect());
  CHECK(!lovrHeadsetIsConnected());
  fake.connectResult = OPENXR_CONNECT_SELECTED;
  CHECK(lovrHeadsetConnect());
  CHECK(lovrHeadsetIsConnected());
  CHECK(lovrHeadsetStart());
  CHECK(lovrHeadsetIsActive());
  lovrHeadsetStop();
  CHECK(!lovrHeadsetIsActive());
  CHECK(lovrHeadsetIsConnected());
  lovrHeadsetDestroy();
  CHECK(lovrHeadsetIsConnected());
  CHECK(fake.callCount == 7 && !memcmp(fake.calls, "iccxcas", 7));
  CHECK(!strcmp(canonical->extensions, "XR_test_extension"));
  lovrHeadsetDestroy();
  lovrHeadsetWillExit();
  CHECK(!lovrHeadsetIsConnected() && !lovrHeadsetIsActive());
  CHECK(fake.callCount == 9 && !memcmp(fake.calls, "iccxcasxd", 9));
  lovrHeadsetDestroy();
  CHECK(fake.callCount == 9);
  fake.connected = true;
  fake.active = true;
  CHECK(lovrHeadsetBeforeGraphicsDestroy());
  CHECK(!fake.connected && !fake.active);
  CHECK(fake.callCount == 10 && fake.calls[9] == 'x');
  return true;
}

static bool featuresAndLayers(void) {
  memset(&fake, 0, sizeof(fake));
  HeadsetFeatures features = { 0 };
  lovrHeadsetGetFeatures(&features);
  CHECK(fake.features == &features);
  CHECK(features.overlay && features.layerCurve && features.depthSubmission);
  CHECK(!features.battery && !features.passthrough && !features.handTracking);
  CHECK(lovrHeadsetGetLayerLimit() == 7);
  LayerInfo info = { .width = 640, .height = 480, .stereo = true, .transparent = true };
  CHECK(lovrLayerCreate(&info) == &layer);
  CHECK(fake.info == &info);
  Layer* layers[] = { &layer };
  fake.result = true;
  CHECK(lovrHeadsetSetLayers(layers, 1, false));
  CHECK(fake.layers == layers && fake.layerCount == 1 && !fake.main);
  uint32_t count = 99;
  bool main = true;
  CHECK(lovrHeadsetGetLayers(&count, &main) == layers);
  CHECK(count == 1 && !main);
  CHECK(lovrHeadsetSubmit());
  fake.result = false;
  CHECK(!lovrHeadsetSetLayers(NULL, 0, true));
  CHECK(fake.layers == NULL && fake.layerCount == 0 && fake.main);
  CHECK(!lovrHeadsetSubmit());
  CHECK(fake.submits == 2);
  lovrLayerDestroy(&layer);
  CHECK(fake.destroyed == &layer);
  return true;
}

static bool falseAndNullForwarding(void) {
  memset(&fake, 0, sizeof(fake));
  float level = 0.75f;
  bool charging = true;
  CHECK(!lovrHeadsetGetBattery(DEVICE_HAND_RIGHT, &level, &charging));
  CHECK(fake.batteryDevice == DEVICE_HAND_RIGHT);
  CHECK(fake.batteryLevel == &level && fake.batteryCharging == &charging);
  CHECK(level == 0.75f && charging);
  CHECK(!lovrLayerSetCurve(&layer, 0.5f));
  CHECK(fake.curved == &layer && fake.curve == 0.5f);
  CHECK(lovrHeadsetNewModelData(UINT64_C(0x123456789abcdef0)) == NULL);
  CHECK(fake.modelKey == UINT64_C(0x123456789abcdef0));
  uint32_t count = 99;
  CHECK(lovrHeadsetGetRefreshRates(&count) == NULL);
  CHECK(fake.refreshCount == &count && count == 0);
  return true;
}

static bool creatorAndGeneration(void) {
  memset(&fake, 0, sizeof(fake));
  uint32_t first = lovrHeadsetNextSessionGeneration();
  uint32_t second = lovrHeadsetNextSessionGeneration();
  CHECK(first != 0 && second > first);
  CHECK(!lovrLayerIsValid(NULL));
  layer.header.generation = 0;
  CHECK(!lovrLayerIsValid(&layer));
  HeadsetOps creator = lovrHeadsetOpenXROps;
  layer.header.creator = &creator;
  CHECK(!lovrLayerIsValid(&layer));
  lovrLayerDestroy(&layer);
  CHECK(fake.destroyed == &layer);
  layer.header.creator = &lovrHeadsetOpenXROps;
  return true;
}

static bool vulkanForwarding(void) {
  memset(&fake, 0, sizeof(fake));
  int instance, info, allocator, proc;
  uintptr_t physical = 0, created = 0, device = 0;
  lovrHeadsetGetVulkanPhysicalDevice(&instance, (uintptr_t) &physical);
  CHECK(fake.vkInstance == &instance && fake.vkOutput == (uintptr_t) &physical);
  CHECK(physical == 0x1234);
  CHECK(lovrHeadsetCreateVulkanInstance(&info, &allocator, (uintptr_t) &created, &proc) == 19);
  CHECK(fake.vkInfo == &info && fake.vkAllocator == &allocator && fake.vkProc == &proc);
  CHECK(fake.vkOutput == (uintptr_t) &created && created == 0x5678);
  CHECK(lovrHeadsetCreateVulkanDevice(&instance, &info, &allocator, (uintptr_t) &device, &proc) == 23);
  CHECK(fake.vkInstance == &instance && fake.vkInfo == &info);
  CHECK(fake.vkAllocator == &allocator && fake.vkProc == &proc);
  CHECK(fake.vkOutput == (uintptr_t) &device && device == 0x9abc);
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "dispatch.lifecycle", lifecycle },
    { "dispatch.features-and-layers", featuresAndLayers },
    { "dispatch.false-and-null-forwarding", falseAndNullForwarding },
    { "dispatch.vulkan-forwarding", vulkanForwarding },
    { "dispatch.creator-and-generation", creatorAndGeneration }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
