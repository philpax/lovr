#include "test.h"
#include "../../src/modules/headset/headset_openxr.c"

struct ModelMetadata* lovrModelGetMetadata(Model* model) { abort(); }
uintptr_t gpu_vk_get_physical_device(void) { abort(); }
void os_sleep(double seconds) { abort(); }
const DataField* lovrMeshGetVertexFormat(Mesh* mesh) { abort(); }
Mesh* lovrMeshCreate(const MeshInfo* info, void** data) { abort(); }
void* lovrMeshSetVertices(Mesh* mesh, uint32_t index, uint32_t count) { abort(); }
void* lovrMeshSetIndices(Mesh* mesh, uint32_t count, DataType type) { abort(); }
bool lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count) { abort(); }
void lovrSystemGetWindowSize(uint32_t* width, uint32_t* height) { abort(); }
void lovrModelDataAllocate(ModelData* model) { abort(); }
bool lovrModelDataFinalize(ModelData* model) { abort(); }
Blob* lovrBlobCreate(void* data, size_t size, const char* name) { abort(); }
ModelData* lovrModelDataCreate(struct Blob* blob, ModelDataIO* io) { abort(); }
void lovrBlobDestroy(void* ref) { abort(); }
void lovrEventPush(Event event) { abort(); }
void lovrModelResetNodeTransforms(Model* model) { abort(); }
void lovrModelSetNodeTransform(Model* model, uint32_t node, float* position, float* scale, float* rotation, float alpha) { abort(); }
void lovrModelSetNodeVisible(Model* model, uint32_t node, bool visible) { abort(); }
const TextureInfo* lovrTextureGetInfo(Texture* texture) { abort(); }
float lovrMathLinearToGamma(float x) { abort(); }
float lovrMathGammaToLinear(float x) { abort(); }
bool lovrGraphicsIsInitialized(void) { abort(); }
uintptr_t gpu_vk_get_instance(void) { abort(); }
uintptr_t gpu_vk_get_device(void) { abort(); }
uintptr_t gpu_vk_get_queue(uint32_t* queueFamilyIndex, uint32_t* queueIndex) { abort(); }
uint32_t lovrGraphicsGetFormatSupport(uint32_t format, uint32_t features) { abort(); }
void lovrGraphicsGetFeatures(GraphicsFeatures* features) { abort(); }
void lovrGraphicsGetBackgroundColor(float background[4]) { abort(); }
Shader* lovrGraphicsGetDefaultShader(DefaultShader type) { abort(); }
bool lovrPassPush(Pass* pass, StackType stack) { abort(); }
void lovrPassSetShader(Pass* pass, Shader* shader) { abort(); }
void lovrPassSetColor(Pass* pass, float color[4]) { abort(); }
bool lovrPassDrawMesh(Pass* pass, Mesh* mesh, float* transform, uint32_t instances) { abort(); }
bool lovrPassPop(Pass* pass, StackType stack) { abort(); }
void lovrPassSetViewport(Pass* pass, float viewport[6]) { abort(); }
bool lovrPassSetScissor(Pass* pass, uint32_t scissor[4]) { abort(); }

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(const char* name, uint32_t capacity, uint32_t* count, XrExtensionProperties* properties) { abort(); }
XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(const XrInstanceCreateInfo* info, XrInstance* instance) { abort(); }
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) { abort(); }

static bool prepareSucceeds = true;
static bool prepared, retired;
static bool failEnumerate, failSwapchainDestroy, failSessionDestroy;
static bool failAcquire, failRelease;
static XrResult waitResult = XR_SUCCESS;
static unsigned preparations, flushes;
bool gpu_prepare_teardown(void) { preparations++; prepared = prepareSucceeds; return prepareSucceeds; }
void gpu_flush_deferred_after_idle(void) { if (!prepared) abort(); retired = false; flushes++; }

static unsigned registeredTextures, registeredPasses, invalidations;
bool lovrGraphicsRegisterSessionTexture(Texture* texture) { if (!texture) abort(); registeredTextures++; return true; }
bool lovrGraphicsRegisterSessionPass(Pass* pass) { if (!pass) abort(); registeredPasses++; return true; }
void lovrGraphicsInvalidateSessionResources(void) { if (!prepared) abort(); retired = true; invalidations++; }
void lovrGraphicsInvalidateSessionTexture(Texture* texture) { if (texture) { if (!prepared) abort(); retired = true; } }
void lovrGraphicsInvalidateSessionPass(Pass* pass) { if (pass) { if (!prepared) abort(); retired = true; } }
bool lovrGraphicsPrepareSessionTeardown(void) {
  if (!gpu_prepare_teardown()) return false;
  lovrGraphicsInvalidateSessionResources();
  gpu_flush_deferred_after_idle();
  return true;
}

static unsigned created, destroyed, acquired;

static bool runtimeLive;

static XrResult XRAPI_CALL fakeCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* info, XrSwapchain* handle) {
  (void) session;
  (void) info;
  *handle = (XrSwapchain) (uintptr_t) ++created;
  return XR_SUCCESS;
}

static XrResult XRAPI_CALL fakeEnumerateImages(XrSwapchain handle, uint32_t capacity, uint32_t* count, XrSwapchainImageBaseHeader* images) {
  (void) handle;
  (void) capacity;
  if (failEnumerate) return XR_ERROR_RUNTIME_FAILURE;
  *count = 1;
  ((XrSwapchainImageVulkanKHR*) images)->image = (VkImage) (uintptr_t) 1;
  return XR_SUCCESS;
}

static XrResult XRAPI_CALL fakeAcquire(XrSwapchain handle, const XrSwapchainImageAcquireInfo* info, uint32_t* index) {
  (void) handle;
  (void) info;
  acquired++;
  if (failAcquire) return XR_ERROR_RUNTIME_FAILURE;
  *index = 0;
  return XR_SUCCESS;
}

static XrResult XRAPI_CALL fakeWait(XrSwapchain handle, const XrSwapchainImageWaitInfo* info) {
  (void) handle;
  (void) info;
  return waitResult;
}

static XrResult XRAPI_CALL fakeRelease(XrSwapchain handle, const XrSwapchainImageReleaseInfo* info) {
  (void) handle; (void) info;
  return failRelease ? XR_ERROR_RUNTIME_FAILURE : XR_SUCCESS;
}

static XrResult XRAPI_CALL fakeDestroySwapchain(XrSwapchain handle) {
  (void) handle;
  if (!runtimeLive || !prepared || retired) abort();
  if (failSwapchainDestroy) return XR_ERROR_RUNTIME_FAILURE;
  destroyed++;
  return XR_SUCCESS;
}

static XrResult XRAPI_CALL fakeDestroySession(XrSession session) {
  (void) session;
  if (failSessionDestroy) return XR_ERROR_RUNTIME_FAILURE;
  runtimeLive = false;
  return XR_SUCCESS;
}

Texture* lovrTextureCreate(const TextureInfo* info) {
  (void) info;
  atomic_uint* texture = lovrCalloc(sizeof(*texture));
  atomic_init(texture, 1);
  return (Texture*) texture;
}

void lovrTextureDestroy(void* texture) { lovrFree(texture); }
void lovrPassDestroy(void* pass) { lovrFree(pass); }
Pass* lovrPassCreate(const char* label) {
  (void) label;
  atomic_uint* pass = lovrCalloc(sizeof(*pass));
  atomic_init(pass, 1);
  return (Pass*) pass;
}
bool lovrPassSetCanvas(Pass* pass, Canvas* canvas) { (void) pass; (void) canvas; return true; }
bool lovrPassSetClear(Pass* pass, LoadAction loads[4], float clears[4][4], LoadAction depthLoad, float depthClear) {
  (void) pass; (void) loads; (void) clears; (void) depthLoad; (void) depthClear;
  return true;
}
bool lovrPassSetViewMatrix(Pass* pass, uint32_t index, float matrix[16]) {
  (void) pass; (void) index; (void) matrix;
  return true;
}
bool lovrPassSetProjection(Pass* pass, uint32_t index, float matrix[16]) {
  (void) pass; (void) index; (void) matrix;
  return true;
}
void lovrMeshDestroy(void* mesh) { lovrFree(mesh); }

static void beginSession(void) {
  runtimeLive = true;
  state.session = (XrSession) (uintptr_t) 1;
  currentGeneration = lovrHeadsetNextSessionGeneration();
  xrCreateSwapchain = fakeCreateSwapchain;
  xrEnumerateSwapchainImages = fakeEnumerateImages;
  xrAcquireSwapchainImage = fakeAcquire;
  xrWaitSwapchainImage = fakeWait;
  xrReleaseSwapchainImage = fakeRelease;
  xrDestroySwapchain = fakeDestroySwapchain;
  xrDestroySession = fakeDestroySession;
}

static bool retainedRestart(void) {
  beginSession();
  unsigned texturesBefore = registeredTextures, passesBefore = registeredPasses, invalidationsBefore = invalidations;
  LayerInfo info = { .width = 32, .height = 32 };
  Layer* neverSubmitted = openxrLayerCreate(&info);
  Layer* submitted = openxrLayerCreate(&info);
  CHECK(neverSubmitted && submitted);
  CHECK(lovrLayerGetCreator(neverSubmitted) == &lovrHeadsetOpenXROps);
  CHECK(neverSubmitted->ownership.generation == currentGeneration);
  CHECK(lovrLayerIsValid(neverSubmitted));
  CHECK(lovrLayerIsValid(submitted));
  CHECK(openxrLayerGetPass(neverSubmitted));
  CHECK(openxrLayerGetPass(submitted));
  CHECK(registeredTextures == texturesBefore + 2 && registeredPasses == passesBefore + 2);
  CHECK(openxrHeadsetSetLayers(&submitted, 1, false));
  unsigned before = destroyed;
  openxrHeadsetStop();
  CHECK(destroyed == before + 2);
  CHECK(invalidations == invalidationsBefore + 1);
  CHECK(!lovrLayerIsValid(neverSubmitted));
  CHECK(!lovrLayerIsValid(submitted));
  beginSession();
  CHECK(!lovrLayerIsValid(neverSubmitted));
  CHECK(!lovrLayerIsValid(submitted));
  unsigned calls = acquired;
  CHECK(openxrLayerGetTexture(neverSubmitted) == NULL);
  CHECK(openxrLayerGetTexture(submitted) == NULL);
  CHECK(openxrLayerGetPass(neverSubmitted) == NULL);
  CHECK(openxrLayerGetPass(submitted) == NULL);
  CHECK(acquired == calls);
  CHECK(!openxrHeadsetSetLayers(&submitted, 1, true));
  CHECK(state.layerCount == 0 && !state.showMainLayer);
  Device origin = submitted->origin;
  openxrLayerSetOrigin(submitted, DEVICE_HEAD);
  CHECK(submitted->origin == origin);
  CHECK(!openxrLayerSetCurve(submitted, 1.f));
  lovrRelease(neverSubmitted, openxrLayerDestroy);
  lovrRelease(submitted, openxrLayerDestroy);
  CHECK(destroyed == before + 2);
  openxrHeadsetStop();
  CHECK(layerRegistry == NULL);
  return true;
}

static bool atomicReplacement(void) {
  beginSession();
  LayerInfo info = { .width = 32, .height = 32 };
  Layer* layer = openxrLayerCreate(&info);
  CHECK(layer);
  CHECK(openxrHeadsetSetLayers(&layer, 1, false));
  lovrRelease(layer, openxrLayerDestroy);
  CHECK(openxrHeadsetSetLayers(state.layers, 1, true));
  CHECK(state.layers[0] == layer && state.layerCount == 1 && state.showMainLayer);
  Layer* invalid[] = { layer, NULL };
  CHECK(!openxrHeadsetSetLayers(invalid, 2, false));
  CHECK(state.layers[0] == layer && state.layerCount == 1 && state.showMainLayer);
  CHECK(!openxrHeadsetSetLayers(NULL, 1, false));
  CHECK(!openxrHeadsetSetLayers(&layer, MAX_LAYERS + 1, false));
  LayerInfo stereoInfo = { .width = 32, .height = 32, .stereo = true };
  Layer* stereo = openxrLayerCreate(&stereoInfo);
  CHECK(stereo);
  Layer* tooMany[MAX_LAYERS];
  for (uint32_t i = 0; i < MAX_LAYERS; i++) tooMany[i] = stereo;
  CHECK(!openxrHeadsetSetLayers(tooMany, MAX_LAYERS, false));
  CHECK(state.layers[0] == layer && state.layerCount == 1 && state.showMainLayer);
  CHECK(atomic_load(&stereo->ownership.ref) == 1);
  lovrRelease(stereo, openxrLayerDestroy);
  openxrHeadsetStop();
  CHECK(layerRegistry == NULL);
  return true;
}

static unsigned instancesDestroyed, actionSetsDestroyed;

static XrResult XRAPI_CALL fakeDestroyInstance(XrInstance instance) {
  (void) instance;
  instancesDestroyed++;
  return XR_SUCCESS;
}

static XrResult XRAPI_CALL fakeDestroyActionSet(XrActionSet actionSet) {
  (void) actionSet;
  actionSetsDestroyed++;
  return XR_SUCCESS;
}

static bool disconnectOwnership(void) {
  disconnect();
  disconnect();
  beginSession();
  state.instance = (XrInstance) (uintptr_t) 1;
  state.actionSet = (XrActionSet) (uintptr_t) 1;
  xrDestroyInstance = fakeDestroyInstance;
  xrDestroyActionSet = fakeDestroyActionSet;
  LayerInfo info = { .width = 32, .height = 32 };
  Layer* layer = openxrLayerCreate(&info);
  CHECK(layer);
  unsigned before = destroyed;
  unsigned invalidationsBefore = invalidations, flushesBefore = flushes;
  prepareSucceeds = false;
  disconnect();
  CHECK(state.session && state.instance && state.actionSet);
  CHECK(lovrLayerIsValid(layer));
  CHECK(destroyed == before && invalidations == invalidationsBefore && flushes == flushesBefore);
  CHECK(instancesDestroyed == 0 && actionSetsDestroyed == 0);
  CHECK(lovrModuleAcquire(&ref));
  lovrModuleReady(&ref);
  unsigned savedRef = atomic_load(&ref);
  openxrHeadsetDestroy();
  CHECK(atomic_load(&ref) == savedRef);
  CHECK(state.session && state.instance && state.actionSet && lovrLayerIsValid(layer));
  prepareSucceeds = true;
  disconnect();
  CHECK(destroyed == before + 1);
  CHECK(instancesDestroyed == 1 && actionSetsDestroyed == 1);
  CHECK(!state.session && !state.instance && !state.actionSet);
  disconnect();
  CHECK(instancesDestroyed == 1 && actionSetsDestroyed == 1);
  CHECK(!lovrLayerIsValid(layer));
  CHECK(!lovrLayerGetTexture(layer));
  lovrRelease(layer, openxrLayerDestroy);
  CHECK(destroyed == before + 1 && layerRegistry == NULL);
  state.instance = (XrInstance) (uintptr_t) 2;
  disconnect();
  CHECK(instancesDestroyed == 2);
  disconnect();
  CHECK(instancesDestroyed == 2);
  openxrHeadsetDestroy();
  CHECK(atomic_load(&ref) == 0);
  return true;
}

static bool targetedFailure(void) {
  beginSession();
  LayerInfo info = { .width = 32, .height = 32 };
  Layer* layer = openxrLayerCreate(&info);
  CHECK(layer && openxrLayerGetPass(layer));
  unsigned before = destroyed;
  prepareSucceeds = false;
  CHECK(!lovrSwapchainDestroy(&layer->swapchain));
  CHECK(destroyed == before && lovrLayerIsValid(layer));
  lovrRelease(layer, openxrLayerDestroy);
  CHECK(layer->deferredDestroy && layer->swapchain.handle && layer->pass);
  CHECK(atomic_load(&layer->ownership.ref) == 1);
  prepareSucceeds = true;
  openxrHeadsetStop();
  CHECK(destroyed == before + 1 && layerRegistry == NULL);
  return true;
}

static unsigned waitFrameCalls;
static XrResult XRAPI_CALL fakeWaitFrame(XrSession session, const XrFrameWaitInfo* info, XrFrameState* frame) {
  (void) session; (void) info; (void) frame;
  waitFrameCalls++;
  return XR_ERROR_SESSION_LOST;
}

static bool runtimeFailures(void) {
  beginSession();
  LayerInfo info = { .width = 32, .height = 32 };
  unsigned before = destroyed;
  failEnumerate = true;
  CHECK(!openxrLayerCreate(&info));
  CHECK(destroyed == before + 1 && !layerRegistry);
  failEnumerate = false;
  state.width = state.height = 32;
  state.viewCount = 1;
  CHECK(createSwapchains());
  failEnumerate = true;
  CHECK(!createSwapchains());
  CHECK(!state.presentationReady && !state.layerViews[0].subImage.swapchain);
  Texture* texture = NULL;
  Pass* pass = NULL;
  CHECK(!openxrHeadsetGetTexture(&texture));
  CHECK(!openxrHeadsetGetPass(&pass));
  CHECK(!openxrHeadsetSubmit());
  failEnumerate = false;
  CHECK(createSwapchains() && state.presentationReady);
  CHECK(openxrStopSession());
  beginSession();
  Layer* layer = openxrLayerCreate(&info);
  CHECK(layer);
  failSwapchainDestroy = true;
  CHECK(!openxrStopSession());
  CHECK(layer->swapchain.handle && !layer->swapchain.textureCount);
  CHECK(!openxrLayerGetTexture(layer));
  failSwapchainDestroy = false;
  failSessionDestroy = true;
  CHECK(!openxrStopSession());
  CHECK(state.session && !layer->swapchain.handle);
  CHECK(!createSwapchains() && state.stopping);
  CHECK(!openxrHeadsetPollEvents());
  xrWaitFrame = fakeWaitFrame;
  state.sessionState = XR_SESSION_STATE_FOCUSED;
  unsigned waitsBefore = waitFrameCalls;
  CHECK(!openxrHeadsetUpdate() && waitFrameCalls == waitsBefore);
  CHECK(!loadControllerModels());
  CHECK(!openxrLayerCreate(&info));
  CHECK(!openxrLayerIsCurrent(layer));
  CHECK(!openxrHeadsetNewModelData(1));
  CHECK(!openxrHeadsetGetModelPose(NULL, NULL, NULL));
  CHECK(!openxrHeadsetAnimate(NULL));
  failSessionDestroy = false;
  CHECK(openxrStopSession());
  CHECK(!state.session);
  CHECK(!openxrHeadsetNewModelData(1));
  CHECK(!openxrHeadsetGetModelPose(NULL, NULL, NULL));
  CHECK(!openxrHeadsetAnimate(NULL));
  lovrRelease(layer, openxrLayerDestroy);
  beginSession();
  state.extensions.layerEquirect = true;
  CHECK(openxrHeadsetSetBackground(32, 32, 1));
  CHECK(state.backgroundReady);
  failSwapchainDestroy = true;
  CHECK(!openxrHeadsetSetBackground(64, 64, 1));
  CHECK(!state.backgroundReady && !state.background.header.type);
  CHECK(state.swapchains[SWAPCHAIN_BACKGROUND].handle);
  failSwapchainDestroy = false;
  CHECK(openxrStopSession());
  beginSession();
  state.simulator.texture = lovrTextureCreate(NULL);
  state.simulator.pass = lovrPassCreate(NULL);
  CHECK(openxrStopSession());
  CHECK(!state.simulator.texture && !state.simulator.pass);
  return true;
}

static bool imageFailures(void) {
  beginSession();
  Swapchain swapchain = { 0 };
  CHECK(lovrSwapchainInit(&swapchain, 32, 32, 0));
  failAcquire = true;
  CHECK(!lovrSwapchainAcquire(&swapchain) && !swapchain.acquired);
  failAcquire = false;
  waitResult = XR_ERROR_SESSION_LOST;
  CHECK(!lovrSwapchainAcquire(&swapchain) && swapchain.acquired && !swapchain.ready);
  unsigned calls = acquired;
  waitResult = XR_TIMEOUT_EXPIRED;
  CHECK(!lovrSwapchainAcquire(&swapchain) && acquired == calls);
  CHECK(!lovrSwapchainRelease(&swapchain) && swapchain.acquired);
  waitResult = XR_SUCCESS;
  CHECK(lovrSwapchainAcquire(&swapchain) && acquired == calls);
  failRelease = true;
  CHECK(!lovrSwapchainRelease(&swapchain) && swapchain.acquired && swapchain.ready);
  failRelease = false;
  CHECK(lovrSwapchainRelease(&swapchain) && !swapchain.acquired && !swapchain.ready);
  CHECK(lovrSwapchainDestroy(&swapchain));
  CHECK(openxrStopSession());
  return true;
}

static unsigned endedFrames;
static XrResult XRAPI_CALL fakeEndFrame(XrSession session, const XrFrameEndInfo* info) {
  (void) session; (void) info;
  endedFrames++;
  return XR_SUCCESS;
}

static bool submitReleaseFailures(void) {
  beginSession();
  state.presentationReady = true;
  state.sessionState = XR_SESSION_STATE_FOCUSED;
  state.frameState.shouldRender = true;
  state.began = true;
  state.showMainLayer = false;
  xrEndFrame = fakeEndFrame;
  LayerInfo info = { .width = 32, .height = 32 };
  Layer* layer = openxrLayerCreate(&info);
  CHECK(layer && openxrHeadsetSetLayers(&layer, 1, false));
  for (uint32_t i = 0; i < 2; i++) {
    CHECK(lovrSwapchainInit(&state.swapchains[i], 32, 32, 0));
    CHECK(lovrSwapchainAcquire(&state.swapchains[i]));
  }
  unsigned before = endedFrames;
  failRelease = true;
  CHECK(!openxrHeadsetSubmit() && endedFrames == before);
  CHECK(lovrSwapchainRelease(&state.swapchains[0]) == false);
  failRelease = false;
  CHECK(lovrSwapchainRelease(&state.swapchains[0]));
  failRelease = true;
  CHECK(!openxrHeadsetSubmit() && endedFrames == before);
  failRelease = false;
  CHECK(lovrSwapchainRelease(&state.swapchains[1]));
  failRelease = true;
  CHECK(!openxrHeadsetSubmit() && endedFrames == before);
  failRelease = false;
  CHECK(openxrHeadsetSubmit() && endedFrames == before + 1);
  CHECK(openxrStopSession());
  lovrRelease(layer, openxrLayerDestroy);
  return true;
}

int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "openxr.layers.retained-restart", retainedRestart },
    { "openxr.layers.atomic-replacement", atomicReplacement },
    { "openxr.layers.disconnect-ownership", disconnectOwnership },
    { "openxr.layers.targeted-failure", targetedFailure },
    { "openxr.layers.runtime-failures", runtimeFailures },
    { "openxr.layers.image-failures", imageFailures },
    { "openxr.layers.submit-release-failures", submitReleaseFailures }
  };
  return nativeRunTests(argc, argv, tests, COUNTOF(tests));
}
