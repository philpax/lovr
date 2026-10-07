#include "test.h"
#ifndef LOVR_OPENVR_GRAPHICS_HARNESS
#include "../../src/modules/headset/headset_openvr.c"
#endif
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
  (void) loader; runtime->initialized = true; return EVRInitError_VRInitError_None;
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
    { "openvr.backend.invalid-scale", invalidScale }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
#endif
