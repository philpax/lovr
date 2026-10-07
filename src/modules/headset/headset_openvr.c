#include "headset/headset_layer.h"
#include "headset/openvr_runtime.h"
#include "headset/openvr_overlay.h"
#include "headset/openvr_vulkan.h"
#include "graphics/graphics.h"
#include "graphics/graphics_session.h"
#include "graphics/graphics_external.h"
#include "core/gpu.h"
#include "data/image.h"
#include "core/maf.h"
#include "util.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern const HeadsetOps lovrHeadsetOpenVROps;

struct Layer {
  LayerHeader ownership;
  Layer* next;
  OpenVRPanel panel;
  OpenVRPanelConfig properties;
  Texture* texture;
  Texture* output;
  Pass* pass;
  Device origin;
  float position[3];
  float orientation[4];
  int32_t viewport[4];
  bool orphan;
  bool frozen;
  bool viewportDirty;
  bool submitted;
};

static struct {
  HeadsetConfig config;
  OpenVRRuntime runtime;
  Layer* all;
  Layer* layers[MAX_LAYERS];
  uint32_t count;
  uint32_t generation;
  uint64_t serial;
  PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices;
  bool initialized;
  bool stopping;
  float clipNear;
  float clipFar;
} state;

static bool panelResult(OpenVRPanelResult result) {
  if (result.status == OPENVR_PANEL_OK) return true;
  lovrSetError("openvr: %s: overlay error %d (cleanup %d)", result.operation ? result.operation : "panel", result.error, result.cleanupError);
  return false;
}

static bool current(Layer* layer) {
  lovrAssert(layer && layer->ownership.creator == &lovrHeadsetOpenVROps && state.generation &&
    layer->ownership.generation == state.generation && !state.stopping && !layer->orphan,
    "openvr: layer belongs to an inactive headset session");
  return true;
}

static void unlinkLayer(Layer* layer) {
  Layer** link = &state.all;
  while (*link && *link != layer) link = &(*link)->next;
  if (*link) *link = layer->next;
  layer->next = NULL;
}

static bool retireLayer(Layer* layer) {
  if (layer->panel.handle && !panelResult(lovrOpenVRPanelDestroy(&layer->panel))) return false;
  lovrGraphicsInvalidateSessionPass(layer->pass);
  lovrGraphicsInvalidateSessionTexture(layer->texture);
  lovrGraphicsInvalidateSessionTexture(layer->output);
  lovrRelease(layer->pass, lovrPassDestroy);
  lovrRelease(layer->texture, lovrTextureDestroy);
  lovrRelease(layer->output, lovrTextureDestroy);
  layer->pass = NULL;
  layer->texture = NULL;
  layer->output = NULL;
  unlinkLayer(layer);
  if (layer->orphan) lovrFree(layer);
  return true;
}

static void layerDestroy(void* ref) {
  Layer* layer = ref;
  layer->orphan = true;
  if (!layer->panel.handle && !layer->texture && !layer->output && !layer->pass) {
    unlinkLayer(layer);
    lovrFree(layer);
    return;
  }
  if (state.stopping) return;
  if (lovrGraphicsQuiesceSessionResources()) {
    retireLayer(layer);
    lovrGraphicsDrainSessionResources();
  }
}

static bool cleanup(void) {
  if (!state.stopping) return true;
  if (!lovrGraphicsQuiesceSessionResources()) return false;
  bool ok = true;
  for (Layer* layer = state.all, *next; layer; layer = next) {
    next = layer->next;
    if (!retireLayer(layer)) ok = false;
  }
  if (!lovrGraphicsDrainSessionResources()) ok = false;
  if (ok) state.stopping = false;
  return ok;
}

static void stop(void) {
  state.generation = 0;
  state.stopping = state.all != NULL || state.stopping;
  for (Layer* layer = state.all; layer; layer = layer->next) {
    if (layer->panel.handle) panelResult(lovrOpenVRPanelHide(&layer->panel));
  }
  uint32_t count = state.count;
  state.count = 0;
  for (uint32_t i = 0; i < count; i++) {
    Layer* layer = state.layers[i];
    state.layers[i] = NULL;
    lovrRelease(layer, layerDestroy);
  }
  cleanup();
}

static bool disconnect(void) {
  stop();
  if (!cleanup()) return false;
  lovrOpenVRDisconnect(&state.runtime);
  return true;
}

static bool init(HeadsetConfig* config) {
  lovrAssert(config && !state.initialized, "openvr: backend is already initialized or config is null");
  state.config = *config;
  state.clipNear = .01f;
  state.clipFar = 0.f;
  state.initialized = true;
  return true;
}

static void destroy(void) {
  if (!disconnect()) return;
  memset(&state.config, 0, sizeof(state.config));
  state.enumeratePhysicalDevices = NULL;
  state.initialized = false;
}

static bool connect(void) {
  lovrAssert(state.initialized && state.config.overlay, "openvr: only overlay applications are supported");
  EVRInitError error = lovrOpenVRConnect(&state.runtime, NULL);
  lovrAssert(error == EVRInitError_VRInitError_None, "openvr: runtime connection error %d", error);
  return true;
}

static bool connected(void) { return state.runtime.initialized; }
static bool active(void) { return state.generation != 0 && !state.stopping; }
static uint32_t generation(void) { return state.generation; }
static const char* name(void) { return NULL; }
static const char* driver(void) { return connected() ? "SteamVR" : NULL; }
static bool seated(void) { return state.config.seated; }
static uint32_t limit(void) { return MAX_LAYERS - 2; }
static bool start(void) {
  lovrAssert(connected(), "openvr: runtime is not connected");
  if (active()) return true;
  if (!cleanup()) return false;
  state.generation = lovrHeadsetNextSessionGeneration();
  return state.generation != 0;
}

static void features(HeadsetFeatures* output) {
  memset(output, 0, sizeof(*output));
  output->overlay = true;
  output->layerColor = true;
  output->layerCurve = true;
}

static Texture* newTexture(uint32_t width, uint32_t height, bool stereo) {
  TextureInfo info = {
    .type = stereo ? TEXTURE_ARRAY : TEXTURE_2D,
    .format = FORMAT_RGBA8,
    .width = width, .height = height, .layers = stereo ? 2 : 1,
    .mipmaps = 1, .samples = 1, .srgb = true,
    .usage = TEXTURE_SAMPLE | TEXTURE_RENDER | TEXTURE_TRANSFER,
    .label = "OpenVR panel"
  };
  Texture* texture = lovrTextureCreate(&info);
  if (texture && !lovrGraphicsRegisterSessionTexture(texture)) {
    lovrRelease(texture, lovrTextureDestroy);
    return NULL;
  }
  return texture;
}

static Layer* layerCreate(const LayerInfo* info) {
  lovrAssert(active() && info && info->width && info->height && info->width <= INT32_MAX / 2 && info->height <= INT32_MAX,
    "openvr: invalid panel dimensions or inactive session");
  lovrAssert(info->filter, "openvr: nearest panel filtering is not supported; use the default filter");
  Layer* layer = lovrCalloc(sizeof(*layer));
  layer->ownership = (LayerHeader) { .ref = 1, .info = *info, .creator = &lovrHeadsetOpenVROps, .generation = state.generation };
  layer->origin = DEVICE_FLOOR;
  layer->orientation[3] = 1.f;
  layer->viewport[2] = info->width;
  layer->viewport[3] = info->height;
  layer->properties = (OpenVRPanelConfig) {
    .textureWidth = info->width * (info->stereo ? 2 : 1), .textureHeight = info->height,
    .viewport = { 0, 0, info->width, info->height },
    .width = 1.f, .height = (float) info->height / info->width,
    .color = { 1.f, 1.f, 1.f, 1.f },
    .colorSpace = EColorSpace_ColorSpace_Gamma,
    .premultiplied = true, .ignoreTextureAlpha = !info->transparent,
    .textureLayout = info->stereo ? OPENVR_PANEL_TEXTURE_SIDE_BY_SIDE_2D : OPENVR_PANEL_TEXTURE_MONO_2D, .filter = OPENVR_PANEL_FILTER_RUNTIME_DEFAULT,
    .order = state.config.overlayOrder,
    .transform = OPENVR_PANEL_ABSOLUTE,
    .origin = state.config.seated ? ETrackingUniverseOrigin_TrackingUniverseSeated : ETrackingUniverseOrigin_TrackingUniverseStanding,
    .pose = { .m = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } } }
  };
  layer->next = state.all;
  state.all = layer;
  layer->texture = newTexture(info->width, info->height, info->stereo);
  if (!layer->texture) goto fail;
  layer->output = newTexture(info->width * (info->stereo ? 2 : 1), info->height, false);
  if (!layer->output) goto fail;
  char key[96];
  snprintf(key, sizeof(key), "lovr.panel.%u.%llu", state.generation, (unsigned long long) ++state.serial);
  if (!panelResult(lovrOpenVRPanelCreate(&layer->panel, state.runtime.overlay, key, "LÖVR panel", &layer->properties))) goto fail;
  return layer;
fail:
  lovrRelease(layer, layerDestroy);
  return NULL;
}

static Layer** getLayers(uint32_t* count, bool* main) {
  *count = state.count;
  *main = false;
  return state.layers;
}

static bool setLayers(Layer** layers, uint32_t count, bool main) {
  lovrAssert(active() && count <= limit() && (!count || layers), "openvr: invalid layer submission");
  lovrAssert(!main, "openvr: projection presentation is not implemented");
  for (uint32_t i = 0; i < count; i++) {
    if (!current(layers[i])) return false;
    for (uint32_t j = 0; j < i; j++) lovrAssert(layers[i] != layers[j], "openvr: duplicate layer submission");
  }
  Layer* previous[MAX_LAYERS];
  uint32_t previousCount = state.count;
  memcpy(previous, state.layers, previousCount * sizeof(*previous));
  for (uint32_t i = 0; i < count; i++) lovrRetain(layers[i]);
  if (count) memmove(state.layers, layers, count * sizeof(*layers));
  state.count = count;
  for (uint32_t i = 0; i < previousCount; i++) lovrRelease(previous[i], layerDestroy);
  return true;
}

static bool configure(Layer* layer, OpenVRPanelConfig* properties) {
  if (!current(layer) || !panelResult(lovrOpenVRPanelConfigure(&layer->panel, properties))) return false;
  layer->properties = *properties;
  return true;
}

static Device getOrigin(Layer* layer) { return current(layer) ? layer->origin : DEVICE_FLOOR; }
static void setOrigin(Layer* layer, Device device) {
  if (!current(layer)) return;
  OpenVRPanelConfig properties = layer->properties;
  if (device == DEVICE_FLOOR) properties.transform = OPENVR_PANEL_ABSOLUTE;
  else if (device == DEVICE_HEAD) {
    properties.transform = OPENVR_PANEL_DEVICE_RELATIVE;
    properties.device = k_unTrackedDeviceIndex_Hmd;
  } else {
    lovrSetError("openvr: device attachment is not implemented for this device");
    return;
  }
  if (configure(layer, &properties)) layer->origin = device;
}
static void getPose(Layer* layer, float* position, float* orientation) {
  memset(position, 0, 3 * sizeof(float));
  memset(orientation, 0, 4 * sizeof(float));
  if (!current(layer)) return;
  memcpy(position, layer->position, 3 * sizeof(float));
  memcpy(orientation, layer->orientation, 4 * sizeof(float));
}
static void setPose(Layer* layer, float* position, float* orientation) {
  if (!current(layer)) return;
  for (unsigned i = 0; i < 3; i++) if (!isfinite(position[i])) { lovrSetError("openvr: invalid position"); return; }
  float norm = 0.f;
  for (unsigned i = 0; i < 4; i++) norm += orientation[i] * orientation[i];
  if (!isfinite(norm) || norm < 1e-12f) { lovrSetError("openvr: invalid orientation"); return; }
  float q[4], matrix[16];
  for (unsigned i = 0; i < 4; i++) q[i] = orientation[i] / sqrtf(norm);
  mat4_fromPose(matrix, position, q);
  OpenVRPanelConfig properties = layer->properties;
  for (unsigned row = 0; row < 3; row++) for (unsigned col = 0; col < 4; col++) properties.pose.m[row][col] = matrix[col * 4 + row];
  if (configure(layer, &properties)) {
    memcpy(layer->position, position, sizeof(layer->position));
    memcpy(layer->orientation, q, sizeof(q));
  }
}
static void getDimensions(Layer* layer, float* width, float* height) {
  *width = *height = 0.f;
  if (current(layer)) { *width = layer->properties.width; *height = layer->properties.height; }
}
static void setDimensions(Layer* layer, float width, float height) {
  if (!current(layer)) return;
  OpenVRPanelConfig properties = layer->properties;
  properties.width = width; properties.height = height;
  configure(layer, &properties);
}
static float getCurve(Layer* layer) { return current(layer) ? layer->properties.curve : 0.f; }
static bool setCurve(Layer* layer, float curve) {
  if (!current(layer)) return false;
  OpenVRPanelConfig properties = layer->properties;
  properties.curve = curve;
  return configure(layer, &properties);
}
static void getColor(Layer* layer, float color[4]) {
  memset(color, 0, 4 * sizeof(float));
  if (current(layer)) memcpy(color, layer->properties.color, 4 * sizeof(float));
}
static void setColor(Layer* layer, float color[4]) {
  if (!current(layer)) return;
  OpenVRPanelConfig properties = layer->properties;
  memcpy(properties.color, color, sizeof(properties.color));
  configure(layer, &properties);
}
static void getViewport(Layer* layer, int32_t* viewport) {
  memset(viewport, 0, 4 * sizeof(int32_t));
  if (current(layer)) memcpy(viewport, layer->viewport, 4 * sizeof(int32_t));
}
static void setViewport(Layer* layer, int32_t* viewport) {
  if (!current(layer)) return;
  int64_t x = viewport[0], y = viewport[1];
  int64_t width = viewport[2] ? viewport[2] : (int64_t) layer->ownership.info.width - x;
  int64_t height = viewport[3] ? viewport[3] : (int64_t) layer->ownership.info.height - y;
  if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
      x + width > layer->ownership.info.width || y + height > layer->ownership.info.height) {
    lovrSetError("openvr: invalid per-eye viewport"); return;
  }
  layer->viewportDirty = true;
  layer->viewport[0] = x; layer->viewport[1] = y;
  layer->viewport[2] = width; layer->viewport[3] = height;
}
static Texture* layerTexture(Layer* layer) {
  if (!current(layer)) return NULL;
  if (layer->frozen) { lovrSetError("openvr: immutable layer has already been submitted"); return NULL; }
  return layer->texture;
}
static Pass* layerPass(Layer* layer) {
  Texture* texture = layerTexture(layer);
  if (!texture) return NULL;
  if (!layer->pass) {
    layer->pass = lovrPassCreate("OpenVR panel");
    if (!layer->pass) return NULL;
    if (!lovrGraphicsRegisterSessionPass(layer->pass)) {
      lovrRelease(layer->pass, lovrPassDestroy); layer->pass = NULL; return NULL;
    }
    float clear[4][4] = { 0 };
    LoadAction loads[4] = { LOAD_CLEAR };
    if (!lovrPassSetClear(layer->pass, loads, clear, LOAD_CLEAR, 0.f)) return NULL;
  }
  Canvas canvas = { .color[0].texture = texture, .depthFormat = state.config.stencil ? FORMAT_D24S8 : FORMAT_D32F,
    .samples = state.config.antialias ? 4 : 1 };
  if (!lovrPassSetCanvas(layer->pass, &canvas)) return NULL;
  float view[16] = MAT4_IDENTITY, projection[16];
  mat4_orthographic(projection, 0, layer->ownership.info.width, 0, layer->ownership.info.height, -1.f, 1.f);
  for (unsigned i = 0; i < (layer->ownership.info.stereo ? 2u : 1u); i++) {
    if (!lovrPassSetViewMatrix(layer->pass, i, view) || !lovrPassSetProjection(layer->pass, i, projection)) return NULL;
  }
  return layer->pass;
}

static void physicalDevice(void* instance, uintptr_t output) {
  OpenVRVulkan context = { .runtime = &state.runtime, .enumeratePhysicalDevices = state.enumeratePhysicalDevices };
  VkPhysicalDevice* device = (VkPhysicalDevice*) output;
  *device = VK_NULL_HANDLE;
  VkResult result = lovrOpenVRVulkanGetPhysicalDevice(&context, (VkInstance) instance, device);
  if (result != VK_SUCCESS) lovrSetError("openvr: required Vulkan adapter error %d", result);
}
static uint32_t createInstance(void* info, void* allocator, uintptr_t output, void* getProc) {
  PFN_vkGetInstanceProcAddr get = (PFN_vkGetInstanceProcAddr) getProc;
  OpenVRVulkan context = { .runtime = &state.runtime,
    .createInstance = (PFN_vkCreateInstance) get(VK_NULL_HANDLE, "vkCreateInstance") };
  VkResult result = lovrOpenVRVulkanCreateInstance(&context, info, allocator, (VkInstance*) output);
  if (result == VK_SUCCESS) state.enumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices) get(*(VkInstance*) output, "vkEnumeratePhysicalDevices");
  return result;
}
static uint32_t createDevice(void* instance, void* info, void* allocator, uintptr_t output, void* getProc) {
  PFN_vkGetInstanceProcAddr get = (PFN_vkGetInstanceProcAddr) getProc;
  OpenVRVulkan context = { .runtime = &state.runtime,
    .enumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices) get((VkInstance) instance, "vkEnumeratePhysicalDevices"),
    .createDevice = (PFN_vkCreateDevice) get((VkInstance) instance, "vkCreateDevice") };
  VkPhysicalDevice device = VK_NULL_HANDLE;
  VkResult result = lovrOpenVRVulkanGetPhysicalDevice(&context, (VkInstance) instance, &device);
  if (result != VK_SUCCESS) { *(VkDevice*) output = VK_NULL_HANDLE; return result; }
  return lovrOpenVRVulkanCreateDevice(&context, (VkInstance) instance, device, info, allocator, (VkDevice*) output);
}

static bool unsupported(void) { return false; }
static bool pollEvents(void) { return connected(); }
static bool update(void) { return active(); }
static void dimensions(uint32_t* width, uint32_t* height) { *width = *height = 0; }
static float* refreshRates(uint32_t* count) { *count = 0; return NULL; }
static float refreshRate(void) { return 0.f; }
static bool setRefreshRate(float rate) { (void) rate; return false; }
static void getFoveation(FoveationLevel* level, bool* dynamic) { *level = FOVEATION_NONE; *dynamic = false; }
static bool setFoveation(FoveationLevel level, bool dynamic) { (void) level; (void) dynamic; return false; }
static bool isHDR(void) { return false; }
static bool visible(bool* main) { if (main) *main = false; return false; }
static bool passthroughSupported(PassthroughMode mode) { (void) mode; return false; }
static PassthroughMode passthrough(void) { return PASSTHROUGH_TRANSPARENT; }
static bool setPassthrough(PassthroughMode mode) { (void) mode; return false; }
static uint32_t viewCount(void) { return 0; }
static bool pose(Device device, float* position, float* orientation) {
  (void) device;
  memset(position, 0, 3 * sizeof(float));
  memset(orientation, 0, 4 * sizeof(float));
  return false;
}
static bool handPose(Side side, HandPose type, float* position, float* orientation) {
  memset(position, 0, 3 * sizeof(float));
  quat_identity(orientation);
  if ((side != SIDE_LEFT && side != SIDE_RIGHT) || (type != HAND_GRIP && type != HAND_POINT)) return false;
  Device device = type == HAND_GRIP
    ? (side == SIDE_LEFT ? DEVICE_HAND_LEFT_GRIP : DEVICE_HAND_RIGHT_GRIP)
    : (side == SIDE_LEFT ? DEVICE_HAND_LEFT_POINT : DEVICE_HAND_RIGHT_POINT);
  return pose(device, position, orientation);
}
static bool viewPose(uint32_t view, float* position, float* orientation) { (void) view; return pose(DEVICE_HEAD, position, orientation); }
static bool viewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  (void) view; *left = *right = *up = *down = 0.f; return false;
}
static void getClip(float* near, float* far) { *near = state.clipNear; *far = state.clipFar; }
static void setClip(float near, float far) {
  if (!isfinite(near) || near <= 0 || !isfinite(far) || (far != 0 && far <= near)) {
    lovrSetError("openvr: invalid clip distances"); return;
  }
  state.clipNear = near; state.clipFar = far;
}
static void bounds(float* width, float* depth) { *width = *depth = 0.f; }
static bool velocity(Device device, float* linear, float* angular) {
  (void) device; memset(linear, 0, 3 * sizeof(float)); memset(angular, 0, 3 * sizeof(float)); return false;
}
static bool down(Device device, DeviceButton button, bool* value, bool* changed) {
  (void) device; (void) button; *value = *changed = false; return false;
}
static bool touched(Device device, DeviceButton button, bool* value) { (void) device; (void) button; *value = false; return false; }
static bool axis(Device device, DeviceAxis axis, float* value) {
  (void) device;
  value[0] = 0.f;
  if (axis == AXIS_THUMBSTICK || axis == AXIS_TOUCHPAD) value[1] = 0.f;
  return false;
}
static bool skeleton(Device device, float* poses, SkeletonSource* source) {
  (void) device; memset(poses, 0, HAND_JOINT_COUNT * 8 * sizeof(float)); *source = SOURCE_UNKNOWN; return false;
}
static bool battery(Device device, float* level, bool* charging) { (void) device; *level = 0.f; *charging = false; return false; }
static bool vibrate(Device device, float strength, float duration, float frequency) {
  (void) device; (void) strength; (void) duration; (void) frequency; return false;
}
static void stopVibration(Device device) { (void) device; }
static uint64_t* modelKeys(uint32_t* count) { *count = 0; return NULL; }
static struct ModelData* modelData(uint64_t key) { (void) key; return NULL; }
static bool modelPose(struct Model* model, float* position, float* orientation) { (void) model; return pose(DEVICE_HEAD, position, orientation); }
static bool animate(struct Model* model) { (void) model; return false; }
static Texture* background(uint32_t width, uint32_t height, uint32_t layers) { (void) width; (void) height; (void) layers; return NULL; }
static bool sceneTexture(Texture** texture) { *texture = NULL; return false; }
static bool scenePass(Pass** pass) { *pass = NULL; return false; }
static bool handoff(const gpu_external_image* image, void* data) {
  Layer* layer = data;
  VRVulkanTextureData_t vulkan = {
    .m_nImage = image->image, .m_pDevice = (VkDevice) image->device,
    .m_pPhysicalDevice = (VkPhysicalDevice) image->physicalDevice,
    .m_pInstance = (VkInstance) image->instance, .m_pQueue = (VkQueue) image->queue,
    .m_nQueueFamilyIndex = image->queueFamily, .m_nWidth = image->width,
    .m_nHeight = image->height, .m_nFormat = image->format, .m_nSampleCount = image->samples
  };
  Texture_t texture = { .handle = &vulkan, .eType = ETextureType_TextureType_Vulkan, .eColorSpace = EColorSpace_ColorSpace_Gamma };
  EVROverlayError error = state.runtime.overlay->SetOverlayTexture(layer->panel.handle, &texture);
  lovrAssert(error == EVROverlayError_VROverlayError_None, "openvr: texture submission error %d", error);
  return true;
}
static bool submit(void) {
  lovrAssert(active(), "openvr: presentation is stopped");
  for (Layer* layer = state.all; layer; layer = layer->next) {
    bool included = false;
    for (uint32_t i = 0; i < state.count; i++) included |= state.layers[i] == layer;
    if (!included && layer->panel.handle && !panelResult(lovrOpenVRPanelHide(&layer->panel))) return false;
  }
  for (uint32_t i = 0; i < state.count; i++) {
    Layer* layer = state.layers[i];
    if (!current(layer)) return false;
    OpenVRPanelConfig properties = layer->properties;
    properties.order = state.config.overlayOrder + i;
    if (properties.order < state.config.overlayOrder) { lovrSetError("openvr: overlay sort order overflow"); return false; }
    if (!configure(layer, &properties)) return false;
    if (!layer->frozen || layer->viewportDirty) {
      uint32_t extent[3] = { layer->ownership.info.width, layer->ownership.info.height, 1 };
      for (uint32_t eye = 0; eye < (layer->ownership.info.stereo ? 2u : 1u); eye++) {
        uint32_t source[4] = { layer->viewport[0], layer->viewport[1], eye, 0 };
        uint32_t target[4] = { eye * extent[0], 0, 0, 0 };
        uint32_t crop[3] = { layer->viewport[2], layer->viewport[3], 1 };
        if (crop[0] == extent[0] && crop[1] == extent[1]) {
          if (!lovrTextureCopy(layer->texture, layer->output, source, target, extent)) return false;
        } else if (!lovrTextureBlit(layer->texture, layer->output, source, target, crop, extent, FILTER_LINEAR)) return false;
      }
      if (!lovrGraphicsHandoffTexture(layer->output, handoff, layer, NULL)) return false;
      layer->viewportDirty = false;
      layer->submitted = true;
      layer->frozen = layer->ownership.info.immutable;
    }
    if (!panelResult(lovrOpenVRPanelShow(&layer->panel))) return false;
  }
  return true;
}
static void setDevicePose(Device device, float* position, float* orientation) { (void) device; (void) position; (void) orientation; }
static void setButton(Device device, DeviceButton button, bool down) { (void) device; (void) button; (void) down; }
static double timeZero(void) { return 0.; }

const HeadsetOps lovrHeadsetOpenVROps = {
  .HeadsetInit = init,
  .HeadsetDestroy = destroy,
  .HeadsetConnect = connect,
  .HeadsetDisconnect = disconnect,
  .HeadsetIsConnected = connected,
  .HeadsetGetName = name,
  .HeadsetGetDriver = driver,
  .HeadsetGetFeatures = features,
  .HeadsetGetLayerLimit = limit,
  .HeadsetIsSeated = seated,
  .HeadsetStart = start,
  .HeadsetStop = stop,
  .HeadsetIsActive = active,
  .HeadsetGetSessionGeneration = generation,
  .HeadsetIsMainSessionVisible = unsupported,
  .HeadsetIsVisible = visible,
  .HeadsetIsFocused = unsupported,
  .HeadsetIsMounted = unsupported,
  .HeadsetPollEvents = pollEvents,
  .HeadsetUpdate = update,
  .HeadsetGetDisplayDimensions = dimensions,
  .HeadsetGetRefreshRates = refreshRates,
  .HeadsetGetRefreshRate = refreshRate,
  .HeadsetSetRefreshRate = setRefreshRate,
  .HeadsetGetFoveation = getFoveation,
  .HeadsetSetFoveation = setFoveation,
  .HeadsetIsHDR = isHDR,
  .HeadsetIsPassthroughSupported = passthroughSupported,
  .HeadsetGetPassthrough = passthrough,
  .HeadsetSetPassthrough = setPassthrough,
  .HeadsetGetViewCount = viewCount,
  .HeadsetGetViewPose = viewPose,
  .HeadsetGetViewAngles = viewAngles,
  .HeadsetGetClipDistance = getClip,
  .HeadsetSetClipDistance = setClip,
  .HeadsetGetBoundsDimensions = bounds,
  .HeadsetGetHandPose = handPose,
  .HeadsetGetPose = pose,
  .HeadsetGetVelocity = velocity,
  .HeadsetIsDown = down,
  .HeadsetIsTouched = touched,
  .HeadsetGetAxis = axis,
  .HeadsetGetSkeleton = skeleton,
  .HeadsetGetBattery = battery,
  .HeadsetVibrate = vibrate,
  .HeadsetStopVibration = stopVibration,
  .HeadsetGetModelKeys = modelKeys,
  .HeadsetNewModelData = modelData,
  .HeadsetGetModelPose = modelPose,
  .HeadsetAnimate = animate,
  .HeadsetSetBackground = background,
  .HeadsetGetLayers = getLayers,
  .HeadsetSetLayers = setLayers,
  .HeadsetGetTexture = sceneTexture,
  .HeadsetGetPass = scenePass,
  .HeadsetSubmit = submit,
  .HeadsetSetPose = setDevicePose,
  .HeadsetSetButton = setButton,
  .LayerCreate = layerCreate,
  .LayerDestroy = layerDestroy,
  .LayerGetOrigin = getOrigin,
  .LayerSetOrigin = setOrigin,
  .LayerGetPose = getPose,
  .LayerSetPose = setPose,
  .LayerGetDimensions = getDimensions,
  .LayerSetDimensions = setDimensions,
  .LayerGetCurve = getCurve,
  .LayerSetCurve = setCurve,
  .LayerGetColor = getColor,
  .LayerSetColor = setColor,
  .LayerGetViewport = getViewport,
  .LayerSetViewport = setViewport,
  .LayerGetTexture = layerTexture,
  .LayerGetPass = layerPass,
  .HeadsetGetVulkanPhysicalDevice = physicalDevice,
  .HeadsetCreateVulkanInstance = createInstance,
  .HeadsetCreateVulkanDevice = createDevice,
  .HeadsetGetDisplayTime = timeZero,
  .HeadsetGetDisplayPeriod = timeZero,
  .HeadsetGetDeltaTime = timeZero,
  .HeadsetGetDepthTexture = sceneTexture
};
