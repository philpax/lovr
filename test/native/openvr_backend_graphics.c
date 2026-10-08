#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define canonicalize libcCanonicalize
#include <math.h>
#undef canonicalize
#include "test.h"
#include "../../src/modules/graphics/graphics.c"
#define LOVR_LAYER_GRAPHICS_HARNESS
#define main graphicsSessionMain
#define gpu_copy_textures unused_copy_textures
#define gpu_sync unused_sync
#define gpu_stream_begin unused_stream_begin
#define gpu_stream_end unused_stream_end
#define gpu_submit unused_submit
#define os_get_time unused_get_time
#include "graphics_session.c"
#undef main
#undef gpu_copy_textures
#undef gpu_sync
#undef gpu_stream_begin
#undef gpu_stream_end
#undef gpu_submit
#undef os_get_time
#include "../../src/modules/headset/openvr_frame.c"
#include "headset/openvr_projection.h"
#define result(...) projectionHelperResult(__VA_ARGS__)
#define runtimeResult projectionRuntimeResult
#include "../../src/modules/headset/openvr_projection.c"
#undef runtimeResult
#undef result
#include "headset/openvr_overlay.h"
#define lovrOpenVRPanelCreate testedPanelCreate
#define lovrOpenVRPanelConfigure testedPanelConfigure
#define lovrOpenVRPanelShow testedPanelShow
#define lovrOpenVRPanelHide testedPanelHide
#define lovrOpenVRPanelDestroy testedPanelDestroy
OpenVRPanelResult testedPanelConfigure(OpenVRPanel* panel, const OpenVRPanelConfig* config);
OpenVRPanelResult testedPanelDestroy(OpenVRPanel* panel);
#include "../../src/modules/headset/openvr_overlay.c"
#undef lovrOpenVRPanelCreate
#undef lovrOpenVRPanelConfigure
#undef lovrOpenVRPanelShow
#undef lovrOpenVRPanelHide
#undef lovrOpenVRPanelDestroy
#define LOVR_OPENVR_VALIDATE_CONFIG
#define state vrState
#define LOVR_OPENVR_GRAPHICS_HARNESS
#include "headset/openvr_assets.h"
static OpenVRAssetsResult backendAssetsResolve(bool shared, const OpenVRAssetsProvider* provider);
#define lovrOpenVRAssetsResolve backendAssetsResolve
#include "../../src/modules/headset/headset_openvr.c"
#undef lovrOpenVRAssetsResolve
static void checkRuntimeShutdown(void);
#include "openvr_backend.c"
#undef state

static unsigned textureCreates;
size_t gpu_sizeof_texture(void) { return sizeof(gpu_texture); }
bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info) { (void) info; textureCreates++; *texture = (gpu_texture) { 0 }; return true; }
bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info) {
  if (!runtimeAlive || !info->source || info->source->destroyed) abort();
  *texture = (gpu_texture) { .view = true }; liveViews++; return true;
}
void gpu_texture_destroy(gpu_texture* texture) {
  if (!runtimeAlive || texture->destroyed) abort();
  if (texture->view) liveViews--;
  texture->destroyed = true; textureDestroys++; deferredDestroys++;
}
bool gpu_wait_idle(void) { return !prepareFails; }
bool gpu_prepare_teardown(void) { return !prepareFails; }
bool gpu_quiesce_locked(void) { return !prepareFails; }
void gpu_flush_deferred(void) { deferredDestroys = 0; drainCalls++; }
void gpu_flush_deferred_after_idle(void) { deferredDestroys = 0; drainCalls++; }
bool lovrHeadsetIsActive(void) { return false; }
double lovrHeadsetGetDisplayTime(void) { return 0.; }

static unsigned frameWaits, graphicsEvent, overlayPolls;
static bool graphicsAvailable = true, graphicsPaused;
static bool OPENVR_FNTABLE_CALLTYPE graphicsInputAvailable(void) { return graphicsAvailable; }
static bool OPENVR_FNTABLE_CALLTYPE graphicsPause(void) { return graphicsPaused; }
static bool OPENVR_FNTABLE_CALLTYPE graphicsOverlayPoll(VROverlayHandle_t handle, struct VREvent_t* event, uint32_t size) {
  (void) event; (void) size;
  if (!handle) abort();
  overlayPolls++; return false;
}
static bool OPENVR_FNTABLE_CALLTYPE graphicsPoll(struct VREvent_t* event, uint32_t size) {
  (void) size;
  if (!graphicsEvent) return false;
  *event = (struct VREvent_t) { .eventType = graphicsEvent }; graphicsEvent = 0; return true;
}
static bool invalidTiming, fatalWait;
static float sceneScale = 1.f;
static uint32_t sceneWidth = 64, sceneHeight = 32;
static double frameNow;
static float framePhotons = .003f;
double os_get_time(void) { return frameNow; }
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeWait(uint32_t timeout) {
  if (!timeout || timeout > 100) abort();
  frameWaits++; frameNow += 1. / 90.;
  return fatalWait ? EVROverlayError_VROverlayError_RequestFailed : EVROverlayError_VROverlayError_None;
}
static float OPENVR_FNTABLE_CALLTYPE runtimeProperty(TrackedDeviceIndex_t device, ETrackedDeviceProperty property, ETrackedPropertyError* error) {
  (void) device; *error = ETrackedPropertyError_TrackedProp_Success;
  return property == ETrackedDeviceProperty_Prop_DisplayFrequency_Float ? 90.f : framePhotons;
}
static bool OPENVR_FNTABLE_CALLTYPE runtimeVsync(float* elapsed, uint64_t* counter) { *elapsed = invalidTiming ? NAN : .001f; *counter = 1; return true; }
static void OPENVR_FNTABLE_CALLTYPE runtimeDimensions(uint32_t* width, uint32_t* height) { *width = 64; *height = 32; }
static void OPENVR_FNTABLE_CALLTYPE runtimeTracking(ETrackingUniverseOrigin origin, float prediction, TrackedDevicePose_t* poses, uint32_t count) {
  (void) origin; if (prediction <= 0.f || count != 1) abort();
  *poses = (TrackedDevicePose_t) { .bPoseIsValid = true, .bDeviceIsConnected = true,
    .mDeviceToAbsoluteTracking = { .m = { { 1, 0, 0, 0 }, { 0, 1, 0, 1.5f }, { 0, 0, 1, 0 } } } };
}
static HmdMatrix34_t OPENVR_FNTABLE_CALLTYPE runtimeEye(EVREye eye) {
  return (HmdMatrix34_t) { .m = { { 1, 0, 0, eye ? .03f : -.03f }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } } };
}
static void OPENVR_FNTABLE_CALLTYPE runtimeFrustum(EVREye eye, float* left, float* right, float* top, float* bottom) {
  *left = eye ? -.9f : -1.1f; *right = eye ? 1.2f : .8f; *top = -1.3f; *bottom = .7f;
}
static struct VR_IVRSystem_FnTable runtimeSystem = { .GetFloatTrackedDeviceProperty = runtimeProperty,
  .GetTimeSinceLastVsync = runtimeVsync, .GetRecommendedRenderTargetSize = runtimeDimensions,
  .GetDeviceToAbsoluteTrackingPose = runtimeTracking, .GetEyeToHeadTransform = runtimeEye, .GetProjectionRaw = runtimeFrustum,
  .PollNextEvent = graphicsPoll, .AcknowledgeQuit_Exiting = backendAck,
  .IsInputAvailable = graphicsInputAvailable, .ShouldApplicationPause = graphicsPause };

static unsigned sceneCopies, sceneSubmissions, panelCopies, panelSubmissions;
static uint32_t sceneCopiedEyes[2];
static gpu_texture* sceneCopiedOutputs[2];
static bool traceScene;

void gpu_copy_textures(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint32_t source[4], uint32_t target[4], uint32_t extent[3]) {
  (void) stream;
  if (traceScene && vrState.all && src == vrState.all->texture->gpu && dst == vrState.all->output->gpu) {
    if (source[2] || target[2] || extent[0] != 64 || extent[1] != 32 || extent[2] != 1) abort();
    panelCopies++; return;
  }
  if (!traceScene || sceneCopies >= 2 || src != vrState.scene.texture->gpu || dst != vrState.scene.output[source[2]]->gpu ||
      source[0] || source[1] || source[3] || target[0] || target[1] || target[2] || target[3] ||
      extent[0] != sceneWidth || extent[1] != sceneHeight || extent[2] != 1) abort();
  sceneCopiedEyes[sceneCopies] = source[2];
  sceneCopiedOutputs[sceneCopies++] = dst;
}
void gpu_sync(gpu_stream* stream, gpu_barrier* barriers, uint32_t count) { (void) stream; (void) barriers; (void) count; }
gpu_stream* gpu_stream_begin(const char* label) { (void) label; return (gpu_stream*) 1; }
bool gpu_stream_end(gpu_stream* stream) { return stream != NULL; }
bool gpu_submit(gpu_stream** streams, uint32_t count, uint32_t tick) { (void) streams; (void) count; (void) tick; return true; }
bool gpu_texture_get_external_image(gpu_texture* texture, gpu_external_image* image) {
  if (!traceScene || !texture || texture->view || texture->destroyed) return false;
  *image = (gpu_external_image) { .instance = 1, .physicalDevice = 2, .device = 3, .queue = 4,
    .queueFamily = 5, .image = (uintptr_t) texture,
     .width = texture == (vrState.all ? vrState.all->output->gpu : NULL) ? 64 : sceneWidth,
     .height = texture == (vrState.all ? vrState.all->output->gpu : NULL) ? 32 : sceneHeight, .samples = 1, .format = VK_FORMAT_R8G8B8A8_SRGB };
  return true;
}
bool gpu_texture_external_barrier(gpu_stream* stream, gpu_texture* texture, bool begin) {
  (void) stream; (void) begin; return texture && !texture->destroyed;
}

static bool runtimeHideFails, runtimeDestroyFails;
static unsigned runtimeDestroyCalls, runtimeHideCalls, shutdownDrainBaseline;
static unsigned projectionCreates, failProjectionCreate, runtimeCreates;
static bool projectionMode, failSceneConfigure, failSceneHandoff, failSceneShow, failSceneHide, failPanelHide;
static bool panelVisible, showAttempted;
static unsigned sceneHideCalls;
static HmdMatrix34_t submittedPoses[2];
static VROverlayProjection_t submittedFrusta[2];
static Texture* runtimeBacking;
static Pass* runtimePass;

static void checkRuntimeShutdown(void) {
  if (runtimeBacking && (!runtimeBacking->sessionDead || !runtimePass->sessionDead ||
      deferredDestroys || drainCalls <= shutdownDrainBaseline)) abort();
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeCreate(char* key, char* name, VROverlayHandle_t* handle) {
  if (!key || !name) abort();
  runtimeCreates++;
  if (projectionMode && strstr(key, ".scene.")) {
    if (++projectionCreates == failProjectionCreate) return EVROverlayError_VROverlayError_RequestFailed;
    *handle = 80 + projectionCreates;
  } else *handle = 73;
  return EVROverlayError_VROverlayError_None;
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeDestroy(VROverlayHandle_t handle) {
  if ((!projectionMode && handle != 73) || (runtimeBacking && handle != 73 && (!lovrTextureIsValid(runtimeBacking) || !lovrPassIsValid(runtimePass)))) abort();
  runtimeDestroyCalls++;
  return runtimeDestroyFails ? EVROverlayError_VROverlayError_RequestFailed : EVROverlayError_VROverlayError_None;
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeHide(VROverlayHandle_t handle) {
  if (!projectionMode && handle != 73) abort();
  runtimeHideCalls++;
  if (handle == 73) {
    if (failPanelHide) return EVROverlayError_VROverlayError_InvalidHandle;
    panelVisible = false;
  } else {
    sceneHideCalls++;
    if (failSceneHide && showAttempted && handle == 81) return EVROverlayError_VROverlayError_InvalidHandle;
  }
  return runtimeHideFails ? EVROverlayError_VROverlayError_RequestFailed : EVROverlayError_VROverlayError_None;
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeShow(VROverlayHandle_t handle) {
  if (handle == 73) panelVisible = true;
  if (failSceneShow && handle == 82) { showAttempted = true; return EVROverlayError_VROverlayError_RequestFailed; }
  return EVROverlayError_VROverlayError_None;
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeFlag(VROverlayHandle_t handle, VROverlayFlags flag, bool enabled) {
  (void) handle; (void) flag; (void) enabled; return EVROverlayError_VROverlayError_None;
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeColor(VROverlayHandle_t handle, float r, float g, float b) {
  (void) handle; (void) r; (void) g; (void) b; return EVROverlayError_VROverlayError_None;
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeFloat(VROverlayHandle_t handle, float value) { (void) handle; (void) value; return EVROverlayError_VROverlayError_None; }
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeOrder(VROverlayHandle_t handle, uint32_t value) { (void) handle; (void) value; return EVROverlayError_VROverlayError_None; }
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeSpace(VROverlayHandle_t handle, EColorSpace value) { (void) handle; (void) value; return EVROverlayError_VROverlayError_None; }
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeBounds(VROverlayHandle_t handle, VRTextureBounds_t* value) { (void) handle; (void) value; return EVROverlayError_VROverlayError_None; }
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeAbsolute(VROverlayHandle_t handle, ETrackingUniverseOrigin origin, HmdMatrix34_t* pose) {
  (void) handle; (void) origin; (void) pose; return EVROverlayError_VROverlayError_None;
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeRelative(VROverlayHandle_t handle, TrackedDeviceIndex_t device, HmdMatrix34_t* pose) {
  (void) handle; (void) device; (void) pose; return EVROverlayError_VROverlayError_None;
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeProjection(VROverlayHandle_t handle, ETrackingUniverseOrigin origin,
    HmdMatrix34_t* pose, VROverlayProjection_t* frustum, EVREye eye) {
  if (!projectionMode || handle != 81u + (unsigned) eye || origin != ETrackingUniverseOrigin_TrackingUniverseStanding) abort();
  if (failSceneConfigure) return EVROverlayError_VROverlayError_RequestFailed;
  submittedPoses[eye] = *pose;
  submittedFrusta[eye] = *frustum;
  return EVROverlayError_VROverlayError_None;
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeTexture(VROverlayHandle_t handle, Texture_t* texture) {
  VRVulkanTextureData_t* image = texture->handle;
  if (handle == 73) {
    if (!vrState.all || image->m_nImage != (uintptr_t) vrState.all->output->gpu || !externalHandoff) abort();
    panelSubmissions++; return EVROverlayError_VROverlayError_None;
  }
  unsigned eye = sceneSubmissions++;
  bool unlocked = mtx_trylock(&state.lock) == thrd_success;
  if (unlocked) mtx_unlock(&state.lock);
  if (unlocked || !externalHandoff || eye >= 2 || handle != 81 + eye ||
      texture->eType != ETextureType_TextureType_Vulkan || texture->eColorSpace != EColorSpace_ColorSpace_Gamma ||
      image->m_nImage != (uintptr_t) vrState.scene.output[eye]->gpu || image->m_nWidth != sceneWidth || image->m_nHeight != sceneHeight ||
      image->m_nFormat != VK_FORMAT_R8G8B8A8_SRGB || image->m_nSampleCount != 1 ||
      (uintptr_t) image->m_pInstance != 1 || (uintptr_t) image->m_pPhysicalDevice != 2 ||
      (uintptr_t) image->m_pDevice != 3 || (uintptr_t) image->m_pQueue != 4 || image->m_nQueueFamilyIndex != 5) abort();
  return failSceneHandoff ? EVROverlayError_VROverlayError_RequestFailed : EVROverlayError_VROverlayError_None;
}
static struct VR_IVROverlay_FnTable runtimeAPI = {
  .CreateOverlay = runtimeCreate, .DestroyOverlay = runtimeDestroy, .HideOverlay = runtimeHide, .ShowOverlay = runtimeShow,
  .PollNextOverlayEvent = graphicsOverlayPoll,
  .SetOverlayFlag = runtimeFlag, .SetOverlayColor = runtimeColor, .SetOverlayAlpha = runtimeFloat,
  .SetOverlaySortOrder = runtimeOrder, .SetOverlayWidthInMeters = runtimeFloat, .SetOverlayTexelAspect = runtimeFloat,
  .SetOverlayCurvature = runtimeFloat, .SetOverlayTextureColorSpace = runtimeSpace, .SetOverlayTextureBounds = runtimeBounds,
  .SetOverlayTransformAbsolute = runtimeAbsolute, .SetOverlayTransformTrackedDeviceRelative = runtimeRelative,
  .SetOverlayTransformProjection = runtimeProjection, .SetOverlayTexture = runtimeTexture, .WaitFrameSync = runtimeWait
};

static bool retainedGraphics(void) {
  runtimeAlive = true;
  textureDestroys = 0;
  prepareFails = false;
  memset(&vrState, 0, sizeof(vrState));
  HeadsetConfig config = { .overlay = true, .supersample = 1.f };
  CHECK(init(&config) && connect() && start());
  vrState.runtime.overlay = &runtimeAPI;
  Texture* probe = rootTexture();
  lovrRelease(probe, lovrTextureDestroy);
  state.features.formats[FORMAT_RGBA8][1] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
  LayerInfo layerInfo = { .width = 64, .height = 64, .stereo = true, .filter = true };
  Layer* layer = layerCreate(&layerInfo);
  CHECK(layer);
  Layer* selective = layerCreate(&layerInfo);
  CHECK(selective);
  Texture* selectiveRoot = selective->texture;
  lovrRetain(selectiveRoot);
  prepareFails = true;
  lovrRelease(selective, layerDestroy);
  CHECK(selective->orphan && lovrTextureIsValid(selectiveRoot));
  CHECK(lovrTextureIsValid(layer->texture) && current(layer));
  prepareFails = false;
  CHECK(lovrGraphicsQuiesceSessionResources() && retireLayer(selective));
  CHECK(lovrGraphicsDrainSessionResources());
  CHECK(selectiveRoot->sessionDead && lovrTextureIsValid(layer->texture) && current(layer));
  lovrRelease(selectiveRoot, lovrTextureDestroy);
  selective = layerCreate(&layerInfo);
  CHECK(selective);
  lovrRelease(selective, layerDestroy);
  CHECK(lovrTextureIsValid(layer->texture) && current(layer));
  CHECK(layer->texture->info.type == TEXTURE_ARRAY && layer->texture->info.layers == 2);
  CHECK(layer->output->info.type == TEXTURE_2D && layer->output->info.width == 128);
  CHECK(layer->output->info.srgb && layer->output->info.samples == 1);
  CHECK(layer->properties.textureLayout == OPENVR_PANEL_TEXTURE_SIDE_BY_SIDE_2D);
  CHECK(layer->properties.textureWidth == 128 && layer->properties.viewport[2] == 64);
  CHECK(layer->viewport[2] == 64 && layer->viewport[3] == 64);
  layer->pass = lovrPassCreate("retained OpenVR pass");
  CHECK(layer->pass && lovrGraphicsRegisterSessionPass(layer->pass));
  trackTexture(layer->pass, layer->texture, GPU_PHASE_SHADER_FRAGMENT, GPU_CACHE_TEXTURE);
  Texture* root = layer->texture;
  Pass* pass = layer->pass;
  lovrRetain(root); lovrRetain(pass);
  TextureViewInfo info = { .type = TEXTURE_2D, .layerCount = 1, .levelCount = 1 };
  Texture* view = lovrTextureCreateView(root, &info);
  CHECK(view);
  Texture* nested = lovrTextureCreateView(view, &info);
  CHECK(nested);
  runtimeBacking = root;
  runtimePass = pass;
  shutdownDrainBaseline = drainCalls;
  unsigned shutdownBaseline = shutdowns;
  unsigned destroyBaseline = runtimeDestroyCalls;
  externalHandoff = true;
  CHECK(!disconnect() && connected() && shutdowns == shutdownBaseline);
  CHECK(runtimeDestroyCalls == destroyBaseline && lovrTextureIsValid(root) && lovrPassIsValid(pass));
  externalHandoff = false;
  prepareFails = true;
  CHECK(!disconnect() && connected());
  CHECK(lovrTextureIsValid(root) && lovrPassIsValid(pass));
  prepareFails = false;
  runtimeHideFails = runtimeDestroyFails = true;
  unsigned beforeFailure = textureDestroys;
  unsigned hideBaseline = runtimeHideCalls;
  CHECK(!disconnect() && connected() && layer->panel.handle == 73);
  CHECK(shutdowns == shutdownBaseline && !active() && !generation());
  CHECK(runtimeDestroyCalls > destroyBaseline && runtimeHideCalls > hideBaseline);
  CHECK(!current(layer) && !start());
  CHECK(lovrTextureIsValid(root) && lovrTextureIsValid(layer->output) && lovrPassIsValid(pass));
  CHECK(!view->sessionDead && !nested->sessionDead && textureDestroys == beforeFailure);
  runtimeHideFails = runtimeDestroyFails = false;
  CHECK(disconnect() && shutdowns == shutdownBaseline + 1);
  runtimeBacking = NULL;
  runtimePass = NULL;
  CHECK(root->sessionDead && view->sessionDead && nested->sessionDead && pass->sessionDead);
  CHECK(!checkPassSession(pass) && !lovrTextureIsValid(root));
  CHECK(!sessionTextures && !sessionPasses);
  runtimeAlive = false;
  unsigned destroyed = textureDestroys;
  lovrRelease(layer, layerDestroy);
  lovrRelease(nested, lovrTextureDestroy);
  lovrRelease(view, lovrTextureDestroy);
  lovrRelease(root, lovrTextureDestroy);
  lovrRelease(pass, lovrPassDestroy);
  CHECK(textureDestroys == destroyed);
  runtimeAlive = true;
  return true;
}

static bool panelCanvas(void) {
  runtimeAlive = true;
  prepareFails = runtimeHideFails = runtimeDestroyFails = false;
  memset(&vrState, 0, sizeof(vrState));
  HeadsetConfig config = { .overlay = true, .stencil = true, .antialias = true, .supersample = 1.f };
  CHECK(init(&config) && connect() && start());
  vrState.runtime.overlay = &runtimeAPI;
  Texture* probe = rootTexture();
  lovrRelease(probe, lovrTextureDestroy);
  state.features.sampleCounts = 1 | 4;
  state.features.formats[FORMAT_RGBA8][1] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
  state.features.formats[FORMAT_D24S8][0] = GPU_FEATURE_RENDER;
  LayerInfo info = { .width = 64, .height = 32, .stereo = true, .filter = true, .transparent = true };
  Layer* budget[MAX_LAYERS];
  unsigned createdBefore = runtimeHideCalls;
  for (unsigned i = 0; i < limit(); i++) CHECK((budget[i] = layerCreate(&info)));
  CHECK(runtimeHideCalls == createdBefore + limit());
  unsigned texturesBefore = textureCreates, overlaysBefore = runtimeCreates;
  Layer* registryBefore = vrState.all;
  unsigned refsBefore = atomic_load(&budget[0]->ownership.ref);
  CHECK(!layerCreate(&info) && strstr(lovrGetError(), "panel budget"));
  CHECK(textureCreates == texturesBefore && runtimeCreates == overlaysBefore && vrState.all == registryBefore &&
    atomic_load(&budget[0]->ownership.ref) == refsBefore && runtimeHideCalls == createdBefore + limit());
  overlayPolls = 0;
  CHECK(pollEvents() && overlayPolls == limit());
  vrState.scene.projection.eyes[0] = 81; vrState.scene.projection.eyes[1] = 82;
  overlayPolls = 0;
  CHECK(pollEvents() && overlayPolls == MAX_LAYERS);
  vrState.scene.projection.eyes[0] = vrState.scene.projection.eyes[1] = 0;
  runtimeDestroyFails = true;
  lovrRelease(budget[0], layerDestroy);
  CHECK(vrState.all && !layerCreate(&info));
  overlayPolls = 0;
  CHECK(pollEvents() && overlayPolls == limit());
  runtimeDestroyFails = false;
  Layer* orphan = vrState.all;
  while (orphan && !orphan->orphan) orphan = orphan->next;
  CHECK(orphan && retireLayer(orphan));
  CHECK((budget[0] = layerCreate(&info)));
  lovrRelease(budget[1], layerDestroy);
  overlaysBefore = runtimeCreates;
  CHECK((budget[1] = layerCreate(&info)) && runtimeCreates == overlaysBefore + 1);
  for (unsigned i = 1; i < limit(); i++) lovrRelease(budget[i], layerDestroy);
  Layer* layer = budget[0];
  CHECK(layer);
  Pass* pass = layerPass(layer);
  CHECK(pass && pass->views == 2 && pass->width == 64 && pass->height == 32);
  CHECK(pass->canvas.color[0].texture == layer->texture && pass->canvas.samples == 4);
  CHECK(pass->canvas.depthFormat == FORMAT_D24S8 && pass->tempDepth && pass->tempColor[0]);
  CHECK(pass->target.color[0].resolve == layer->texture->renderView && pass->target.depth.texture);
  CHECK(pass->pipeline->info.depth.test == GPU_COMPARE_GEQUAL && pass->pipeline->info.depth.write);
  LoadAction loads[4] = { 0 }, depthLoad;
  float clears[4][4] = { { 1, 1, 1, 1 } }, depthClear = 1.f;
  lovrPassGetClear(pass, loads, clears, &depthLoad, &depthClear);
  CHECK(loads[0] == LOAD_CLEAR && depthLoad == LOAD_CLEAR && depthClear == 0.f);
  CHECK(clears[0][0] == 0.f && clears[0][1] == 0.f && clears[0][2] == 0.f && clears[0][3] == 0.f);
  float expected[16], observed[16], identity[16] = MAT4_IDENTITY;
  mat4_orthographic(expected, 0, 64, 0, 32, -1.f, 1.f);
  for (unsigned eye = 0; eye < 2; eye++) {
    CHECK(lovrPassGetViewMatrix(pass, eye, observed) && !memcmp(observed, identity, sizeof(observed)));
    CHECK(lovrPassGetProjection(pass, eye, observed) && !memcmp(observed, expected, sizeof(observed)));
  }
  CHECK(layerPass(layer) == pass && pass->views == 2);
  lovrRetain(pass);
  CHECK(disconnect() && pass->sessionDead && !pass->target.depth.texture && !pass->target.color[0].texture);
  lovrRelease(layer, layerDestroy);
  lovrRelease(pass, lovrPassDestroy);
  CHECK(!sessionTextures && !sessionPasses);
  return true;
}

static bool sceneCanvas(void) {
  runtimeAlive = true;
  sceneWidth = (uint32_t) (64 * sceneScale); sceneHeight = (uint32_t) (32 * sceneScale);
  projectionMode = traceScene = true;
  projectionCreates = failProjectionCreate = sceneCopies = sceneSubmissions = 0;
  prepareFails = runtimeHideFails = runtimeDestroyFails = false;
  memset(&vrState, 0, sizeof(vrState));
  HeadsetConfig config = { .overlay = true, .stencil = true, .antialias = true, .supersample = 1.f };
  CHECK(init(&config) && connect() && start());
  vrState.runtime.overlay = &runtimeAPI;
  Texture* probe = rootTexture();
  lovrRelease(probe, lovrTextureDestroy);
  state.features.sampleCounts = 1 | 4;
  state.features.formats[FORMAT_RGBA8][1] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
  state.features.formats[FORMAT_D24S8][0] = GPU_FEATURE_RENDER;
  vrState.config.supersample = sceneScale;
  lovrOpenVRFrameInit(&vrState.frame, &runtimeSystem, &runtimeAPI);
  frameWaits = 0; frameNow = 1.;
  unsigned actionsBefore = actionUpdates;
  CHECK(update() && frameWaits == 1 && vrState.frame.updated);
  CHECK(actionUpdates == actionsBefore + 1 && vrState.inputSerial == vrState.frame.snapshot.serial);
  failActions = true;
  CHECK(vibrate(DEVICE_HAND_LEFT, .5f, 1.f, 120.f));
  CHECK(!update() && !vrState.inputSerial && !vrState.haptics.hands[0].active);
  CHECK(strstr(lovrGetError(), "action snapshot"));
  failActions = false;
  CHECK(update() && vrState.inputSerial == vrState.frame.snapshot.serial);
  invalidTiming = true;
  actionsBefore = actionUpdates;
  CHECK(update() && actionUpdates == actionsBefore && !vrState.inputSerial);
  invalidTiming = false;
  CHECK(update() && vrState.inputSerial == vrState.frame.snapshot.serial);
  activeInputPose = true;
  CHECK(update() && vibrate(DEVICE_HAND_LEFT, .5f, 1.f, 120.f));
  failHaptic = true;
  frameNow += .03;
  CHECK(!update() && !vrState.inputSerial && !vrState.haptics.hands[0].active);
  CHECK(strstr(lovrGetError(), "deferred haptic"));
  failHaptic = false;
  CHECK(update() && vibrate(DEVICE_HAND_LEFT, .5f, 1.f, 120.f));
  activeInputPose = false;
  CHECK(update() && !vrState.haptics.hands[0].active);
  vrState.events.sampled = true;
  vrState.events.inputFocus = false;
  actionsBefore = actionUpdates;
  vrState.haptics.hands[0].active = true;
  CHECK(update() && actionUpdates == actionsBefore + 1 && vrState.inputSerial && !vrState.haptics.hands[0].active);
  vrState.events.inputFocus = true;
  CHECK(update() && actionUpdates == actionsBefore + 2 && vrState.inputSerial);
  unsigned pacedFrames = frameWaits;
  OpenVRFrameSnapshot* frame = &vrState.frame.snapshot;
  frame->origin = ETrackingUniverseOrigin_TrackingUniverseStanding;
  frame->width = 64; frame->height = 32; frame->head.valid = true;
  for (unsigned eye = 0; eye < 2; eye++) {
    frame->eyes[eye].pose.valid = true;
    float eyeToWorld[16] = { 0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, eye ? .03f : -.03f, 1.5f, 2, 1 };
    memcpy(frame->eyes[eye].pose.matrix, eyeToWorld, sizeof(eyeToWorld));
    frame->eyes[eye].transform = (HmdMatrix34_t) { .m = { { 0, 0, 1, eye ? .03f : -.03f }, { 0, 1, 0, 1.5f }, { -1, 0, 0, 2 } } };
    float tangents[4] = { eye ? -.9f : -1.1f, eye ? 1.2f : .8f, -1.3f, .7f };
    memcpy(frame->eyes[eye].tangents, tangents, sizeof(tangents));
  }
  OpenVRFrameSnapshot captured = *frame;
  Pass* pass;
  CHECK(scenePass(&pass) && pass && pass->views == 2 && pass->canvas.samples == 4);
  CHECK(pass->tempDepth && pass->tempColor[0] && pass->canvas.depthFormat == FORMAT_D24S8);
  CHECK(pass->target.color[0].resolve == vrState.scene.texture->renderView);
  CHECK(pass->target.color[0].clear[3] == 0.f && pass->target.depth.clear == 0.f);
  Texture* root;
  CHECK(sceneTexture(&root) && root && root->info.type == TEXTURE_ARRAY && root->info.layers == 2);
  CHECK(root == pass->canvas.color[0].texture && root->info.samples == 1);
  for (unsigned eye = 0; eye < 2; eye++) {
    CHECK(vrState.scene.output[eye]->root == vrState.scene.output[eye]);
    CHECK(vrState.scene.output[eye]->info.type == TEXTURE_2D && vrState.scene.output[eye]->info.layers == 1);
    float expected[16], observed[16];
    mat4_init(expected, frame->eyes[eye].pose.matrix); mat4_invert(expected);
    CHECK(lovrPassGetViewMatrix(pass, eye, observed) && !memcmp(expected, observed, sizeof(expected)));
    CHECK(lovrOpenVRFrameProjection(frame->eyes[eye].tangents, .01f, 0.f, expected));
    CHECK(lovrPassGetProjection(pass, eye, observed) && !memcmp(expected, observed, sizeof(expected)));
    float oracle[16], view[16];
    const float* tangents = captured.eyes[eye].tangents;
    mat4_fov(oracle, -atanf(tangents[0]), atanf(tangents[1]), -atanf(tangents[2]), atanf(tangents[3]), .01f, 0.f);
    for (unsigned i = 0; i < 16; i++) CHECK(fabsf(observed[i] - oracle[i]) < 1e-5f);
    CHECK(lovrPassGetViewMatrix(pass, eye, view));
    for (unsigned corner = 0; corner < 4; corner++) {
      float x = tangents[corner & 1];
      float y = -tangents[2 + (corner >> 1)];
      // A 90-degree yaw maps eye (x, y, -1) to world (tx - 1, 1.5 + y, 2 - x).
      float point[4] = { (eye ? .03f : -.03f) - 1.f, 1.5f + y, 2.f - x, 1.f };
      mat4_mulVec4(view, point);
      CHECK(fabsf(point[0] - x) < 1e-5f && fabsf(point[1] - y) < 1e-5f && fabsf(point[2] + 1.f) < 1e-5f);
      mat4_mulVec4(observed, point);
      CHECK(fabsf(point[0] / point[3] - ((corner & 1) ? 1.f : -1.f)) < 1e-5f);
      CHECK(fabsf(point[1] / point[3] - ((corner >> 1) ? 1.f : -1.f)) < 1e-5f);
    }
  }
  Pass* repeated;
  CHECK(scenePass(&repeated) && repeated == pass && frameWaits == pacedFrames);
  float position[3], orientation[4], left, right, up, down;
  CHECK(viewPose(0, position, orientation) && viewAngles(1, &left, &right, &up, &down));
  uint32_t width, height; dimensions(&width, &height);
  CHECK(width == sceneWidth && height == sceneHeight && pass->width == sceneWidth && pass->height == sceneHeight && frameWaits == pacedFrames && displayTime() > 0.);
  frame->eyes[0].transform.m[0][3] = 99.f;
  mat4_identity(frame->eyes[0].pose.matrix);
  frame->eyes[0].tangents[2] = -.2f;
  CHECK(scenePass(&repeated) && repeated == pass);
  CHECK(!memcmp(vrState.scene.render.eyes, captured.eyes, sizeof(captured.eyes)));
  state.lockReady = mtx_init(&state.lock, mtx_plain) == thrd_success;
  CHECK(state.lockReady);
  state.initialized = true;
  initAllocator(&thread.stack);
  state.stream = (gpu_stream*) 1;
  state.barrier = (gpu_barrier) { 0 };
  LayerInfo panelInfo = { .width = 64, .height = 32, .filter = true, .transparent = true };
  Layer* panel = layerCreate(&panelInfo);
  CHECK(panel && layerPass(panel));
  panelCopies = panelSubmissions = 0;
  CHECK(setLayers(&panel, 1, true) && submit());
  CHECK(panelCopies == 1 && panelSubmissions == 1);
  CHECK(sceneCopies == 2 && sceneSubmissions == 2 && sceneCopiedEyes[0] == 0 && sceneCopiedEyes[1] == 1);
  CHECK(sceneCopiedOutputs[0] != sceneCopiedOutputs[1]);
  for (unsigned eye = 0; eye < 2; eye++) {
    // The overlay receives the view matrix that renders the eye, and the frustum with its downward edge in fTop.
    float renderedView[16];
    mat4_init(renderedView, captured.eyes[eye].pose.matrix);
    mat4_invert(renderedView);
    for (unsigned row = 0; row < 3; row++) {
      for (unsigned column = 0; column < 4; column++) {
        CHECK(fabsf(submittedPoses[eye].m[row][column] - renderedView[column * 4 + row]) < 1e-5f);
      }
    }
    CHECK(submittedFrusta[eye].fTop == -captured.eyes[eye].tangents[3] &&
      submittedFrusta[eye].fBottom == -captured.eyes[eye].tangents[2]);
  }
  CHECK(submittedFrusta[0].fLeft == -1.1f && submittedFrusta[1].fRight == 1.2f);
  CHECK(vrState.scene.projection.visible[0] && vrState.scene.projection.visible[1]);
  CHECK(panelVisible);
  CHECK(setLayers(NULL, 0, true));
  failSceneConfigure = true;
  CHECK(!submit() && !panelVisible && strstr(lovrGetError(), "SetOverlayTransformProjection"));
  failSceneConfigure = false;
  sceneCopies = sceneSubmissions = 0;
  CHECK(setLayers(&panel, 1, true) && submit() && panelVisible);
  CHECK(setLayers(NULL, 0, true));
  failSceneHandoff = true; failPanelHide = true;
  sceneCopies = sceneSubmissions = 0;
  CHECK(!submit() && panelVisible && strstr(lovrGetError(), "SetOverlayTexture") && strstr(lovrGetError(), "omitted panel"));
  failPanelHide = false;
  sceneCopies = sceneSubmissions = 0;
  CHECK(!submit() && !panelVisible);
  failSceneHandoff = false;
  failSceneShow = true;
  unsigned hidesBefore = sceneHideCalls;
  sceneCopies = sceneSubmissions = 0;
  CHECK(!submit() && sceneHideCalls == hidesBefore + 4);
  CHECK(!vrState.scene.projection.visible[0] && !vrState.scene.projection.visible[1]);
  CHECK(lovrTextureIsValid(root) && lovrPassIsValid(pass));
  failSceneHide = true; showAttempted = false;
  sceneCopies = sceneSubmissions = 0;
  CHECK(!submit() && strstr(lovrGetError(), "ShowOverlay") && strstr(lovrGetError(), "HideOverlay"));
  CHECK(lovrTextureIsValid(root) && lovrPassIsValid(pass));
  failSceneShow = failSceneHide = showAttempted = false;
  sceneCopies = sceneSubmissions = 0;
  CHECK(submit());
  invalidTiming = true;
  CHECK(update() && !vrState.scene.projection.visible[0] && !vrState.scene.projection.visible[1]);
  CHECK(scenePass(&repeated) && !repeated && !viewPose(0, position, orientation));
  left = right = up = down = 9.f;
  CHECK(!viewAngles(0, &left, &right, &up, &down));
  CHECK(left == 0.f && right == 0.f && up == 0.f && down == 0.f);
  CHECK(displayTime() == 0. && deltaTime() == 0. && submit());
  CHECK(setLayers(&panel, 1, true) && submit() && panelVisible);
  CHECK(setLayers(NULL, 0, true) && submit() && !panelVisible);
  runtimeHideFails = true;
  CHECK(!update() && strstr(lovrGetError(), "HideOverlay"));
  runtimeHideFails = false;
  invalidTiming = false;
  sceneCopies = sceneSubmissions = 0;
  CHECK(update() && scenePass(&repeated) && repeated == pass && submit());
  CHECK(vrState.scene.projection.visible[0] && vrState.scene.projection.visible[1]);
  for (unsigned failure = 0; failure < 2; failure++) {
    activeInputPose = true;
    CHECK(update() && vibrate(DEVICE_HAND_LEFT, .5f, 1.f, 120.f));
    sceneCopies = sceneSubmissions = 0;
    CHECK(scenePass(&repeated) && repeated && submit());
    unsigned hiddenBefore = sceneHideCalls;
    failActions = failure == 0; failHaptic = failure == 1;
    frameNow += .03;
    CHECK(!update() && sceneHideCalls == hiddenBefore + 2);
    CHECK(vrState.frame.updated && !vrState.frame.snapshot.head.valid && !vrState.inputSerial);
    CHECK(!vrState.scene.captured && !vrState.scene.prepared && !vrState.scene.requested);
    CHECK(!pose(DEVICE_HEAD, position, orientation) && orientation[3] == 0.f);
    CHECK(!viewPose(0, position, orientation) && !viewPose(1, position, orientation));
    CHECK(!vrState.scene.projection.visible[0] && !vrState.scene.projection.visible[1]);
    runtimeHideFails = true;
    if (failure) {
      failHaptic = false; CHECK(update() && vibrate(DEVICE_HAND_LEFT, .5f, 1.f, 120.f));
      failHaptic = true; frameNow += .03;
    }
    CHECK(!update() && strstr(lovrGetError(), failure ? "deferred haptic" : "action snapshot") &&
      strstr(lovrGetError(), "HideOverlay"));
    runtimeHideFails = failActions = failHaptic = false;
    sceneCopies = sceneSubmissions = 0;
    CHECK(update() && scenePass(&repeated) && repeated && submit());
  }
  vrState.runtime.system = &runtimeSystem;
  unsigned recenterHides = sceneHideCalls;
  graphicsEvent = EVREventType_VREvent_SeatedZeroPoseReset;
  CHECK(pollEvents() && sceneHideCalls == recenterHides && vrState.frame.updated && vrState.inputSerial &&
    vrState.scene.projection.visible[0] && vrState.scene.projection.visible[1]);
  graphicsEvent = EVREventType_VREvent_StandingZeroPoseReset;
  CHECK(pollEvents() && sceneHideCalls == recenterHides + 2 && vrState.frame.updated && !vrState.frame.snapshot.head.valid);
  CHECK(!vrState.scene.projection.visible[0] && !vrState.scene.projection.visible[1]);
  CHECK(!viewPose(0, position, orientation) && !pose(DEVICE_HEAD, position, orientation));
  runtimeHideFails = true;
  graphicsEvent = EVREventType_VREvent_StandingZeroPoseReset;
  CHECK(!pollEvents() && strstr(lovrGetError(), "HideOverlay"));
  runtimeHideFails = false;
  vrState.events.inputFocus = true;
  sceneCopies = sceneSubmissions = 0;
  CHECK(update() && scenePass(&repeated) && repeated && submit());
  framePhotons = .1f;
  sceneCopies = sceneSubmissions = 0;
  CHECK(update() && scenePass(&repeated) && repeated && submit());
  framePhotons = .003f;
  CHECK(update() && displayTime() == 0. && deltaTime() == 0.);
  CHECK(!vrState.scene.projection.visible[0] && !vrState.scene.projection.visible[1]);
  CHECK(scenePass(&repeated) && !repeated && !viewPose(0, position, orientation) &&
    !viewAngles(0, &left, &right, &up, &down) && submit());
  CHECK(left == 0.f && right == 0.f && up == 0.f && down == 0.f);
  frameNow += .2;
  sceneCopies = sceneSubmissions = 0;
  CHECK(update() && scenePass(&repeated) && repeated && submit());
  CHECK(vrState.scene.projection.visible[0] && vrState.scene.projection.visible[1]);
  fatalWait = true;
  CHECK(!update() && !vrState.scene.projection.visible[0] && !vrState.scene.projection.visible[1]);
  CHECK(strstr(lovrGetError(), "frame update"));
  runtimeHideFails = true;
  CHECK(!update() && strstr(lovrGetError(), "frame update") && strstr(lovrGetError(), "HideOverlay"));
  runtimeHideFails = false;
  fatalWait = false;
  sceneCopies = sceneSubmissions = 0;
  CHECK(update() && scenePass(&repeated) && repeated && submit());
  CHECK(setLayers(NULL, 0, false) && submit());
  CHECK(!vrState.scene.projection.visible[0] && !vrState.scene.projection.visible[1]);
  CHECK(sceneSubmissions == 2);
  lovrRetain(root); lovrRetain(pass);
  TextureViewInfo viewInfo = { .type = TEXTURE_2D, .layerCount = 1, .levelCount = 1 };
  Texture* view = lovrTextureCreateView(root, &viewInfo);
  Texture* nested = lovrTextureCreateView(view, &viewInfo);
  CHECK(view && nested);
  runtimeBacking = root; runtimePass = pass;
  shutdownDrainBaseline = drainCalls;
  unsigned shutdownBaseline = shutdowns;
  runtimeHideFails = runtimeDestroyFails = true;
  CHECK(!disconnect() && connected() && !active() && shutdowns == shutdownBaseline);
  CHECK(!root->sessionDead && !pass->sessionDead && !nested->sessionDead);
  runtimeHideFails = false;
  CHECK(!disconnect() && connected() && !root->sessionDead);
  runtimeDestroyFails = false;
  CHECK(disconnect() && shutdowns == shutdownBaseline + 1 && root->sessionDead && pass->sessionDead && nested->sessionDead);
  runtimeBacking = NULL; runtimePass = NULL;
  lovrRelease(panel, layerDestroy);
  lovrRelease(nested, lovrTextureDestroy); lovrRelease(view, lovrTextureDestroy);
  lovrRelease(root, lovrTextureDestroy); lovrRelease(pass, lovrPassDestroy);
  CHECK(!sessionTextures && !sessionPasses && !deferredDestroys);
  uint32_t oldGeneration = nextGeneration;
  CHECK(connect() && start() && generation() > oldGeneration && vrState.main);
  vrState.runtime.overlay = &runtimeAPI;
  vrState.frame.updated = true;
  vrState.frame.snapshot = (OpenVRFrameSnapshot) { .origin = ETrackingUniverseOrigin_TrackingUniverseStanding,
    .width = 64, .height = 32, .head.valid = true };
  for (unsigned eye = 0; eye < 2; eye++) {
    vrState.frame.snapshot.eyes[eye] = vrState.scene.render.eyes[eye];
    vrState.frame.snapshot.eyes[eye].pose.valid = true;
    mat4_identity(vrState.frame.snapshot.eyes[eye].pose.matrix);
    vrState.frame.snapshot.eyes[eye].transform = (HmdMatrix34_t) { .m = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } } };
    float tangents[4] = { -1, 1, -1, 1 };
    memcpy(vrState.frame.snapshot.eyes[eye].tangents, tangents, sizeof(tangents));
  }
  CHECK(scenePass(&pass) && pass && !pass->sessionDead);
  CHECK(sceneTexture(&root) && root && !root->sessionDead);
  projectionCreates = 0;
  failProjectionCreate = 2;
  runtimeDestroyFails = true;
  CHECK(!submit() && vrState.scene.projection.eyes[0] && !vrState.scene.projection.eyes[1]);
  CHECK(!disconnect() && connected() && lovrTextureIsValid(root) && lovrPassIsValid(pass));
  runtimeDestroyFails = false;
  CHECK(disconnect() && !vrState.scene.texture && !vrState.scene.projection.eyes[0]);
  CHECK(!sessionTextures && !sessionPasses && !deferredDestroys);
  state.initialized = state.lockReady = false;
  mtx_destroy(&state.lock);
  lovrFree(thread.stack.memory);
  thread.stack = (Allocator) { 0 };
  projectionMode = traceScene = false;
  return true;
}

static bool inputCleared(void) {
  bool value = true, changed = true;
  float values[2] = { 9.f, 9.f }, p[3], q[4];
  CHECK(!down(DEVICE_HAND_LEFT, BUTTON_TRIGGER, &value, &changed) && !value && !changed);
  CHECK(!touched(DEVICE_HAND_LEFT, BUTTON_TRIGGER, &value) && !value);
  CHECK(!axis(DEVICE_HAND_LEFT, AXIS_THUMBSTICK, values) && values[0] == 0.f && values[1] == 0.f);
  CHECK(!axis(DEVICE_HAND_LEFT, AXIS_TRIGGER, values) && values[0] == 0.f);
  CHECK(!pose(DEVICE_HAND_LEFT, p, q) && q[0] == 0.f && q[3] == 0.f);
  return true;
}
static bool inputTransitions(void) {
  memset(&vrState, 0, sizeof(vrState));
  HeadsetConfig config = { .overlay = true, .supersample = 1.f };
  CHECK(init(&config) && connect() && start());
  vrState.runtime.system = &runtimeSystem;
  vrState.runtime.overlay = &runtimeAPI;
  lovrOpenVRFrameInit(&vrState.frame, &runtimeSystem, &runtimeAPI);
  frameNow = 1.;
  activeInputPose = inputActive = inputDown = inputChanged = true;
  for (unsigned transition = 0; transition < 3; transition++) {
    inputDown = transition != 2; inputChanged = transition != 1;
    unsigned before = actionUpdates, digitalBefore = digitalReads, analogBefore = analogReads, poseBefore = poseReads;
    CHECK(update() && actionUpdates == before + 1);
    CHECK(digitalReads == digitalBefore + 4 * OPENVR_INPUT_BUTTON_COUNT &&
      analogReads == analogBefore + 2 * OPENVR_INPUT_AXIS_COUNT && poseReads == poseBefore + 4);
    inputDown = !inputDown; inputChanged = !inputChanged;
    for (unsigned repeat = 0; repeat < 3; repeat++) {
      bool value, changed;
      float values[2];
      CHECK(down(DEVICE_HAND_LEFT, BUTTON_TRIGGER, &value, &changed) && value == (transition != 2) && changed == (transition != 1));
      CHECK(touched(DEVICE_HAND_RIGHT, BUTTON_TRIGGER, &value) && value == (transition != 2));
      CHECK(axis(DEVICE_HAND_LEFT, AXIS_THUMBSTICK, values) && values[0] == .25f && values[1] == -.75f);
      CHECK(axis(DEVICE_HAND_RIGHT, AXIS_TRIGGER, values) && values[0] == .25f);
      CHECK(actionUpdates == before + 1 && digitalReads == digitalBefore + 4 * OPENVR_INPUT_BUTTON_COUNT &&
        analogReads == analogBefore + 2 * OPENVR_INPUT_AXIS_COUNT && poseReads == poseBefore + 4);
    }
  }
  for (unsigned reason = 0; reason < 2; reason++) {
    graphicsAvailable = reason != 0;
    graphicsPaused = reason == 1;
    CHECK(vibrate(DEVICE_HAND_LEFT, .5f, 1.f, 120.f));
    unsigned before = actionUpdates, poseBefore = poseReads;
    uint64_t serial = vrState.inputSerial;
    CHECK(pollEvents() && !focused() && !vrState.haptics.hands[0].active && vrState.inputSerial == serial);
    CHECK(!vrState.input.hands[0].buttons[0].bActive && !vrState.input.hands[0].touches[0].bActive &&
      !vrState.input.hands[0].axes[0].bActive);
    vrState.input.hands[0].buttons[0] = (InputDigitalActionData_t) { .bActive = true, .bState = true, .bChanged = true };
    vrState.input.hands[0].touches[0] = vrState.input.hands[0].buttons[0];
    vrState.input.hands[0].axes[0] = (InputAnalogActionData_t) { .bActive = true, .x = 1.f, .y = 1.f };
    for (unsigned repeat = 0; repeat < 3; repeat++) {
      bool value = true, changed = true;
      float values[2] = { 9.f, 9.f }, p[3], q[4], linear[3], angular[3];
      CHECK(pose(DEVICE_HAND_LEFT_GRIP, p, q) && q[3] == 1.f);
      CHECK(pose(DEVICE_HAND_RIGHT_POINT, p, q) && q[3] == 1.f);
      CHECK(velocity(DEVICE_HAND_LEFT, linear, angular));
      CHECK(!down(DEVICE_HAND_LEFT, BUTTON_TRIGGER, &value, &changed) && !value && !changed);
      CHECK(!touched(DEVICE_HAND_LEFT, BUTTON_TRIGGER, &value) && !value);
      CHECK(!axis(DEVICE_HAND_LEFT, AXIS_THUMBSTICK, values) && values[0] == 0.f && values[1] == 0.f);
      CHECK(!vibrate(DEVICE_HAND_LEFT, .5f, 1.f, 120.f));
      CHECK(actionUpdates == before && poseReads == poseBefore);
    }
    failHaptic = true;
    vrState.haptics.hands[0].active = true;
    CHECK(update() && actionUpdates == before + 1 && poseReads == poseBefore + 4 &&
      vrState.inputSerial == vrState.frame.snapshot.serial && !vrState.haptics.hands[0].active);
    CHECK(!vrState.input.hands[0].buttons[0].bActive && !vrState.input.hands[0].axes[0].bActive);
    float p[3], q[4];
    CHECK(pose(DEVICE_HAND_LEFT, p, q));
    CHECK(pollEvents() && pose(DEVICE_HAND_LEFT, p, q) && actionUpdates == before + 1 && poseReads == poseBefore + 4);
    failHaptic = false;
    failActions = true;
    CHECK(!update() && inputCleared());
    failActions = false;
    CHECK(update() && pose(DEVICE_HAND_LEFT, p, q));
    graphicsEvent = EVREventType_VREvent_StandingZeroPoseReset;
    CHECK(pollEvents() && inputCleared() && !vrState.frame.snapshot.head.valid);
    CHECK(update() && pose(DEVICE_HAND_LEFT, p, q));
    graphicsAvailable = true; graphicsPaused = false;
    CHECK(pollEvents() && focused());
    bool value, changed;
    CHECK(!down(DEVICE_HAND_LEFT, BUTTON_TRIGGER, &value, &changed));
    CHECK(update());
  }
  failActions = true;
  CHECK(!update() && inputCleared());
  double acceptedNow = vrState.frame.lastNow, acceptedEpoch = vrState.frame.epoch;
  double acceptedDisplayTime = vrState.frame.lastDisplayTime;
  CHECK(vrState.frame.updated && displayTime() == 0. && deltaTime() == 0.);
  failActions = false;
  frameNow = acceptedNow - .02;
  unsigned beforeRegression = actionUpdates;
  CHECK(update() && inputCleared() && actionUpdates == beforeRegression);
  CHECK(vrState.frame.lastNow == acceptedNow && vrState.frame.epoch == acceptedEpoch &&
    vrState.frame.lastDisplayTime == acceptedDisplayTime);
  frameNow = acceptedNow;
  CHECK(update() && actionUpdates == beforeRegression + 1 && vrState.inputSerial &&
    vrState.frame.snapshot.head.valid && vrState.frame.lastNow > acceptedNow && vrState.frame.epoch == acceptedEpoch);
  graphicsEvent = EVREventType_VREvent_StandingZeroPoseReset;
  acceptedNow = vrState.frame.lastNow; acceptedEpoch = vrState.frame.epoch;
  acceptedDisplayTime = vrState.frame.lastDisplayTime;
  CHECK(pollEvents() && inputCleared() && vrState.frame.updated);
  CHECK(vrState.frame.lastNow == acceptedNow && vrState.frame.epoch == acceptedEpoch &&
    vrState.frame.lastDisplayTime == acceptedDisplayTime);
  frameNow = acceptedNow - .02;
  beforeRegression = actionUpdates;
  CHECK(update() && inputCleared() && actionUpdates == beforeRegression && vrState.frame.lastNow == acceptedNow);
  frameNow = acceptedNow;
  vrState.events.inputFocus = true;
  CHECK(update());
  stop();
  CHECK(inputCleared() && start() && inputCleared());
  CHECK(disconnect());
  activeInputPose = inputActive = inputDown = inputChanged = false;
  return true;
}

static bool scaledScene(void) {
  sceneScale = 1.5f;
  bool ok = sceneCanvas();
  if (ok) { sceneScale = .75f; ok = sceneCanvas(); }
  sceneScale = 1.f;
  return ok;
}

int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "openvr.backend.input-transitions", inputTransitions },
    { "openvr.backend.retained-graphics", retainedGraphics },
    { "openvr.backend.panel-canvas", panelCanvas },
    { "openvr.backend.scene-canvas", sceneCanvas },
    { "openvr.backend.scaled-scene", scaledScene }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
