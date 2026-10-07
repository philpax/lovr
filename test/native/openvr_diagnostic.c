#define LOVR_ENABLE_OPENVR_DIAGNOSTIC 1
#define main backendMain
#include "openvr_backend.c"
#undef main

static VROverlayHandle_t outputHandle;
static EVROverlayError createError, destroyError, textureError;
static unsigned delegatedCreates, delegatedDestroys, delegatedTextures;

static EVROverlayError OPENVR_FNTABLE_CALLTYPE trackedCreate(char* key, char* name, VROverlayHandle_t* handle) {
  (void) key;
  (void) name;
  delegatedCreates++;
  *handle = outputHandle;
  return createError;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE trackedDestroy(VROverlayHandle_t handle) {
  (void) handle;
  delegatedDestroys++;
  return destroyError;
}

static EVROverlayError OPENVR_FNTABLE_CALLTYPE trackedTexture(VROverlayHandle_t handle, Texture_t* texture) {
  (void) handle;
  (void) texture;
  delegatedTextures++;
  return textureError;
}

static void resetTracking(void) {
  memset(&state, 0, sizeof(state));
  memset(&diagnostic, 0, sizeof(diagnostic));
  diagnostic.original.CreateOverlay = trackedCreate;
  diagnostic.original.DestroyOverlay = trackedDestroy;
  diagnostic.original.SetOverlayTexture = trackedTexture;
  outputHandle = 17;
  createError = destroyError = textureError = 0;
  delegatedCreates = delegatedDestroys = delegatedTextures = 0;
}

static uint32_t sceneSamples;

static uint32_t OPENVR_FNTABLE_CALLTYPE trackedScenePID(void) {
  sceneSamples++;
  return 419;
}

static bool connectedOracle(void) {
  resetTracking();
  sceneSamples = 0;
  struct VR_IVRApplications_FnTable applications = { .GetCurrentSceneProcessId = trackedScenePID };
  state.runtime.initialized = true;
  state.runtime.applications = &applications;
  state.runtime.overlay = &diagnostic.original;
  diagnosticDelegateOverlay();
  CHECK(state.runtime.overlay == &diagnostic.delegated);
  diagnosticDelegateOverlay();
  VROverlayHandle_t handle = 0;
  CHECK(!state.runtime.overlay->CreateOverlay(NULL, NULL, &handle));
  Texture_t texture = { 0 };
  CHECK(!state.runtime.overlay->SetOverlayTexture(handle, &texture));
  state.generation = 7;
  OpenVRDiagnosticObservation observation;
  CHECK(lovrOpenVRDiagnosticObserve(&observation) && observation.currentScenePID == 419 && observation.generation == 7);
  state.generation = 0;
  state.stopping = true;
  destroyError = EVROverlayError_VROverlayError_RequestFailed;
  CHECK(state.runtime.overlay->DestroyOverlay(handle) == destroyError);
  CHECK(lovrOpenVRDiagnosticObserve(&observation) && observation.cleanupPending && observation.liveOverlayHandles == 1);
  destroyError = 0;
  CHECK(!state.runtime.overlay->DestroyOverlay(handle));
  state.stopping = false;
  state.generation = 8;
  CHECK(lovrOpenVRDiagnosticObserve(&observation) && observation.retiredOverlayHandles == 1 && observation.setOverlayTextureSuccesses == 1);
  CHECK(sceneSamples == 3 && delegatedCreates == 1 && delegatedDestroys == 2 && delegatedTextures == 1);
  state.runtime.applications = NULL;
  CHECK(!lovrOpenVRDiagnosticObserve(&observation));
  state.runtime.initialized = false;
  CHECK(lovrOpenVRDiagnosticObserve(&observation) && !observation.currentScenePID);
  return true;
}

static bool identity(void) {
  resetTracking();
  VROverlayHandle_t handle = 0;
  createError = EVROverlayError_VROverlayError_RequestFailed;
  CHECK(diagnosticCreateOverlay(NULL, NULL, &handle) == createError);
  OpenVRDiagnosticObservation observation;
  CHECK(lovrOpenVRDiagnosticObserve(&observation));
  CHECK(!observation.connected && !observation.currentScenePID && observation.liveOverlayHandles == 1);
  destroyError = EVROverlayError_VROverlayError_PermissionDenied;
  CHECK(diagnosticDestroyOverlay(handle) == destroyError);
  CHECK(lovrOpenVRDiagnosticObserve(&observation) && observation.liveOverlayHandles == 1 && !observation.retiredOverlayHandles);
  destroyError = 0;
  CHECK(!diagnosticDestroyOverlay(handle));
  CHECK(lovrOpenVRDiagnosticObserve(&observation) && !observation.liveOverlayHandles && observation.retiredOverlayHandles == 1);
  CHECK(!diagnosticDestroyOverlay(handle));
  CHECK(!lovrOpenVRDiagnosticObserve(&observation));
  CHECK(!diagnostic.liveOverlayHandles && diagnostic.retiredOverlayHandles == 1);
  CHECK(delegatedCreates == 1 && delegatedDestroys == 3);
  resetTracking();
  outputHandle = 0;
  CHECK(!diagnosticCreateOverlay(NULL, NULL, &handle));
  CHECK(!lovrOpenVRDiagnosticObserve(&observation));
  resetTracking();
  CHECK(!diagnosticCreateOverlay(NULL, NULL, &handle));
  CHECK(!diagnosticCreateOverlay(NULL, NULL, &handle));
  CHECK(!lovrOpenVRDiagnosticObserve(&observation) && diagnostic.liveOverlayHandles == 1);
  return true;
}

static bool exhaustion(void) {
  resetTracking();
  VROverlayHandle_t handle;
  for (uint32_t i = 0; i < MAX_LAYERS; i++) {
    outputHandle = i + 1;
    CHECK(!diagnosticCreateOverlay(NULL, NULL, &handle));
  }
  OpenVRDiagnosticObservation observation;
  CHECK(lovrOpenVRDiagnosticObserve(&observation) && observation.liveOverlayHandles == MAX_LAYERS);
  outputHandle = MAX_LAYERS + 1;
  CHECK(!diagnosticCreateOverlay(NULL, NULL, &handle));
  CHECK(!lovrOpenVRDiagnosticObserve(&observation));
  CHECK(delegatedCreates == MAX_LAYERS + 1);
  resetTracking();
  CHECK(!diagnosticCreateOverlay(NULL, NULL, &handle));
  diagnostic.retiredOverlayHandles = UINT64_MAX;
  CHECK(!diagnosticDestroyOverlay(handle));
  CHECK(!lovrOpenVRDiagnosticObserve(&observation) && diagnostic.retiredOverlayHandles == UINT64_MAX);
  return true;
}

static bool textureCounters(void) {
  resetTracking();
  Texture_t texture = { 0 };
  textureError = EVROverlayError_VROverlayError_RequestFailed;
  CHECK(diagnosticSetOverlayTexture(17, &texture) == textureError);
  textureError = 0;
  CHECK(!diagnosticSetOverlayTexture(17, &texture));
  OpenVRDiagnosticObservation observation;
  CHECK(lovrOpenVRDiagnosticObserve(&observation));
  CHECK(observation.setOverlayTextureCalls == 2 && observation.setOverlayTextureSuccesses == 1 && !observation.lastSetOverlayTextureError);
  diagnostic.setOverlayTextureCalls = UINT64_MAX;
  CHECK(!diagnosticSetOverlayTexture(17, &texture));
  CHECK(!lovrOpenVRDiagnosticObserve(&observation) && delegatedTextures == 3);
  resetTracking();
  diagnostic.setOverlayTextureSuccesses = UINT64_MAX;
  CHECK(!diagnosticSetOverlayTexture(17, &texture));
  CHECK(!lovrOpenVRDiagnosticObserve(&observation) && diagnostic.setOverlayTextureSuccesses == UINT64_MAX);
  return true;
}

int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "openvr.diagnostic.connected-oracle", connectedOracle },
    { "openvr.diagnostic.identity", identity },
    { "openvr.diagnostic.exhaustion", exhaustion },
    { "openvr.diagnostic.texture-counters", textureCounters }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
