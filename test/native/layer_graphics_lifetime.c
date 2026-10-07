#include "test.h"
#include "../../src/modules/graphics/graphics.c"
#define LOVR_LAYER_GRAPHICS_HARNESS
#define main graphicsSessionMain
#include "graphics_session.c"
#undef main
#define state xrState
#define onMessage xrOnMessage
#include "../../src/modules/headset/headset_openxr.c"
#undef state
#undef onMessage

uintptr_t gpu_vk_get_instance(void) { abort(); }
uintptr_t gpu_vk_get_physical_device(void) { abort(); }
uintptr_t gpu_vk_get_device(void) { abort(); }
uintptr_t gpu_vk_get_queue(uint32_t* family, uint32_t* index) { abort(); }
void lovrEventPush(Event event) { abort(); }
void os_sleep(double seconds) { abort(); }
void lovrSystemGetWindowSize(uint32_t* width, uint32_t* height) { abort(); }
ModelData* lovrModelDataCreate(Blob* blob, ModelDataIO* io) { abort(); }
void lovrModelDataAllocate(ModelData* model) { abort(); }
bool lovrModelDataFinalize(ModelData* model) { abort(); }
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(const char* name, uint32_t capacity, uint32_t* count, XrExtensionProperties* properties) { abort(); }
XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(const XrInstanceCreateInfo* info, XrInstance* instance) { abort(); }
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) { abort(); }

typedef struct {
  gpu_texture* object;
  unsigned root;
  uintptr_t image;
  bool live;
} TrackedTexture;
static TrackedTexture tracked[128];
static unsigned trackedCount, swapchainCreates, swapchainDestroys, acquireCalls, preparations;
static bool prepared;
static bool imagesLive[32];

static unsigned trackIndex(gpu_texture* texture) {
  for (unsigned i = trackedCount; i > 0; i--) if (tracked[i - 1].object == texture && tracked[i - 1].live) return i - 1;
  abort();
}

size_t gpu_sizeof_texture(void) { return sizeof(gpu_texture); }
bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info) {
  if (!runtimeAlive || trackedCount == 128) abort();
  *texture = (gpu_texture) { 0 };
  unsigned i = trackedCount++;
  tracked[i] = (TrackedTexture) { texture, i, info->handle, true };
  return true;
}
bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info) {
  unsigned source = trackIndex(info->source);
  if (!runtimeAlive || trackedCount == 128) abort();
  *texture = (gpu_texture) { .view = true };
  tracked[trackedCount++] = (TrackedTexture) { texture, tracked[source].root, tracked[source].image, true };
  liveViews++;
  return true;
}
void gpu_texture_destroy(gpu_texture* texture) {
  unsigned i = trackIndex(texture);
  if (!runtimeAlive || texture->destroyed) abort();
  if (tracked[i].image && !imagesLive[tracked[i].image]) abort();
  if (!texture->view) {
    for (unsigned j = 0; j < trackedCount; j++) if (j != i && tracked[j].live && tracked[j].root == i) abort();
  } else liveViews--;
  tracked[i].live = false;
  texture->destroyed = true;
  textureDestroys++;
  deferredDestroys++;
}
bool gpu_prepare_teardown(void) {
  preparations++;
  prepared = !prepareFails;
  if (!prepared) lovrSetError("gpu: injected teardown failure");
  return prepared;
}
void gpu_flush_deferred_after_idle(void) { if (!prepared) abort(); deferredDestroys = 0; drainCalls++; }
void gpu_flush_deferred(void) { deferredDestroys = 0; drainCalls++; }
bool gpu_quiesce_locked(void) { return gpu_prepare_teardown(); }
bool gpu_wait_idle(void) { if (!gpu_prepare_teardown()) return false; gpu_flush_deferred_after_idle(); return true; }

static XrResult XRAPI_CALL jointCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* info, XrSwapchain* handle) {
  *handle = (XrSwapchain) (uintptr_t) ++swapchainCreates;
  if (swapchainCreates >= 32) abort();
  imagesLive[swapchainCreates] = true;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL jointEnumerateImages(XrSwapchain handle, uint32_t capacity, uint32_t* count, XrSwapchainImageBaseHeader* images) {
  *count = 1;
  ((XrSwapchainImageVulkanKHR*) images)->image = (VkImage) (uintptr_t) handle;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL jointAcquire(XrSwapchain handle, const XrSwapchainImageAcquireInfo* info, uint32_t* index) {
  if (!imagesLive[(uintptr_t) handle]) abort();
  acquireCalls++;
  *index = 0;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL jointWait(XrSwapchain handle, const XrSwapchainImageWaitInfo* info) { return XR_SUCCESS; }
static XrResult XRAPI_CALL jointReleaseImage(XrSwapchain handle, const XrSwapchainImageReleaseInfo* info) { return XR_SUCCESS; }
static XrResult XRAPI_CALL jointDestroySwapchain(XrSwapchain handle) {
  uintptr_t image = (uintptr_t) handle;
  if (!imagesLive[image] || !prepared || deferredDestroys) abort();
  for (unsigned i = 0; i < trackedCount; i++) if (tracked[i].live && tracked[i].image == image) abort();
  imagesLive[image] = false;
  swapchainDestroys++;
  return XR_SUCCESS;
}
static XrResult XRAPI_CALL jointDestroySession(XrSession session) {
  for (unsigned i = 1; i <= swapchainCreates; i++) if (imagesLive[i]) abort();
  return XR_SUCCESS;
}

static void jointBegin(void) {
  runtimeAlive = true;
  Texture* probe = rootTexture();
  lovrRelease(probe, lovrTextureDestroy);
  state.features.formats[FORMAT_RGBA8][1] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
  xrState.session = (XrSession) (uintptr_t) 1;
  currentGeneration = lovrHeadsetNextSessionGeneration();
  xrCreateSwapchain = jointCreateSwapchain;
  xrEnumerateSwapchainImages = jointEnumerateImages;
  xrAcquireSwapchainImage = jointAcquire;
  xrWaitSwapchainImage = jointWait;
  xrReleaseSwapchainImage = jointReleaseImage;
  xrDestroySwapchain = jointDestroySwapchain;
  xrDestroySession = jointDestroySession;
}

typedef struct { Layer* layer; Texture* texture; Texture* view; Texture* nested; Pass* pass; } RetainedLayer;
static bool jointRetain(RetainedLayer* retained) {
  LayerInfo info = { .width = 32, .height = 32 };
  retained->layer = lovrLayerCreate(&info);
  CHECK(retained->layer);
  retained->texture = lovrLayerGetTexture(retained->layer);
  retained->pass = lovrLayerGetPass(retained->layer);
  CHECK(retained->texture && retained->pass);
  lovrRetain(retained->texture);
  lovrRetain(retained->pass);
  TextureViewInfo viewInfo = { .type = TEXTURE_2D, .layerCount = 1, .levelCount = 1 };
  retained->view = lovrTextureCreateView(retained->texture, &viewInfo);
  CHECK(retained->view);
  retained->nested = lovrTextureCreateView(retained->view, &viewInfo);
  CHECK(retained->nested);
  return true;
}
static bool jointDead(RetainedLayer* retained) {
  CHECK(!openxrLayerIsCurrent(retained->layer));
  CHECK(!lovrTextureIsValid(retained->texture) && !lovrTextureIsValid(retained->view) && !lovrTextureIsValid(retained->nested));
  CHECK(!lovrPassIsValid(retained->pass));
  CHECK(!retained->texture->gpu && !retained->view->gpu && !retained->nested->gpu);
  CHECK(!retained->pass->canvas.color[0].texture && !retained->pass->target.color[0].texture);
  unsigned calls = acquireCalls;
  CHECK(!lovrLayerGetTexture(retained->layer) && !lovrLayerGetPass(retained->layer));
  CHECK(calls == acquireCalls);
  TextureViewInfo info = { .type = TEXTURE_2D, .layerCount = 1, .levelCount = 1 };
  CHECK(!lovrTextureCreateView(retained->nested, &info));
  CHECK(!lovrPassSetCanvas(retained->pass, NULL));
  CHECK(!lovrGraphicsSubmit(&retained->pass, 1));
  CHECK(strstr(lovrGetError(), "invalidated headset session"));
  return true;
}
static void jointRelease(RetainedLayer* retained) {
  lovrRelease(retained->layer, lovrLayerDestroy);
  lovrRelease(retained->texture, lovrTextureDestroy);
  lovrRelease(retained->view, lovrTextureDestroy);
  lovrRelease(retained->nested, lovrTextureDestroy);
  lovrRelease(retained->pass, lovrPassDestroy);
}
static bool jointRestart(void) {
  jointBegin();
  RetainedLayer submitted, unsubmitted;
  CHECK(jointRetain(&submitted) && jointRetain(&unsubmitted));
  CHECK(lovrHeadsetSetLayers(&submitted.layer, 1, false));
  lovrHeadsetStop();
  CHECK(swapchainDestroys == 2 && !deferredDestroys && drainCalls);
  CHECK(jointDead(&submitted) && jointDead(&unsubmitted));
  jointBegin();
  CHECK(jointDead(&submitted) && jointDead(&unsubmitted));
  RetainedLayer fresh;
  CHECK(jointRetain(&fresh));
  unsigned destroys = textureDestroys, xrDestroys = swapchainDestroys;
  jointRelease(&submitted);
  jointRelease(&unsubmitted);
  CHECK(textureDestroys == destroys && swapchainDestroys == xrDestroys);
  CHECK(lovrLayerIsValid(fresh.layer) && lovrTextureIsValid(fresh.nested) && lovrPassIsValid(fresh.pass));
  lovrHeadsetStop();
  CHECK(jointDead(&fresh));
  runtimeAlive = false;
  jointRelease(&fresh);
  CHECK(!layerRegistry && !sessionTextures && !sessionPasses);
  return true;
}
static bool jointSelective(void) {
  unsigned before = swapchainDestroys;
  jointBegin();
  RetainedLayer retired, live;
  CHECK(jointRetain(&retired) && jointRetain(&live));
  Texture* unrelated = rootTexture();
  Pass* unrelatedPass = lovrPassCreate("Unrelated");
  CHECK(unrelatedPass);
  prepareFails = true;
  lovrRelease(retired.layer, lovrLayerDestroy);
  CHECK(retired.layer->deferredDestroy && lovrLayerIsValid(retired.layer));
  CHECK(swapchainDestroys == before && lovrTextureIsValid(retired.nested));
  prepareFails = false;
  CHECK(openxrLayerInvalidate(retired.layer));
  CHECK(swapchainDestroys == before + 1 && !deferredDestroys);
  CHECK(jointDead(&retired));
  CHECK(lovrLayerIsValid(live.layer) && lovrTextureIsValid(live.nested) && lovrPassIsValid(live.pass));
  CHECK(lovrTextureIsValid(unrelated) && lovrPassIsValid(unrelatedPass));
  CHECK(lovrLayerGetTexture(live.layer) == live.texture);
  CHECK(lovrPassSetCanvas(unrelatedPass, NULL));
  jointRelease(&retired);
  lovrHeadsetStop();
  CHECK(jointDead(&live));
  CHECK(lovrTextureIsValid(unrelated) && lovrPassIsValid(unrelatedPass));
  jointRelease(&live);
  lovrRelease(unrelated, lovrTextureDestroy);
  lovrRelease(unrelatedPass, lovrPassDestroy);
  CHECK(!layerRegistry && !sessionTextures && !sessionPasses);
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "openxr.graphics.retained-restart", jointRestart },
    { "openxr.graphics.selective-retirement", jointSelective }
  };
  return nativeRunTests(argc, argv, tests, COUNTOF(tests));
}
