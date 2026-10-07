#include "test.h"
#define main graphicsSessionMain
#include "graphics_session.c"
#undef main
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
#include "../../src/modules/headset/headset_openvr.c"
static void checkRuntimeShutdown(void);
#include "openvr_backend.c"
#undef state

bool gpu_texture_get_external_image(gpu_texture* texture, gpu_external_image* image) { (void) texture; (void) image; abort(); }
bool gpu_texture_external_barrier(gpu_stream* stream, gpu_texture* texture, bool begin) { (void) stream; (void) texture; (void) begin; abort(); }

static bool runtimeHideFails, runtimeDestroyFails;
static unsigned runtimeDestroyCalls, runtimeHideCalls, shutdownDrainBaseline;
static Texture* runtimeBacking;
static Pass* runtimePass;

static void checkRuntimeShutdown(void) {
  if (runtimeBacking && (!runtimeBacking->sessionDead || !runtimePass->sessionDead ||
      deferredDestroys || drainCalls <= shutdownDrainBaseline)) abort();
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeCreate(char* key, char* name, VROverlayHandle_t* handle) {
  if (!key || !name) abort();
  *handle = 73; return EVROverlayError_VROverlayError_None;
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeDestroy(VROverlayHandle_t handle) {
  if (handle != 73 || (runtimeBacking && (!lovrTextureIsValid(runtimeBacking) || !lovrPassIsValid(runtimePass)))) abort();
  runtimeDestroyCalls++;
  return runtimeDestroyFails ? EVROverlayError_VROverlayError_RequestFailed : EVROverlayError_VROverlayError_None;
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeHide(VROverlayHandle_t handle) {
  if (handle != 73) abort();
  runtimeHideCalls++;
  return runtimeHideFails ? EVROverlayError_VROverlayError_RequestFailed : EVROverlayError_VROverlayError_None;
}
static EVROverlayError OPENVR_FNTABLE_CALLTYPE runtimeShow(VROverlayHandle_t handle) { (void) handle; return EVROverlayError_VROverlayError_None; }
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
static struct VR_IVROverlay_FnTable runtimeAPI = {
  .CreateOverlay = runtimeCreate, .DestroyOverlay = runtimeDestroy, .HideOverlay = runtimeHide, .ShowOverlay = runtimeShow,
  .SetOverlayFlag = runtimeFlag, .SetOverlayColor = runtimeColor, .SetOverlayAlpha = runtimeFloat,
  .SetOverlaySortOrder = runtimeOrder, .SetOverlayWidthInMeters = runtimeFloat, .SetOverlayTexelAspect = runtimeFloat,
  .SetOverlayCurvature = runtimeFloat, .SetOverlayTextureColorSpace = runtimeSpace, .SetOverlayTextureBounds = runtimeBounds,
  .SetOverlayTransformAbsolute = runtimeAbsolute, .SetOverlayTransformTrackedDeviceRelative = runtimeRelative
};

static bool retainedGraphics(void) {
  runtimeAlive = true;
  textureDestroys = 0;
  prepareFails = false;
  memset(&vrState, 0, sizeof(vrState));
  HeadsetConfig config = { .overlay = true };
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

int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "openvr.backend.retained-graphics", retainedGraphics }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
