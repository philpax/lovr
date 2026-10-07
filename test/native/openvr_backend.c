#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define canonicalize libcCanonicalize
#include <math.h>
#undef canonicalize
#include "test.h"
#include "headset/openvr_assets.h"
static OpenVRAssetsResult backendAssetsResolve(bool shared, const OpenVRAssetsProvider* provider);
#ifndef LOVR_OPENVR_GRAPHICS_HARNESS
#define lovrOpenVRAssetsResolve backendAssetsResolve
#include "../../src/modules/headset/headset_openvr.c"
#undef lovrOpenVRAssetsResolve
#endif
#define getPose inputGetPose
#include "../../src/modules/headset/openvr_input.c"
#undef getPose
#define getHand hapticsGetHand
#include "../../src/modules/headset/openvr_haptics.c"
#undef getHand
#define reduce eventsReduce
#include "../../src/modules/headset/openvr_events.c"
#undef reduce
#define validate assetsValidate
#include "../../src/modules/headset/openvr_assets.c"
#undef validate
static Event backendEvents[32];
static unsigned backendEventCount;
void lovrEventPush(Event event) {
  if (backendEventCount >= 32) abort();
  backendEvents[backendEventCount++] = event;
}

static bool failAssets, failManifest, failActions, failHaptic, activeInputPose;
static unsigned manifests, actionUpdates, quitAcks, digitalReads, analogReads, poseReads;
static bool inputActive, inputDown, inputChanged;
static void OPENVR_FNTABLE_CALLTYPE backendAck(void) { quitAcks++; }
static OpenVRAssetsStatus backendArtifact(void* context, bool shared, char* path, size_t capacity, int* error) {
  (void) context; (void) shared; (void) capacity;
  if (failAssets) { *error = 13; return OPENVR_ASSETS_ACCESS_DENIED; }
  strcpy(path, "/managed/lovr");
  return OPENVR_ASSETS_OK_PATHS_NOT_PINNED;
}
static OpenVRAssetsStatus backendCanonical(void* context, const char* path, char* output, size_t capacity, int* error) {
  (void) context; (void) capacity; (void) error; strcpy(output, path); return OPENVR_ASSETS_OK_PATHS_NOT_PINNED;
}
static OpenVRAssetsStatus backendInspect(void* context, const char* path, OpenVRAssetMetadata* metadata, int* error) {
  (void) context; (void) error;
  *metadata = (OpenVRAssetMetadata) { .kind = strstr(path, ".json") ? OPENVR_ASSET_REGULAR : OPENVR_ASSET_DIRECTORY,
    .permissions = 0755, .trustedOwner = true, .readable = true };
  return OPENVR_ASSETS_OK_PATHS_NOT_PINNED;
}
static OpenVRAssetsResult backendAssetsResolve(bool shared, const OpenVRAssetsProvider* provider) {
  (void) provider;
  OpenVRAssetsProvider fake = { NULL, backendArtifact, backendCanonical, backendInspect };
  return lovrOpenVRAssetsResolve(shared, &fake);
}
static EVRInputError OPENVR_FNTABLE_CALLTYPE backendManifest(char* path) {
  if (strcmp(path, "/managed/lovr-openvr/actions.json")) abort();
  manifests++;
  return failManifest ? EVRInputError_VRInputError_InvalidParam : EVRInputError_VRInputError_None;
}
static EVRInputError OPENVR_FNTABLE_CALLTYPE backendSet(char* path, VRActionSetHandle_t* handle) {
  (void) path; *handle = 1; return 0;
}
static EVRInputError OPENVR_FNTABLE_CALLTYPE backendAction(char* path, VRActionHandle_t* handle) {
  (void) path; *handle = 1; return 0;
}
static EVRInputError OPENVR_FNTABLE_CALLTYPE backendSource(char* path, VRInputValueHandle_t* handle) {
  *handle = strstr(path, "left") ? 1 : 2; return 0;
}
static EVRInputError OPENVR_FNTABLE_CALLTYPE backendActions(VRActiveActionSet_t* sets, uint32_t size, uint32_t count) {
  (void) sets; (void) size; (void) count;
  if (!manifests) abort();
  actionUpdates++;
  return failActions ? EVRInputError_VRInputError_InvalidHandle : 0;
}
static EVRInputError OPENVR_FNTABLE_CALLTYPE backendDigital(VRActionHandle_t handle, InputDigitalActionData_t* data, uint32_t size, VRInputValueHandle_t source) {
  (void) handle; (void) size; (void) source;
  digitalReads++;
  *data = (InputDigitalActionData_t) { .bActive = inputActive, .bState = inputDown, .bChanged = inputChanged }; return 0;
}
static EVRInputError OPENVR_FNTABLE_CALLTYPE backendAnalog(VRActionHandle_t handle, InputAnalogActionData_t* data, uint32_t size, VRInputValueHandle_t source) {
  (void) handle; (void) size; (void) source;
  analogReads++;
  *data = (InputAnalogActionData_t) { .bActive = inputActive, .x = .25f, .y = -.75f }; return 0;
}
static EVRInputError OPENVR_FNTABLE_CALLTYPE backendInputPose(VRActionHandle_t handle, ETrackingUniverseOrigin origin, float prediction, InputPoseActionData_t* data, uint32_t size, VRInputValueHandle_t source) {
  (void) handle; (void) origin; (void) prediction; (void) size; (void) source;
  poseReads++;
  *data = (InputPoseActionData_t) { .bActive = activeInputPose,
    .pose = { .bDeviceIsConnected = activeInputPose, .bPoseIsValid = activeInputPose,
      .mDeviceToAbsoluteTracking = { .m = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } } } } };
  return 0;
}
static EVRInputError OPENVR_FNTABLE_CALLTYPE backendHaptic(VRActionHandle_t handle, float start, float duration, float frequency, float strength, VRInputValueHandle_t source) {
  (void) handle; (void) start; (void) duration; (void) frequency; (void) strength; (void) source;
  return failHaptic ? EVRInputError_VRInputError_InvalidHandle : 0;
}
static struct VR_IVRInput_FnTable backendInput = { .SetActionManifestPath = backendManifest,
  .GetActionSetHandle = backendSet, .GetActionHandle = backendAction, .GetInputSourceHandle = backendSource,
  .UpdateActionState = backendActions, .GetDigitalActionData = backendDigital, .GetAnalogActionData = backendAnalog,
  .GetPoseActionDataRelativeToNow = backendInputPose, .TriggerHapticVibrationAction = backendHaptic };
#include <stdlib.h>
#ifndef LOVR_OPENVR_GRAPHICS_HARNESS
#include "../../src/modules/headset/openvr_frame.c"

static OpenVRProjectionResult projectionStatus(void) {
  return (OpenVRProjectionResult) { .status = OPENVR_PROJECTION_OK };
}
OpenVRProjectionResult lovrOpenVRProjectionCreate(OpenVRProjection* projection,
    struct VR_IVROverlay_FnTable* api, const char* keys[2], const char* names[2], const OpenVRProjectionConfig* config) {
  (void) projection; (void) api; (void) keys; (void) names; (void) config; abort();
}
OpenVRProjectionResult lovrOpenVRProjectionConfigure(OpenVRProjection* projection, const OpenVRProjectionConfig* config) {
  (void) projection; (void) config; abort();
}
OpenVRProjectionResult lovrOpenVRProjectionShow(OpenVRProjection* projection) { (void) projection; abort(); }
OpenVRProjectionResult lovrOpenVRProjectionHide(OpenVRProjection* projection) { (void) projection; return projectionStatus(); }
OpenVRProjectionResult lovrOpenVRProjectionDestroy(OpenVRProjection* projection) { (void) projection; return projectionStatus(); }
bool lovrOpenVRProjectionHandoff(const gpu_external_image* image, void* data) { (void) image; (void) data; abort(); }
bool lovrOpenVRProjectionPanelOrder(uint32_t mainOrder, uint32_t panelIndex, uint32_t* order) {
  if (mainOrder == UINT32_MAX || panelIndex >= UINT32_MAX - mainOrder) return false;
  *order = mainOrder + 1 + panelIndex;
  return true;
}
double os_get_time(void) { return 1.; }
#endif

static unsigned shutdowns, nextGeneration;
#ifndef LOVR_OPENVR_VALIDATE_CONFIG
static bool failDestroy, failProperty;
static unsigned destroys, hides;
#endif
#ifndef LOVR_OPENVR_GRAPHICS_HARNESS
static bool failWait;
static unsigned waits, copies, blits, handoffs, textureSubmits;
static bool failHandoff;
static uint32_t copiedEye[2], copiedX[2];
#endif

#ifndef LOVR_OPENVR_GRAPHICS_HARNESS
bool lovrGraphicsQuiesceSessionResources(void) { waits++; return !failWait; }
bool lovrGraphicsDrainSessionResources(void) { return true; }
void lovrGraphicsInvalidateSessionPass(Pass* pass) { if (pass) abort(); }
void lovrGraphicsInvalidateSessionTexture(Texture* texture) { if (texture) abort(); }
bool lovrGraphicsRegisterSessionPass(Pass* pass) { (void) pass; abort(); }
bool lovrGraphicsRegisterSessionTexture(Texture* texture) { (void) texture; abort(); }
Texture* lovrTextureCreate(const TextureInfo* info) { (void) info; abort(); }
const TextureInfo* lovrTextureGetInfo(Texture* texture) { (void) texture; abort(); }
void lovrTextureDestroy(void* ref) { (void) ref; abort(); }
Pass* lovrPassCreate(const char* label) { (void) label; abort(); }
void lovrPassDestroy(void* ref) { (void) ref; abort(); }
bool lovrPassSetClear(Pass* pass, LoadAction loads[4], float clear[4][4], LoadAction depth, float value) {
  (void) pass; (void) loads; (void) clear; (void) depth; (void) value; abort();
}
bool lovrPassSetCanvas(Pass* pass, Canvas* canvas) { (void) pass; (void) canvas; abort(); }
bool lovrPassSetViewMatrix(Pass* pass, uint32_t index, float matrix[16]) { (void) pass; (void) index; (void) matrix; abort(); }
bool lovrPassSetProjection(Pass* pass, uint32_t index, float matrix[16]) { (void) pass; (void) index; (void) matrix; abort(); }
bool lovrTextureCopy(Texture* src, Texture* dst, uint32_t a[4], uint32_t b[4], uint32_t extent[3]) {
  (void) src; (void) dst;
  if (copies >= 2 || extent[0] != 64 || extent[1] != 32 || extent[2] != 1) abort();
  copiedEye[copies] = a[2]; copiedX[copies++] = b[0];
  return true;
}
bool lovrTextureBlit(Texture* src, Texture* dst, uint32_t a[4], uint32_t b[4], uint32_t source[3], uint32_t target[3], FilterMode filter) {
  (void) src; (void) dst;
  if (source[0] != 32 || source[1] != 16 || target[0] != 64 || target[1] != 32 || a[0] != 16 || a[1] != 8 || b[0] != a[2] * 64 || filter != FILTER_LINEAR) abort();
  blits++;
  return true;
}
bool lovrGraphicsHandoffTexture(Texture* texture, lovrGraphicsExternalCallback callback, void* data, uint32_t* completion) {
  (void) texture; (void) completion;
  handoffs++;
  if (failHandoff) return false;
  gpu_external_image image = { .image = 42, .width = 128, .height = 32, .samples = 1, .format = VK_FORMAT_R8G8B8A8_SRGB };
  return callback(&image, data);
}
#endif
uint32_t lovrHeadsetNextSessionGeneration(void) { return ++nextGeneration; }
EVRInitError lovrOpenVRConnect(OpenVRRuntime* runtime, const OpenVRLoader* loader) {
  (void) loader; runtime->initialized = true; runtime->input = &backendInput; return EVRInitError_VRInitError_None;
}
void lovrOpenVRDisconnect(OpenVRRuntime* runtime) {
  if (runtime->initialized) {
#ifdef LOVR_OPENVR_VALIDATE_CONFIG
    checkRuntimeShutdown();
#endif
    shutdowns++;
  }
  memset(runtime, 0, sizeof(*runtime));
}
#ifndef LOVR_OPENVR_VALIDATE_CONFIG
static OpenVRPanelResult panelStatus(bool failed) {
  return (OpenVRPanelResult) { .status = failed ? OPENVR_PANEL_RUNTIME_ERROR : OPENVR_PANEL_OK, .operation = "fake" };
}
#endif
OpenVRPanelResult lovrOpenVRPanelCreate(OpenVRPanel* panel, struct VR_IVROverlay_FnTable* api,
  const char* key, const char* name, const OpenVRPanelConfig* properties) {
  (void) api; (void) key; (void) name; (void) properties;
#ifdef LOVR_OPENVR_GRAPHICS_HARNESS
#ifdef LOVR_OPENVR_VALIDATE_CONFIG
  return testedPanelCreate(panel, api, key, name, properties);
#else
  panel->handle = 1;
  return panelStatus(false);
#endif
#else
  (void) panel; abort();
#endif
}
OpenVRPanelResult lovrOpenVRPanelConfigure(OpenVRPanel* panel, const OpenVRPanelConfig* properties) {
#ifdef LOVR_OPENVR_VALIDATE_CONFIG
  return testedPanelConfigure(panel, properties);
#else
  (void) panel; (void) properties; return panelStatus(failProperty);
#endif
}
OpenVRPanelResult lovrOpenVRPanelShow(OpenVRPanel* panel) {
#ifdef LOVR_OPENVR_VALIDATE_CONFIG
  return testedPanelShow(panel);
#else
  (void) panel; return panelStatus(false);
#endif
}
OpenVRPanelResult lovrOpenVRPanelHide(OpenVRPanel* panel) {
#ifdef LOVR_OPENVR_VALIDATE_CONFIG
  return testedPanelHide(panel);
#else
  (void) panel; hides++; return panelStatus(false);
#endif
}
OpenVRPanelResult lovrOpenVRPanelDestroy(OpenVRPanel* panel) {
#ifdef LOVR_OPENVR_VALIDATE_CONFIG
  return testedPanelDestroy(panel);
#else
  destroys++;
  if (!failDestroy) panel->handle = 0;
  return panelStatus(failDestroy);
#endif
}
VkResult lovrOpenVRVulkanGetPhysicalDevice(const OpenVRVulkan* context, VkInstance instance, VkPhysicalDevice* device) {
  (void) context; (void) instance; *device = VK_NULL_HANDLE; return VK_ERROR_INITIALIZATION_FAILED;
}
VkResult lovrOpenVRVulkanCreateInstance(const OpenVRVulkan* context, const VkInstanceCreateInfo* info,
  const VkAllocationCallbacks* allocator, VkInstance* instance) {
  (void) context; (void) info; (void) allocator; *instance = VK_NULL_HANDLE; return VK_ERROR_INITIALIZATION_FAILED;
}
VkResult lovrOpenVRVulkanCreateDevice(const OpenVRVulkan* context, VkInstance instance, VkPhysicalDevice device,
  const VkDeviceCreateInfo* info, const VkAllocationCallbacks* allocator, VkDevice* output) {
  (void) context; (void) instance; (void) device; (void) info; (void) allocator; *output = VK_NULL_HANDLE; return VK_ERROR_INITIALIZATION_FAILED;
}

#ifndef LOVR_OPENVR_GRAPHICS_HARNESS
static void begin(void) {
  memset(&state, 0, sizeof(state));
  failWait = failDestroy = failProperty = false;
  waits = destroys = shutdowns = hides = 0;
  HeadsetConfig config = { .overlay = true, .supersample = 1.f };
  if (!init(&config) || !connect() || !start()) abort();
}
static Layer* wrapper(void) {
  Layer* layer = lovrCalloc(sizeof(*layer));
  layer->ownership = (LayerHeader) { .ref = 1, .creator = &lovrHeadsetOpenVROps, .generation = generation() };
  layer->panel.handle = 1;
  layer->next = state.all;
  state.all = layer;
  return layer;
}
static bool neverSubmitted(void) {
  begin();
  Layer* layer = wrapper();
  uint32_t old = generation();
  stop();
  CHECK(connected() && !active() && !state.all && destroys == 1);
  CHECK(!current(layer));
  CHECK(start() && generation() > old);
  lovrRelease(layer, layerDestroy);
  CHECK(disconnect() && shutdowns == 1);
  return true;
}
static bool atomicLists(void) {
  begin();
  Layer* a = wrapper();
  Layer* b = wrapper();
  Layer* list[] = { a, b };
  CHECK(setLayers(list, 2, true));
  CHECK(state.main && atomic_load(&a->ownership.ref) == 2);
  b->ownership.generation--;
  CHECK(!setLayers(list + 1, 1, false));
  CHECK(state.count == 2 && state.main && state.layers[0] == a && atomic_load(&a->ownership.ref) == 2);
  b->ownership.generation++;
  CHECK(setLayers(state.layers, 2, false));
  CHECK(atomic_load(&a->ownership.ref) == 2);
  CHECK(!state.main);
  CHECK(setLayers(list, 2, true) && state.main);
  stop();
  lovrRelease(a, layerDestroy); lovrRelease(b, layerDestroy);
  CHECK(disconnect());
  return true;
}
static bool retryCleanup(void) {
  begin();
  Layer* layer = wrapper();
  failWait = true;
  lovrRelease(layer, layerDestroy);
  CHECK(state.all == layer && layer->orphan);
  CHECK(!disconnect() && connected() && shutdowns == 0 && destroys == 0);
  CHECK(!start());
  failWait = false; failDestroy = true;
  CHECK(!disconnect() && connected() && state.all == layer);
  failDestroy = false;
  CHECK(disconnect() && !state.all && shutdowns == 1);
  CHECK(disconnect() && shutdowns == 1);
  return true;
}
static bool propertyFailure(void) {
  begin();
  Layer* layer = wrapper();
  layer->properties.width = 1.f;
  failProperty = true;
  setDimensions(layer, 2.f, 3.f);
  CHECK(layer->properties.width == 1.f);
  failProperty = false;
  setDimensions(layer, 2.f, 3.f);
  CHECK(layer->properties.width == 2.f && layer->properties.height == 3.f);
  CHECK(disconnect()); lovrRelease(layer, layerDestroy);
  return true;
}
static EVROverlayError submitTexture(VROverlayHandle_t handle, Texture_t* texture) {
  VRVulkanTextureData_t* image = texture->handle;
  if (handle != 1 || texture->eType != ETextureType_TextureType_Vulkan || image->m_nImage != 42 || image->m_nWidth != 128 || image->m_nSampleCount != 1) abort();
  textureSubmits++;
  return EVROverlayError_VROverlayError_None;
}
static bool stereoTransport(void) {
  begin();
  static struct VR_IVROverlay_FnTable api;
  api.SetOverlayTexture = submitTexture;
  state.runtime.overlay = &api;
  Layer* layer = wrapper();
  layer->ownership.info = (LayerInfo) { .width = 64, .height = 32, .stereo = true, .immutable = true };
  layer->texture = (Texture*) 1; layer->output = (Texture*) 2;
  layer->viewport[2] = 64; layer->viewport[3] = 32;
  CHECK(setLayers(&layer, 1, false));
  copies = handoffs = textureSubmits = 0;
  failHandoff = true;
  CHECK(!submit() && !layer->frozen);
  CHECK(copies == 2 && copiedEye[0] == 0 && copiedEye[1] == 1 && copiedX[0] == 0 && copiedX[1] == 64);
  copies = 0; failHandoff = false;
  CHECK(submit() && layer->frozen && textureSubmits == 1);
  CHECK(setLayers(NULL, 0, false) && submit() && hides == 1);
  CHECK(setLayers(&layer, 1, false) && submit() && textureSubmits == 1 && handoffs == 2);
  int32_t crop[4] = { 16, 8, 32, 16 };
  blits = 0;
  setViewport(layer, crop);
  CHECK(submit() && blits == 2 && layer->frozen && !layer->viewportDirty);
  CHECK(textureSubmits == 2);
  CHECK(submit() && textureSubmits == 2);
  int32_t observed[4]; getViewport(layer, observed);
  CHECK(memcmp(crop, observed, sizeof(crop)) == 0);
  layer->texture = layer->output = NULL;
  CHECK(disconnect()); lovrRelease(layer, layerDestroy);
  return true;
}
static bool borrowedConfig(void) {
  memset(&state, 0, sizeof(state));
  char extensions[] = "VK_TEST_extension";
  HeadsetConfig config = { .overlay = true, .supersample = 1.f, .extensions = extensions, .extensionCount = 1 };
  CHECK(init(&config) && connect() && start());
  CHECK(state.config.extensions == extensions);
  Layer* layer = wrapper();
  failWait = true;
  destroy();
  CHECK(connected() && state.initialized && state.config.extensions == extensions);
  failWait = false;
  destroy();
  CHECK(!connected() && !state.initialized && !state.config.extensions);
  CHECK(config.extensions == extensions && strcmp(extensions, "VK_TEST_extension") == 0);
  lovrRelease(layer, layerDestroy);
  CHECK(init(&config));
  destroy();
  return true;
}
static bool unsupportedOutputs(void) {
  begin();
  float p[3] = { 9, 9, 9 }, q[4] = { 9, 9, 9, 9 };
  CHECK(!pose(DEVICE_HEAD, p, q) && p[0] == 0.f && q[3] == 0.f);
  Texture* texture = (Texture*) 1;
  CHECK(sceneTexture(&texture) && !texture);
  HeadsetFeatures output;
  features(&output);
  CHECK(output.overlay && !output.handTracking && !output.depthSubmission && !output.layerFilter);
  CHECK(disconnect());
  return true;
}
static bool connectionTransaction(void) {
  memset(&state, 0, sizeof(state));
  HeadsetConfig config = { .overlay = true, .supersample = 1.f };
  CHECK(init(&config));
  failAssets = true;
  CHECK(!connect() && !connected() && !state.inputReady && !state.input.api);
  CHECK(strstr(lovrGetError(), "errno 13"));
  failAssets = false; failManifest = true;
  unsigned before = shutdowns;
  CHECK(!connect() && shutdowns == before + 1 && !state.input.api);
  CHECK(strstr(lovrGetError(), "/managed/lovr-openvr/actions.json"));
  failManifest = false;
  CHECK(connect() && state.inputReady && start());
  VRActionHandle_t handle = state.input.actions[0];
  state.input.hands[0].buttons[0].bActive = true;
  state.inputSerial = 9;
  stop();
  CHECK(state.inputReady && state.input.actions[0] == handle && !state.inputSerial && !state.input.hands[0].buttons[0].bActive);
  CHECK(start() && state.input.actions[0] == handle);
  CHECK(disconnect() && !state.inputReady && !state.input.api && !state.input.actions[0]);
  return true;
}
static bool snapshotRoutes(void) {
  begin();
  state.frame.snapshot.head.valid = state.frame.snapshot.velocityValid = true;
  state.frame.snapshot.head.position[1] = 1.5f;
  state.frame.snapshot.head.orientation[3] = 1.f;
  state.frame.snapshot.velocity[0] = 2.f;
  state.inputSerial = 1;
  InputPoseActionData_t* grip = &state.input.hands[0].poses[0];
  *grip = (InputPoseActionData_t) { .bActive = true, .pose = { .bDeviceIsConnected = true, .bPoseIsValid = true,
    .mDeviceToAbsoluteTracking = { .m = { { 1, 0, 0, 3 }, { 0, 1, 0, 4 }, { 0, 0, 1, 5 } } } } };
  state.input.hands[0].poses[1] = *grip;
  state.input.hands[0].poses[1].pose.mDeviceToAbsoluteTracking.m[0][3] = 6;
  float p[3], q[4], v[3], a[3];
  state.frame.snapshot.head.valid = false;
  CHECK(!pose(DEVICE_HEAD, p, q) && p[0] == 0.f && q[0] == 0.f && q[3] == 0.f);
  state.frame.snapshot.head.valid = true;
  CHECK(pose(DEVICE_HEAD, p, q) && p[1] == 1.5f && q[3] == 1.f);
  CHECK(velocity(DEVICE_HEAD, v, a) && v[0] == 2.f);
  CHECK(pose(DEVICE_HAND_LEFT, p, q) && p[0] == 3.f);
  CHECK(pose(DEVICE_HAND_LEFT_POINT, p, q) && p[0] == 6.f);
  grip->pose.mDeviceToAbsoluteTracking.m[0][0] = NAN;
  CHECK(!pose(DEVICE_HAND_LEFT, p, q) && p[0] == 0.f && q[0] == 0.f && q[3] == 0.f);
  grip->pose.mDeviceToAbsoluteTracking.m[0][0] = 1.f;
  CHECK(!pose(DEVICE_HAND_RIGHT, p, q) && q[3] == 0.f);
  CHECK(!modelPose(NULL, p, q) && p[0] == 0.f && q[3] == 0.f);
  CHECK(vibrate(DEVICE_HAND_LEFT, .5f, 1.f, 0.f) && state.haptics.hands[0].active);
  stopVibration(DEVICE_HAND_LEFT);
  CHECK(!state.haptics.hands[0].active);
  Device aliases[] = { DEVICE_HAND_LEFT_GRIP, DEVICE_HAND_LEFT_POINT, DEVICE_HAND_RIGHT_GRIP, DEVICE_HAND_RIGHT_POINT };
  for (unsigned i = 0; i < 4; i++) {
    CHECK(vibrate(aliases[i], .5f, 1.f, 0.f) && state.haptics.hands[i / 2].active);
    stopVibration(aliases[i]);
    CHECK(!state.haptics.hands[i / 2].active);
  }
  CHECK(vibrate(DEVICE_HAND_RIGHT, .5f, 1.f, 120.f));
  stop();
  CHECK(!pose(DEVICE_HEAD, p, q) && p[0] == 0.f && q[3] == 0.f && !state.haptics.hands[1].active);
  CHECK(!pose(DEVICE_HAND_LEFT, p, q) && q[3] == 0.f);
  CHECK(!velocity(DEVICE_HAND_LEFT, v, a) && v[0] == 0.f);
  CHECK(disconnect());
  return true;
}
static bool asymmetricViewAngles(void) {
  begin();
  state.frame.updated = true;
  float tangents[2][4] = { { -1.2f, .8f, -.7f, 1.1f }, { -.9f, 1.3f, -1.2f, .6f } };
  for (unsigned eye = 0; eye < 2; eye++) {
    state.frame.snapshot.eyes[eye].pose.valid = true;
    memcpy(state.frame.snapshot.eyes[eye].tangents, tangents[eye], sizeof(tangents[eye]));
    float left, right, up, down;
    CHECK(lovrHeadsetOpenVROps.HeadsetGetViewAngles(eye, &left, &right, &up, &down));
    CHECK(fabsf(left + atanf(tangents[eye][0])) < 1e-5f);
    CHECK(fabsf(right - atanf(tangents[eye][1])) < 1e-5f);
    CHECK(fabsf(up - atanf(tangents[eye][3])) < 1e-5f);
    CHECK(fabsf(down + atanf(tangents[eye][2])) < 1e-5f);
    float matrix[16], oracle[16];
    CHECK(lovrOpenVRFrameProjection(tangents[eye], .1f, 0.f, matrix));
    mat4_fov(oracle, left, right, up, down, .1f, 0.f);
    for (unsigned i = 0; i < 16; i++) CHECK(fabsf(matrix[i] - oracle[i]) < 1e-5f);
  }
  CHECK(disconnect());
  return true;
}
static unsigned eventPolls;
static uint32_t queuedEvent;
static bool OPENVR_FNTABLE_CALLTYPE backendPoll(struct VREvent_t* event, uint32_t size) {
  (void) size;
  if (!manifests) abort();
  eventPolls++;
  if (!queuedEvent) return false;
  *event = (struct VREvent_t) { .eventType = queuedEvent, .data.process.pid = (uint32_t) getpid() };
  queuedEvent = 0;
  return true;
}
static bool eventIntegration(void) {
  begin();
  struct VR_IVRSystem_FnTable system = { .PollNextEvent = backendPoll, .AcknowledgeQuit_Exiting = backendAck };
  quitAcks = 0;
  state.runtime.system = &system;
  backendEventCount = eventPolls = 0;
  state.inputReady = false;
  CHECK(!pollEvents() && !eventPolls);
  state.inputReady = true;
  CHECK(pollEvents() && eventPolls == 1 && mainVisible() && !visible(NULL) && !focused() && !mounted());
  CHECK(backendEventCount == 3 && backendEvents[0].type == EVENT_VISIBLE &&
    backendEvents[1].type == EVENT_FOCUS && backendEvents[2].type == EVENT_MOUNT);
  state.frame.updated = state.frame.snapshot.head.valid = true;
  state.inputSerial = 4;
  state.haptics.hands[0].active = true;
  CHECK(!vibrate(DEVICE_HAND_LEFT, .5f, 1.f, 120.f));
  CHECK(pollEvents() && state.inputSerial == 4 && state.frame.snapshot.head.valid && !state.haptics.hands[0].active);
  queuedEvent = EVREventType_VREvent_StandingZeroPoseReset;
  CHECK(pollEvents() && backendEvents[3].type == EVENT_RECENTER);
  CHECK(state.frame.updated && !state.frame.snapshot.head.valid && !state.inputSerial && !state.haptics.hands[0].active);
  queuedEvent = EVREventType_VREvent_Quit;
  CHECK(pollEvents() && backendEvents[4].type == EVENT_QUIT);
  CHECK(pollEvents() && backendEventCount == 5);
  state.events.visible = state.events.inputFocus = state.events.activityKnown = state.events.active = true;
  stop();
  CHECK(state.events.quitRequested && backendEventCount == 8);
  CHECK(backendEvents[5].type == EVENT_VISIBLE && !backendEvents[5].data.visible.visible);
  CHECK(backendEvents[6].type == EVENT_FOCUS && !backendEvents[6].data.focus.focused);
  CHECK(backendEvents[7].type == EVENT_MOUNT && !backendEvents[7].data.mount.mounted);
  stop();
  CHECK(backendEventCount == 8);
  unsigned before = eventPolls;
  CHECK(pollEvents() && eventPolls == before && !visible(NULL) && mainVisible());
  CHECK(start() && !quitAcks);
  queuedEvent = EVREventType_VREvent_ProcessQuit;
  CHECK(pollEvents() && !quitAcks);
  CHECK(disconnect() && !quitAcks);
  CHECK(connect() && start());
  state.runtime.system = &system;
  queuedEvent = EVREventType_VREvent_ProcessQuit;
  CHECK(pollEvents() && !quitAcks);
  destroy();
  CHECK(!quitAcks);
  return true;
}
static bool actualExit(void) {
  begin();
  struct VR_IVRSystem_FnTable system = { .PollNextEvent = backendPoll, .AcknowledgeQuit_Exiting = backendAck };
  state.runtime.system = &system;
  quitAcks = 0;
  lovrHeadsetOpenVROps.WillExit();
  CHECK(!quitAcks);
  struct VREvent_t foreign = { .eventType = EVREventType_VREvent_ProcessQuit,
    .data.process.pid = (uint32_t) getpid() + 1 };
  bool dashboardActivated = false;
  OpenVREventEffects effects = { 0 };
  eventsReduce(&state.events, &foreign, (uint32_t) getpid(), &dashboardActivated, &effects);
  CHECK(!state.events.quitRequested);
  lovrHeadsetOpenVROps.WillExit();
  CHECK(!quitAcks);
  queuedEvent = EVREventType_VREvent_Quit;
  CHECK(pollEvents() && state.events.quitRequested && !quitAcks);
  CHECK(pollEvents() && !quitAcks);
  stop();
  CHECK(!quitAcks);
  CHECK(start() && !quitAcks);
  CHECK(disconnect() && !quitAcks);
  lovrHeadsetOpenVROps.WillExit();
  CHECK(!quitAcks);
  destroy();
  begin();
  state.runtime.system = &system;
  lovrHeadsetOpenVROps.WillExit();
  CHECK(!quitAcks);
  queuedEvent = EVREventType_VREvent_ProcessQuit;
  CHECK(pollEvents() && state.events.quitRequested && !quitAcks);
  lovrHeadsetOpenVROps.WillExit();
  lovrHeadsetOpenVROps.WillExit();
  CHECK(quitAcks == 1);
  CHECK(disconnect() && quitAcks == 1);
  destroy();
  CHECK(quitAcks == 1);
  begin();
  state.runtime.system = &system;
  lovrHeadsetOpenVROps.WillExit();
  CHECK(quitAcks == 1);
  queuedEvent = EVREventType_VREvent_Quit;
  CHECK(pollEvents() && state.events.quitRequested && quitAcks == 1);
  lovrHeadsetOpenVROps.WillExit();
  lovrHeadsetOpenVROps.WillExit();
  CHECK(quitAcks == 2);
  destroy();
  CHECK(quitAcks == 2);
  return true;
}

static bool invalidScale(void) {
  float scales[] = { 0.f, -1.f, NAN, INFINITY };
  for (unsigned i = 0; i < sizeof(scales) / sizeof(scales[0]); i++) {
    memset(&state, 0, sizeof(state));
    HeadsetConfig config = { .overlay = true, .supersample = scales[i] };
    CHECK(!init(&config) && !state.initialized);
  }
  begin();
  state.config.supersample = 1e30f;
  state.frame.updated = true;
  state.frame.snapshot.width = 64; state.frame.snapshot.height = 32;
  state.frame.snapshot.head.valid = true;
  state.frame.snapshot.eyes[0].pose.valid = state.frame.snapshot.eyes[1].pose.valid = true;
  Texture* texture;
  CHECK(!sceneTexture(&texture) && !texture && !state.scene.texture);
  CHECK(strstr(lovrGetError(), "dimensions"));
  CHECK(disconnect());
  return true;
}
int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "openvr.backend.never-submitted-restart", neverSubmitted },
    { "openvr.backend.atomic-lists", atomicLists },
    { "openvr.backend.cleanup-retries", retryCleanup },
    { "openvr.backend.property-failure", propertyFailure },
    { "openvr.backend.unsupported-outputs", unsupportedOutputs },
    { "openvr.backend.stereo-immutable-transport", stereoTransport },
    { "openvr.backend.borrowed-config", borrowedConfig },
    { "openvr.backend.invalid-scale", invalidScale },
    { "openvr.backend.connection-transaction", connectionTransaction },
    { "openvr.backend.snapshot-routes", snapshotRoutes },
    { "openvr.backend.asymmetric-view-angles", asymmetricViewAngles },
    { "openvr.backend.events-recenter", eventIntegration },
    { "openvr.backend.actual-exit", actualExit }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
#endif
