#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include "test.h"

static void* testOpen(const char* path, int flags);
static void* testSymbol(void* library, const char* name);
static int testClose(void* library);
#define dlopen testOpen
#define dlsym testSymbol
#define dlclose testClose
#include "../../src/core/gpu_vk.c"
#undef dlopen
#undef dlsym
#undef dlclose

#define HANDLE(Type, value) ((Type) (uintptr_t) (value))

static bool invalid;
static bool locksReady;
static unsigned imagesCreated, allocations, bindings, viewsCreated, barriers;
static VkImageCreateInfo createdImage;
static VkImageViewCreateInfo createdView;
static VkDependencyInfoKHR dependency;
static VkImageMemoryBarrier2KHR barrier;
static VkCommandBuffer commands;
static bool queueLocked;
static bool rejectQueueLock;
static unsigned queueLocks, queueUnlocks, idleCalls;
static VkResult idleResult;
static bool reset(void);
static gpu_upload* concurrentUpload;

static bool lockQueue(void* context) {
  invalid |= context != &queueLocked || queueLocked;
  queueLocks++;
  if (rejectQueueLock) return false;
  queueLocked = true;
  return true;
}

static void unlockQueue(void* context) {
  invalid |= context != &queueLocked || !queueLocked;
  queueLocked = false;
  queueUnlocks++;
}

static VKAPI_ATTR VkResult VKAPI_CALL waitIdle(VkDevice device) {
  invalid |= device != state.device || !queueLocked;
  idleCalls++;
  return idleResult;
}

static bool beforeDestroy(void) {
  invalid |= queueLocked;
  return gpu_prepare_teardown();
}

static VKAPI_ATTR VkResult VKAPI_CALL failBegin(VkCommandBuffer buffer, const VkCommandBufferBeginInfo* info) {
  if (concurrentUpload) {
    concurrentUpload->next = atomic_load(&state.uploads);
    while (!atomic_compare_exchange_strong(&state.uploads, &concurrentUpload->next, concurrentUpload)) {}
  }
  return VK_ERROR_OUT_OF_HOST_MEMORY;
}

static bool pendingUploadFailure(void) {
  CHECK(reset());
  gpu_stream stream = { .commands = HANDLE(VkCommandBuffer, 1) };
  gpu_stream_pool pool = { .head = &stream };
  memset(&thread, 0, sizeof(thread));
  thread.activeStreamPool = &pool;
  gpu_upload upload = { .image = HANDLE(VkImage, 2), .aspect = VK_IMAGE_ASPECT_COLOR_BIT };
  atomic_store(&state.uploads, &upload);
  vkBeginCommandBuffer = failBegin;
  CHECK(!gpu_submit(NULL, 0, 1));
  CHECK(atomic_load(&state.uploads) == &upload && upload.next == NULL);
  CHECK(barriers == 0 && !invalid);
  gpu_upload additional = { .image = HANDLE(VkImage, 3) };
  concurrentUpload = &additional;
  CHECK(!gpu_submit(NULL, 0, 1));
  CHECK(atomic_load(&state.uploads) == &additional && additional.next == &upload && upload.next == NULL);
  CHECK(barriers == 0 && !invalid);
  concurrentUpload = NULL;
  atomic_store(&state.uploads, NULL);
  memset(&thread, 0, sizeof(thread));
  return true;
}

static bool teardownLocking(void) {
  CHECK(reset());
  state.config.userdata = &queueLocked;
  state.config.fnQueueLock = lockQueue;
  state.config.fnQueueUnlock = unlockQueue;
  state.config.vk.beforeDestroy = beforeDestroy;
  vkDeviceWaitIdle = waitIdle;
  queueLocked = rejectQueueLock = false;
  queueLocks = queueUnlocks = idleCalls = 0;
  idleResult = VK_ERROR_OUT_OF_HOST_MEMORY;
  CHECK(!gpu_prepare_teardown());
  CHECK(queueLocks == 1 && queueUnlocks == 1 && idleCalls == 1 && !queueLocked);
  rejectQueueLock = true;
  CHECK(!gpu_begin_teardown());
  CHECK(queueLocks == 2 && queueUnlocks == 1 && idleCalls == 1);
  rejectQueueLock = false;
  idleResult = VK_ERROR_DEVICE_LOST;
  CHECK(gpu_prepare_teardown());
  CHECK(gpu_begin_teardown());
  CHECK(queueLocks == 4 && queueUnlocks == 3 && idleCalls == 3 && !invalid && !queueLocked);
  return true;
}

static VKAPI_ATTR VkResult VKAPI_CALL createImage(VkDevice device, const VkImageCreateInfo* info,
  const VkAllocationCallbacks* allocator, VkImage* image) {
  invalid |= device != state.device || allocator || info->sType != VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  createdImage = *info;
  *image = HANDLE(VkImage, 0x12345678);
  imagesCreated++;
  return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL requirements(VkDevice device, VkImage image, VkMemoryRequirements* info) {
  invalid |= device != state.device || image != HANDLE(VkImage, 0x12345678);
  *info = (VkMemoryRequirements) { .size = GPU_PAGE_SIZE, .alignment = GPU_PAGE_SIZE, .memoryTypeBits = 1 };
}

static VKAPI_ATTR VkResult VKAPI_CALL allocateMemory(VkDevice device, const VkMemoryAllocateInfo* info,
  const VkAllocationCallbacks* allocator, VkDeviceMemory* memory) {
  invalid |= device != state.device || allocator || info->sType != VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO ||
    info->memoryTypeIndex != 0 || info->allocationSize < GPU_PAGE_SIZE;
  *memory = HANDLE(VkDeviceMemory, 0x23456789);
  allocations++;
  return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL bindMemory(VkDevice device, VkImage image, VkDeviceMemory memory,
  VkDeviceSize offset) {
  invalid |= device != state.device || image != HANDLE(VkImage, 0x12345678) ||
    memory != HANDLE(VkDeviceMemory, 0x23456789) || offset != 0;
  bindings++;
  return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL createView(VkDevice device, const VkImageViewCreateInfo* info,
  const VkAllocationCallbacks* allocator, VkImageView* view) {
  invalid |= device != state.device || allocator || info->sType != VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  createdView = *info;
  *view = HANDLE(VkImageView, 0x3456789a);
  viewsCreated++;
  return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL destroyImage(VkDevice device, VkImage image,
  const VkAllocationCallbacks* allocator) {
  invalid = true;
}

static VKAPI_ATTR void VKAPI_CALL pipelineBarrier(VkCommandBuffer commandBuffer, const VkDependencyInfoKHR* info) {
  barriers++;
  commands = commandBuffer;
  dependency = *info;
  if (info->imageMemoryBarrierCount != 1 || !info->pImageMemoryBarriers) {
    invalid = true;
    return;
  }
  barrier = *info->pImageMemoryBarriers;
}

static bool reset(void) {
  if (locksReady) {
    mtx_destroy(&state.allocatorLock);
    mtx_destroy(&state.morgue.lock);
    locksReady = false;
  }
  memset(&state, 0, sizeof(state));
  CHECK(mtx_init(&state.allocatorLock, mtx_plain) == thrd_success);
  if (mtx_init(&state.morgue.lock, mtx_plain) != thrd_success) {
    mtx_destroy(&state.allocatorLock);
    CHECK(false);
  }
  locksReady = true;
  state.config.fnAlloc = malloc;
  state.config.fnFree = free;
  state.instance = HANDLE(VkInstance, 0x11);
  state.adapter = HANDLE(VkPhysicalDevice, 0x22);
  state.device = HANDLE(VkDevice, 0x33);
  state.queue = HANDLE(VkQueue, 0x44);
  state.queueFamilyIndex = 7;
  vkCreateImage = createImage;
  vkGetImageMemoryRequirements = requirements;
  vkAllocateMemory = allocateMemory;
  vkBindImageMemory = bindMemory;
  vkCreateImageView = createView;
  vkDestroyImage = destroyImage;
  vkCmdPipelineBarrier2KHR = pipelineBarrier;
  invalid = false;
  imagesCreated = allocations = bindings = viewsCreated = barriers = 0;
  memset(&barrier, 0, sizeof(barrier));
  return true;
}

static gpu_texture_info textureInfo(void) {
  return (gpu_texture_info) {
    .type = GPU_TEXTURE_2D,
    .format = GPU_FORMAT_RGBA8,
    .size = { 320, 192, 1 },
    .mipmaps = 4,
    .samples = 1,
    .usage = GPU_TEXTURE_SAMPLE | GPU_TEXTURE_COPY_SRC | GPU_TEXTURE_RENDER
  };
}

static bool descriptor(void) {
  const struct { gpu_texture_format format; bool srgb; VkFormat expected; } formats[] = {
    { GPU_FORMAT_RGBA8, false, VK_FORMAT_R8G8B8A8_UNORM },
    { GPU_FORMAT_RGBA8, true, VK_FORMAT_R8G8B8A8_SRGB },
    { GPU_FORMAT_BGRA8, false, VK_FORMAT_B8G8R8A8_UNORM },
    { GPU_FORMAT_BGRA8, true, VK_FORMAT_B8G8R8A8_SRGB },
    { GPU_FORMAT_RGBA16F, false, VK_FORMAT_R16G16B16A16_SFLOAT }
  };
  for (size_t i = 0; i < COUNTOF(formats); i++) {
    CHECK(reset());
    gpu_texture texture = { 0 };
    gpu_texture_info info = textureInfo();
    info.format = formats[i].format;
    info.srgb = formats[i].srgb;
    info.samples = i % 2;
    CHECK(gpu_texture_init(&texture, &info));
    CHECK(!invalid && imagesCreated == 1 && allocations == 1 && bindings == 1 && viewsCreated == 1);
    CHECK(texture.externalValid && texture.memory && !texture.imported && !texture.foreign);
    CHECK(createdImage.imageType == VK_IMAGE_TYPE_2D && createdImage.extent.width == 320 &&
      createdImage.extent.height == 192 && createdImage.extent.depth == 1 && createdImage.arrayLayers == 1);
    CHECK(createdImage.mipLevels == 4 && createdImage.samples == VK_SAMPLE_COUNT_1_BIT);
    CHECK(createdImage.format == formats[i].expected && createdView.format == formats[i].expected);
    CHECK(createdImage.usage == (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
    gpu_external_image image = { 0 };
    CHECK(gpu_texture_get_external_image(&texture, &image));
    CHECK(image.instance == 0x11 && image.physicalDevice == 0x22 && image.device == 0x33 && image.queue == 0x44);
    CHECK(image.image == 0x12345678 && image.image != (uint64_t) texture.view);
    CHECK(image.queueFamily == 7 && image.queueIndex == 0);
    CHECK(image.width == 320 && image.height == 192 && image.samples == 1 && image.format == formats[i].expected);
    CHECK(!invalid && !barriers);
  }
  return true;
}

static bool rejected(gpu_texture* texture) {
  gpu_external_image image, original;
  memset(&image, 0xa5, sizeof(image));
  memcpy(&original, &image, sizeof(image));
  unsigned before = barriers;
  gpu_stream stream = { .commands = HANDLE(VkCommandBuffer, 0x55) };
  CHECK(!gpu_texture_get_external_image(texture, &image));
  CHECK(!memcmp(&image, &original, sizeof(image)));
  CHECK(!gpu_texture_external_barrier(&stream, texture, true));
  CHECK(!gpu_texture_external_barrier(&stream, texture, false));
  CHECK(barriers == before && !invalid);
  return true;
}

static unsigned adapterCallbacks, adapterEnumerations, deviceExtensionQueries, instancesDestroyed, librariesClosed;
static VkPhysicalDevice suppliedAdapter;

static void* testOpen(const char* path, int flags) {
  invalid |= strcmp(path, "libvulkan.so.1") != 0 || flags != (RTLD_NOW | RTLD_LOCAL);
  return (void*) (uintptr_t) 1;
}

static int testClose(void* library) {
  invalid |= library != (void*) (uintptr_t) 1;
  librariesClosed++;
  return 0;
}

static VKAPI_ATTR VkResult VKAPI_CALL initLayers(uint32_t* count, VkLayerProperties* properties) {
  *count = 1;
  if (properties) memset(properties, 0, sizeof(*properties));
  return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL initExtensions(const char* layer, uint32_t* count, VkExtensionProperties* properties) {
  invalid |= layer != NULL;
  *count = 1;
  if (properties) memset(properties, 0, sizeof(*properties));
  return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL initInstance(const VkInstanceCreateInfo* info,
    const VkAllocationCallbacks* allocator, VkInstance* instance) {
  invalid |= info->sType != VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO || allocator != NULL;
  *instance = HANDLE(VkInstance, 0x11);
  return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL destroyInstance(VkInstance instance, const VkAllocationCallbacks* allocator) {
  invalid |= instance != HANDLE(VkInstance, 0x11) || allocator != NULL;
  instancesDestroyed++;
}

static VKAPI_ATTR VkResult VKAPI_CALL enumerateAdapters(VkInstance instance, uint32_t* count, VkPhysicalDevice* adapters) {
  invalid |= instance != HANDLE(VkInstance, 0x11) || adapters != NULL;
  adapterEnumerations++;
  *count = 0;
  return VK_ERROR_INITIALIZATION_FAILED;
}

static VKAPI_ATTR VkResult VKAPI_CALL deviceExtensions(VkPhysicalDevice adapter, const char* layer,
    uint32_t* count, VkExtensionProperties* properties) {
  invalid |= adapter != suppliedAdapter || layer != NULL || properties != NULL;
  deviceExtensionQueries++;
  *count = 0;
  return VK_ERROR_INITIALIZATION_FAILED;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL initProc(VkInstance instance, const char* name) {
  if (!strcmp(name, "vkEnumerateInstanceLayerProperties")) return (PFN_vkVoidFunction) initLayers;
  if (!strcmp(name, "vkEnumerateInstanceExtensionProperties")) return (PFN_vkVoidFunction) initExtensions;
  if (!strcmp(name, "vkCreateInstance")) return (PFN_vkVoidFunction) initInstance;
  if (!strcmp(name, "vkDestroyInstance")) return (PFN_vkVoidFunction) destroyInstance;
  if (!strcmp(name, "vkEnumeratePhysicalDevices")) return (PFN_vkVoidFunction) enumerateAdapters;
  if (!strcmp(name, "vkEnumerateDeviceExtensionProperties")) return (PFN_vkVoidFunction) deviceExtensions;
  return NULL;
}

static void* testSymbol(void* library, const char* name) {
  invalid |= library != (void*) (uintptr_t) 1 || strcmp(name, "vkGetInstanceProcAddr") != 0;
  return (void*) initProc;
}

static void provideAdapter(void* instance, uintptr_t adapter) {
  invalid |= instance != HANDLE(VkInstance, 0x11);
  adapterCallbacks++;
  *(VkPhysicalDevice*) adapter = suppliedAdapter;
}

static bool physicalDeviceSelection(void) {
  if (locksReady) {
    mtx_destroy(&state.allocatorLock);
    mtx_destroy(&state.morgue.lock);
    locksReady = false;
  }
  for (unsigned strict = 0; strict < 2; strict++) {
    for (unsigned selection = 0; selection < 3; selection++) {
      memset(&state, 0, sizeof(state));
      invalid = false;
      adapterCallbacks = adapterEnumerations = deviceExtensionQueries = instancesDestroyed = librariesClosed = 0;
      suppliedAdapter = selection == 2 ? HANDLE(VkPhysicalDevice, 0x22) : VK_NULL_HANDLE;
      gpu_config config = { .fnAlloc = malloc, .fnFree = free };
      CHECK(!config.vk.requirePhysicalDevice);
      config.vk.requirePhysicalDevice = strict;
      config.vk.getPhysicalDevice = selection ? provideAdapter : NULL;
      CHECK(!gpu_init(&config));
      CHECK(adapterCallbacks == (selection != 0));
      CHECK(adapterEnumerations == (!strict && selection != 2));
      CHECK(deviceExtensionQueries == (selection == 2));
      if (strict && selection != 2) {
        CHECK(!strcmp(gpu_get_error(), "Required physical device unavailable"));
      } else {
        CHECK(strstr(gpu_get_error(), selection == 2 ? "vkEnumerateDeviceExtensionProperties" : "vkEnumeratePhysicalDevices"));
      }
      CHECK(instancesDestroyed == 1 && librariesClosed == 1);
      CHECK(!state.instance && !state.adapter && !state.library && !state.config.fnAlloc);
      CHECK(!invalid);
    }
  }
  return true;
}

static bool rejection(void) {
  CHECK(physicalDeviceSelection());
  CHECK(reset());
  gpu_texture texture = { 0 };
  gpu_texture_info info = textureInfo();
  CHECK(gpu_texture_init(&texture, &info));
  CHECK(rejected(NULL));
  CHECK(!gpu_texture_get_external_image(&texture, NULL));
  gpu_texture empty = { 0 };
  CHECK(rejected(&empty));
  gpu_texture original = texture;
#define REJECT(field, value) do { texture = original; texture.field = value; CHECK(rejected(&texture)); } while (0)
  REJECT(externalValid, false);
  REJECT(handle, VK_NULL_HANDLE);
  REJECT(view, VK_NULL_HANDLE);
  REJECT(memory, NULL);
  REJECT(imported, true);
  REJECT(foreign, true);
  REJECT(type, GPU_TEXTURE_ARRAY);
  REJECT(type, GPU_TEXTURE_3D);
  REJECT(type, GPU_TEXTURE_CUBE);
  REJECT(layers, 0);
  REJECT(layers, 2);
  REJECT(baseLevel, 1);
  REJECT(samples, 2);
  REJECT(aspect, VK_IMAGE_ASPECT_DEPTH_BIT);
  REJECT(aspect, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
  REJECT(usage, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  REJECT(usage, VK_IMAGE_USAGE_SAMPLED_BIT);
  REJECT(width, 0);
  REJECT(height, 0);
  REJECT(mipmaps, 0);
  REJECT(vkformat, VK_FORMAT_UNDEFINED);
  REJECT(layout, VK_IMAGE_LAYOUT_UNDEFINED);
#undef REJECT
  texture = original;
#define REJECT_STATE(field, value) do { \
    __typeof__(state.field) saved = state.field; \
    state.field = value; CHECK(rejected(&texture)); state.field = saved; \
  } while (0)
  REJECT_STATE(instance, VK_NULL_HANDLE);
  REJECT_STATE(adapter, VK_NULL_HANDLE);
  REJECT_STATE(device, VK_NULL_HANDLE);
  REJECT_STATE(queue, VK_NULL_HANDLE);
  REJECT_STATE(teardownPrepared, true);
#undef REJECT_STATE
  gpu_stream stream = { 0 };
  CHECK(!gpu_texture_external_barrier(NULL, &texture, true));
  CHECK(!gpu_texture_external_barrier(&stream, &texture, false));
  CHECK(!barriers && !invalid);
  return true;
}

static bool ownership(void) {
  CHECK(reset());
  gpu_texture owned = { 0 }, view = { 0 }, imported = { 0 }, foreign = { 0 };
  gpu_texture_info info = textureInfo();
  CHECK(gpu_texture_init(&owned, &info));
  gpu_texture_view_info viewInfo = {
    .source = &owned, .type = GPU_TEXTURE_2D, .usage = GPU_TEXTURE_SAMPLE,
    .levelIndex = 1, .levelCount = 1
  };
  CHECK(gpu_texture_init_view(&view, &viewInfo));
  CHECK(view.handle == owned.handle && !view.memory && !view.externalValid);
  CHECK(rejected(&view));
  info.handle = 0xabcdef;
  CHECK(gpu_texture_init(&imported, &info));
  CHECK(imported.imported && !imported.externalValid);
  CHECK(rejected(&imported));
  info.foreign = true;
  CHECK(gpu_texture_init(&foreign, &info));
  CHECK(foreign.foreign && foreign.imported && !foreign.externalValid);
  CHECK(rejected(&foreign));
  gpu_victim victims[2] = { 0 };
  victims[0].next = &victims[1];
  state.morgue.pool = &victims[0];
  gpu_texture_destroy(&owned);
  CHECK(!owned.externalValid && state.morgue.head == &victims[0] && state.morgue.tail == &victims[1]);
  CHECK(rejected(&owned));
  state.morgue.head = state.morgue.tail = state.morgue.pool = NULL;
  CHECK(!invalid && imagesCreated == 1 && allocations == 1 && bindings == 1 && viewsCreated == 4);
  return true;
}

static bool transitions(void) {
  CHECK(pendingUploadFailure());
  CHECK(teardownLocking());
  CHECK(reset());
  gpu_texture texture = { 0 };
  gpu_texture_info info = textureInfo();
  CHECK(gpu_texture_init(&texture, &info));
  gpu_stream stream = { .commands = HANDLE(VkCommandBuffer, 0x55) };
  const VkImageLayout layouts[] = { VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR,
    VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR };
  for (size_t i = 0; i < COUNTOF(layouts); i++) {
    texture.layout = layouts[i];
    for (unsigned j = 0; j < 2; j++) {
      bool begin = j == 0;
      unsigned before = barriers;
      CHECK(gpu_texture_external_barrier(&stream, &texture, begin));
      CHECK(barriers == before + 1 && commands == stream.commands && !invalid);
      CHECK(dependency.sType == VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR && !dependency.pNext);
      CHECK(!dependency.dependencyFlags && !dependency.memoryBarrierCount && !dependency.bufferMemoryBarrierCount);
      CHECK(!dependency.pMemoryBarriers && !dependency.pBufferMemoryBarriers && dependency.imageMemoryBarrierCount == 1);
      CHECK(barrier.sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR && !barrier.pNext);
      CHECK(barrier.image == texture.handle);
      CHECK(barrier.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED && barrier.dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
      CHECK(barrier.oldLayout == (begin ? layouts[i] : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
      CHECK(barrier.newLayout == (begin ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : layouts[i]));
      CHECK(barrier.srcStageMask == (begin ? VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR : VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR));
      CHECK(barrier.dstStageMask == (begin ? VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR));
      VkAccessFlags2 memoryAccess = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
      CHECK(barrier.srcAccessMask == (begin ? memoryAccess : VK_ACCESS_2_TRANSFER_READ_BIT_KHR));
      CHECK(barrier.dstAccessMask == (begin ? VK_ACCESS_2_TRANSFER_READ_BIT_KHR : memoryAccess));
      CHECK(barrier.subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);
      CHECK(barrier.subresourceRange.baseMipLevel == 0 && barrier.subresourceRange.levelCount == 4);
      CHECK(barrier.subresourceRange.baseArrayLayer == 0 && barrier.subresourceRange.layerCount == 1);
      CHECK(texture.layout == layouts[i]);
      gpu_external_image image;
      CHECK(gpu_texture_get_external_image(&texture, &image) && image.image == 0x12345678);
    }
  }
  return true;
}

int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "gpu.external.descriptor", descriptor },
    { "gpu.external.rejection", rejection },
    { "gpu.external.ownership", ownership },
    { "gpu.external.transitions", transitions }
  };
  int status = nativeRunTests(argc, argv, tests, COUNTOF(tests));
  if (locksReady) {
    mtx_destroy(&state.allocatorLock);
    mtx_destroy(&state.morgue.lock);
  }
  return status;
}
