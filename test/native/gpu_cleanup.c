#include "test.h"
#include <stdlib.h>
#include <stdint.h>
#include <threads.h>
#include <dlfcn.h>

static char trace[1024];
static bool invalid;
static bool deviceGone;
static int locks;
static int mutexes;
static bool failMutexInit;
static mtx_t* readyMutexes[2];
static bool mutexReady(mtx_t* mutex) {
  return readyMutexes[0] == mutex || readyMutexes[1] == mutex;
}
static bool loaderPresent;
static void record(const char* event) {
  if (strlen(trace) + strlen(event) + 1 >= sizeof(trace)) abort();
  strcat(trace, event);
}
static int testMutexInit(mtx_t* mutex, int type) {
  if (failMutexInit) return thrd_error;
  int result = mtx_init(mutex, type);
  if (result == thrd_success) {
    if (mutexes >= 2 || mutexReady(mutex)) abort();
    readyMutexes[mutexes++] = mutex;
  }
  return result;
}
static int testMutexLock(mtx_t* mutex) {
  if (!mutexReady(mutex)) { invalid = true; return thrd_error; }
  locks++;
  return mtx_lock(mutex);
}
static int testMutexUnlock(mtx_t* mutex) {
  if (!locks) { invalid = true; return thrd_error; }
  locks--;
  return mtx_unlock(mutex);
}
static void testMutexDestroy(mtx_t* mutex) {
  if (!mutexReady(mutex)) { invalid = true; return; }
  for (size_t i = 0; i < 2; i++) {
    if (readyMutexes[i] == mutex) readyMutexes[i] = NULL;
  }
  mutexes--;
  mtx_destroy(mutex);
}
static void* testOpen(const char* path, int flags) {
  (void) path; (void) flags;
  return loaderPresent ? (void*) (uintptr_t) 1 : NULL;
}
static void* testSymbol(void* library, const char* name);
static int testClose(void* library) {
  (void) library;
  record("unload,");
  return 0;
}
#define mtx_init testMutexInit
#define mtx_lock testMutexLock
#define mtx_unlock testMutexUnlock
#define mtx_destroy testMutexDestroy
#define dlopen testOpen
#define dlsym testSymbol
#define dlclose testClose
#include "../../src/core/gpu_vk.c"
#undef mtx_init
#undef mtx_lock
#undef mtx_unlock
#undef mtx_destroy
#undef dlopen
#undef dlsym
#undef dlclose

static VkResult idleResult;
static VKAPI_ATTR VkResult VKAPI_CALL idle(VkDevice device) {
  if (!device || deviceGone) invalid = true;
  record("idle,");
  return idleResult;
}
#define DESTROY(name, Type, event)   static VKAPI_ATTR void VKAPI_CALL name(VkDevice device, Type object, const VkAllocationCallbacks* allocator) {     (void) allocator;     if (!device || !object || deviceGone) invalid = true;     record(event);   }
DESTROY(buffer, VkBuffer, "buffer,")
DESTROY(commandPool, VkCommandPool, "pool,")
DESTROY(cache, VkPipelineCache, "cache,")
DESTROY(semaphore, VkSemaphore, "semaphore,")
DESTROY(memory, VkDeviceMemory, "memory,")
DESTROY(view, VkImageView, "view,")
DESTROY(swapchain, VkSwapchainKHR, "swapchain,")
static VKAPI_ATTR void VKAPI_CALL device(VkDevice handle, const VkAllocationCallbacks* allocator) {
  (void) allocator;
  if (!handle || deviceGone) invalid = true;
  record("device,");
  deviceGone = true;
}
#define INSTANCE_DESTROY(name, Type, event)   static VKAPI_ATTR void VKAPI_CALL name(VkInstance instance, Type object, const VkAllocationCallbacks* allocator) {     (void) allocator;     if (!instance || !object) invalid = true;     record(event);   }
INSTANCE_DESTROY(surface, VkSurfaceKHR, "surface,")
INSTANCE_DESTROY(messenger, VkDebugUtilsMessengerEXT, "messenger,")
static VKAPI_ATTR void VKAPI_CALL instance(VkInstance handle, const VkAllocationCallbacks* allocator) {
  (void) allocator;
  if (!handle) invalid = true;
  record("instance,");
}
static bool beforeDestroy(void) {
  if (state.config.vk.beforeDestroy || deviceGone) invalid = true;
  record("callback,");
  return true;
}
static VKAPI_ATTR VkResult VKAPI_CALL failLayers(uint32_t* count, VkLayerProperties* properties) {
  (void) count; (void) properties;
  return VK_ERROR_INITIALIZATION_FAILED;
}
static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL getProc(VkInstance handle, const char* name) {
  (void) handle;
  if (!strcmp(name, "vkEnumerateInstanceLayerProperties")) return (PFN_vkVoidFunction) failLayers;
  return NULL;
}
static void* testSymbol(void* library, const char* name) {
  (void) library; (void) name;
  return (void*) getProc;
}
static void reset(void) {
  memset(&state, 0, sizeof(state));
  trace[0] = 0;
  idleResult = VK_SUCCESS;
  invalid = deviceGone = loaderPresent = false;
  mutexes = locks = 0;
  failMutexInit = false;
  memset(readyMutexes, 0, sizeof(readyMutexes));
  state.config.fnAlloc = malloc;
  state.config.fnFree = free;
  state.config.vk.beforeDestroy = beforeDestroy;
  vkDeviceWaitIdle = idle;
  vkDestroyBuffer = buffer;
  vkDestroyCommandPool = commandPool;
  vkDestroyPipelineCache = cache;
  vkDestroySemaphore = semaphore;
  vkFreeMemory = memory;
  vkDestroyImageView = view;
  vkDestroySwapchainKHR = swapchain;
  vkDestroyDevice = device;
  vkDestroySurfaceKHR = surface;
  vkDestroyDebugUtilsMessengerEXT = messenger;
  vkDestroyInstance = instance;
}
#define HANDLE(Type) ((Type) (uintptr_t) 1)
static bool normal(void) {
  reset();
  state.device = HANDLE(VkDevice);
  state.instance = HANDLE(VkInstance);
  state.morgueLockReady = testMutexInit(&state.morgue.lock, mtx_plain) == thrd_success;
  state.allocatorLockReady = testMutexInit(&state.allocatorLock, mtx_plain) == thrd_success;
  CHECK(state.morgueLockReady && state.allocatorLockReady);
  gpu_victim* victim = calloc(1, sizeof(*victim));
  CHECK(victim);
  victim->type = VK_OBJECT_TYPE_BUFFER;
  victim->handle = (void*) (uintptr_t) 1;
  state.morgue.head = state.morgue.tail = victim;
  gpu_thread_state t = { 0 };
  gpu_stream_pool* pool = calloc(1, sizeof(*pool));
  CHECK(pool);
  pool->handle = HANDLE(VkCommandPool);
  t.streamPools = pool;
  state.threads = &t;
  state.pipelineCache = HANDLE(VkPipelineCache);
  state.semaphore = HANDLE(VkSemaphore);
  state.memory[0].handle = HANDLE(VkDeviceMemory);
  state.surface.acquireSemaphores[0] = HANDLE(VkSemaphore);
  state.surface.presentSemaphores[0] = HANDLE(VkSemaphore);
  state.surface.images[0].view = HANDLE(VkImageView);
  state.surface.swapchain = HANDLE(VkSwapchainKHR);
  state.surface.handle = HANDLE(VkSurfaceKHR);
  state.messenger = HANDLE(VkDebugUtilsMessengerEXT);
  gpu_destroy();
  CHECK(!invalid && !mutexes && !locks);
  CHECK(!strcmp(trace, "idle,callback,buffer,pool,cache,semaphore,memory,semaphore,semaphore,view,swapchain,device,surface,messenger,instance,"));
  gpu_destroy();
  CHECK(!invalid && !strcmp(trace, "idle,callback,buffer,pool,cache,semaphore,memory,semaphore,semaphore,view,swapchain,device,surface,messenger,instance,"));
  return true;
}
static bool partial(void) {
  reset();
  state.device = HANDLE(VkDevice);
  state.instance = HANDLE(VkInstance);
  state.semaphore = HANDLE(VkSemaphore);
  state.allocatorLockReady = testMutexInit(&state.allocatorLock, mtx_plain) == thrd_success;
  CHECK(state.allocatorLockReady);
  gpu_destroy();
  CHECK(!invalid && !mutexes && !locks);
  CHECK(!strcmp(trace, "idle,callback,semaphore,device,instance,"));
  reset();
  state.instance = HANDLE(VkInstance);
  state.messenger = HANDLE(VkDebugUtilsMessengerEXT);
  gpu_destroy();
  CHECK(!invalid && !mutexes && !locks);
  CHECK(!strcmp(trace, "callback,messenger,instance,"));
  return true;
}
static bool absent(void) {
  reset();
  state.config.vk.beforeDestroy = NULL;
  gpu_destroy();
  gpu_destroy();
  CHECK(!invalid && !mutexes && !locks && !trace[0]);
  state.device = HANDLE(VkDevice);
  gpu_destroy();
  CHECK(!invalid && !strcmp(trace, "idle,device,"));
  return true;
}
static bool initFailure(void) {
  reset();
  gpu_config config = state.config;
  memset(&state, 0, sizeof(state));
  CHECK(!gpu_init(&config));
  CHECK(!strcmp(trace, "callback,") && !invalid && !mutexes && !locks);
  reset();
  memset(&state, 0, sizeof(state));
  loaderPresent = true;
  CHECK(!gpu_init(&config));
  CHECK(!strcmp(trace, "callback,unload,") && !invalid && !mutexes && !locks);
  gpu_destroy();
  CHECK(!strcmp(trace, "callback,unload,") && !invalid);
  return true;
}
static bool retireRuntime(void) {
  beforeDestroy();
  gpu_texture texture = { .view = HANDLE(VkImageView), .imported = true };
  gpu_texture_destroy(&texture);
  record("retire,");
  if (!gpu_wait_idle()) invalid = true;
  if (state.morgue.head || deviceGone) invalid = true;
  record("runtime-images,");
  return true;
}
static bool runtimeRetirement(void) {
  reset();
  state.device = HANDLE(VkDevice);
  state.instance = HANDLE(VkInstance);
  state.morgueLockReady = testMutexInit(&state.morgue.lock, mtx_plain) == thrd_success;
  CHECK(state.morgueLockReady);
  state.config.vk.beforeDestroy = retireRuntime;
  gpu_destroy();
  CHECK(!invalid && !mutexes && !locks);
  CHECK(!strcmp(trace, "idle,callback,retire,idle,view,runtime-images,device,instance,"));
  gpu_flush_deferred_after_idle();
  gpu_destroy();
  CHECK(!invalid && !strcmp(trace, "idle,callback,retire,idle,view,runtime-images,device,instance,"));
  reset();
  state.device = HANDLE(VkDevice);
  state.morgueLockReady = testMutexInit(&state.morgue.lock, mtx_plain) == thrd_success;
  CHECK(state.morgueLockReady);
  gpu_texture texture = { .view = HANDLE(VkImageView), .imported = true };
  gpu_texture_destroy(&texture);
  idleResult = VK_ERROR_DEVICE_LOST;
  CHECK(!gpu_wait_idle());
  CHECK(state.morgue.head && !invalid && !strcmp(trace, "idle,"));
  idleResult = VK_SUCCESS;
  CHECK(gpu_wait_idle());
  CHECK(!state.morgue.head && !invalid && !strcmp(trace, "idle,idle,view,"));
  gpu_texture_destroy(&texture);
  gpu_flush_deferred_after_idle();
  CHECK(!state.morgue.head && !invalid && !strcmp(trace, "idle,idle,view,view,"));
  gpu_destroy();
  CHECK(!invalid && !mutexes && !locks);
  return true;
}
static bool retireAfterIdle(void) {
  beforeDestroy();
  gpu_texture texture = { .view = HANDLE(VkImageView), .imported = true };
  gpu_texture_destroy(&texture);
  record("retire,");
  gpu_flush_deferred_after_idle();
  if (state.morgue.head || deviceGone) invalid = true;
  record("runtime-images,");
  return true;
}
static bool waitErrors(void) {
  reset();
  state.device = HANDLE(VkDevice);
  state.instance = HANDLE(VkInstance);
  state.morgueLockReady = testMutexInit(&state.morgue.lock, mtx_plain) == thrd_success;
  CHECK(state.morgueLockReady);
  state.config.vk.beforeDestroy = retireAfterIdle;
  idleResult = VK_ERROR_OUT_OF_HOST_MEMORY;
  gpu_destroy();
  CHECK(!invalid && mutexes == 1 && !locks && !strcmp(trace, "idle,"));
  CHECK(state.device && state.instance && state.config.vk.beforeDestroy == retireAfterIdle);
  CHECK(strstr(gpu_get_error(), "vkDeviceWaitIdle"));
  idleResult = VK_SUCCESS;
  gpu_destroy();
  CHECK(!invalid && !mutexes && !locks);
  CHECK(!strcmp(trace, "idle,idle,callback,retire,view,runtime-images,device,instance,"));
  reset();
  state.device = HANDLE(VkDevice);
  state.instance = HANDLE(VkInstance);
  state.morgueLockReady = testMutexInit(&state.morgue.lock, mtx_plain) == thrd_success;
  CHECK(state.morgueLockReady);
  state.config.vk.beforeDestroy = retireAfterIdle;
  idleResult = VK_ERROR_DEVICE_LOST;
  gpu_destroy();
  CHECK(!invalid && !mutexes && !locks);
  CHECK(!strcmp(trace, "idle,callback,retire,view,runtime-images,device,instance,"));
  gpu_destroy();
  CHECK(!invalid && !strcmp(trace, "idle,callback,retire,view,runtime-images,device,instance,"));
  return true;
}
static bool prepareTeardown(void) {
  reset();
  CHECK(gpu_prepare_teardown());
  CHECK(!invalid && !trace[0]);
  state.device = HANDLE(VkDevice);
  state.morgueLockReady = testMutexInit(&state.morgue.lock, mtx_plain) == thrd_success;
  CHECK(state.morgueLockReady);
  gpu_texture texture = { .view = HANDLE(VkImageView), .imported = true };
  gpu_texture_destroy(&texture);
  idleResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
  CHECK(!gpu_prepare_teardown());
  CHECK(!invalid && state.morgue.head && !strcmp(trace, "idle,"));
  CHECK(state.config.vk.beforeDestroy == beforeDestroy && mutexes == 1);
  idleResult = VK_SUCCESS;
  CHECK(gpu_prepare_teardown());
  CHECK(!invalid && !state.morgue.head && !strcmp(trace, "idle,idle,view,"));
  gpu_texture_destroy(&texture);
  idleResult = VK_ERROR_DEVICE_LOST;
  CHECK(!gpu_wait_idle());
  CHECK(state.morgue.head && !invalid && !strcmp(trace, "idle,idle,view,idle,"));
  CHECK(gpu_prepare_teardown());
  CHECK(!state.morgue.head && !invalid && !strcmp(trace, "idle,idle,view,idle,idle,view,"));
  gpu_destroy();
  CHECK(!invalid && !mutexes && !locks);
  return true;
}
static bool mutexFailure(void) {
  reset();
  failMutexInit = true;
  state.allocatorLockReady = testMutexInit(&state.allocatorLock, mtx_plain) == thrd_success;
  state.morgueLockReady = testMutexInit(&state.morgue.lock, mtx_plain) == thrd_success;
  CHECK(!state.allocatorLockReady && !state.morgueLockReady);
  state.device = HANDLE(VkDevice);
  gpu_destroy();
  CHECK(!invalid && !mutexes && !locks && !strcmp(trace, "idle,callback,device,"));
  reset();
  state.allocatorLockReady = testMutexInit(&state.allocatorLock, mtx_plain) == thrd_success;
  CHECK(state.allocatorLockReady);
  failMutexInit = true;
  state.morgueLockReady = testMutexInit(&state.morgue.lock, mtx_plain) == thrd_success;
  CHECK(!state.morgueLockReady);
  state.device = HANDLE(VkDevice);
  gpu_destroy();
  CHECK(!invalid && !mutexes && !locks && !strcmp(trace, "idle,callback,device,"));
  return true;
}
int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "gpu.normal", normal },
    { "gpu.partial", partial },
    { "gpu.absent", absent },
    { "gpu.init-failure", initFailure },
    { "gpu.runtime-retirement", runtimeRetirement },
    { "gpu.wait-errors", waitErrors },
    { "gpu.mutex-failure", mutexFailure },
    { "gpu.prepare-teardown", prepareTeardown }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
