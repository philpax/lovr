#define state gpuState
#define main gpuCleanupTestsMain
#include "gpu_cleanup.c"
#undef main
#undef state
#include "util.h"

static atomic_uint ref;
static _Thread_local bool externalHandoff;
typedef struct MaterialBlock {
  struct MaterialBlock* next;
  gpu_bundle_pool* bundlePool;
  gpu_buffer* buffer;
  void* materials;
  void* bundles;
} MaterialBlock;
typedef struct BundlePool {
  struct BundlePool* next;
  gpu_bundle_pool* gpu;
  void* bundles;
} BundlePool;
typedef struct Layout {
  struct Layout* next;
  BundlePool* head;
  gpu_layout* gpu;
  mtx_t lock;
} Layout;
typedef struct Readback { struct Readback* next; } Readback;
typedef struct { void* sync; } Window;
static struct {
  bool initialized;
  Readback* readbacks;
  void* timingReadback;
  gpu_tally* timestamps;
  Window* window;
  void* windowPass;
  void* defaultFont;
  void* defaultBuffer;
  void* defaultTexture;
  void* defaultMaterial;
  void* defaultSamplers[2];
  void* defaultShaders[1];
  MaterialBlock* materials;
  uint32_t pipelineCount;
  void* pipelines;
  void* pipelineLookup;
  void* bufferAllocators[1];
  Layout* layouts;
  mtx_t lock;
  bool lockReady;
} state;
static struct { struct { void* memory; } stack; } shellThread;
static void shellFree(void* pointer) {
  if (pointer) record("shell-free,");
  free(pointer);
}
static bool disconnectFails;
static bool partialShell;
static bool shellDisconnect(void) {
  if ((!state.initialized && !partialShell) || !state.pipelines || !shellThread.stack.memory) invalid = true;
  if (disconnectFails) {
    beforeDestroy();
    return false;
  }
  if (mtx_trylock(&state.lock) != thrd_success) invalid = true;
  else mtx_unlock(&state.lock);
  if (!gpu_prepare_teardown()) invalid = true;
  retireAfterIdle();
  idleResult = VK_ERROR_OUT_OF_HOST_MEMORY;
  return true;
}
static void shellMutexDestroy(mtx_t* mutex) {
  if (mutex == &state.lock && !state.lockReady) { invalid = true; return; }
  mtx_destroy(mutex);
}
#define mtx_destroy shellMutexDestroy
#define lovrHeadsetStop() record("stop,")
#define thread shellThread
#define lovrFree shellFree
#define lovrRelease(object, destructor) ((void) (object))
#define map_free(map) ((void) (map))
#define destroyBuffers(allocator) ((void) (allocator))
#define getPipeline(index) ((gpu_pipeline*) NULL)
#include "graphics_destroy.inc"
#undef mtx_destroy
#undef thread
#undef lovrFree
#undef lovrRelease
#undef map_free
#undef destroyBuffers
#undef getPipeline

static bool shellRetry(void) {
  reset();
  memset(&state, 0, sizeof(state));
  atomic_store(&ref, 0);
  CHECK(lovrModuleAcquire(&ref));
  if (!partialShell) lovrModuleReady(&ref);
  state.initialized = !partialShell;
  state.pipelines = malloc(1);
  shellThread.stack.memory = malloc(1);
  CHECK(state.pipelines && shellThread.stack.memory);
  CHECK(mtx_init(&state.lock, mtx_plain) == thrd_success);
  state.lockReady = true;
  gpuState.device = HANDLE(VkDevice);
  gpuState.instance = HANDLE(VkInstance);
  gpuState.config.vk.beforeDestroy = shellDisconnect;
  gpuState.morgueLockReady = testMutexInit(&gpuState.morgue.lock, mtx_plain) == thrd_success;
  CHECK(gpuState.morgueLockReady);
  unsigned reference = atomic_load(&ref);
  idleResult = VK_ERROR_OUT_OF_HOST_MEMORY;
  lovrGraphicsDestroy();
  CHECK(!invalid && !strcmp(trace, "idle,"));
  CHECK(atomic_load(&ref) == reference && state.initialized == !partialShell && state.pipelines && shellThread.stack.memory);
  CHECK(gpuState.device && gpuState.config.vk.beforeDestroy == shellDisconnect && mutexes == 1);
  idleResult = VK_SUCCESS;
  disconnectFails = true;
  lovrGraphicsDestroy();
  CHECK(!invalid && !strcmp(trace, "idle,idle,callback,"));
  CHECK(atomic_load(&ref) == reference && state.initialized == !partialShell && state.pipelines && shellThread.stack.memory);
  CHECK(gpuState.device && gpuState.config.vk.beforeDestroy == shellDisconnect && mutexes == 1);
  disconnectFails = false;
  lovrGraphicsDestroy();
  CHECK(!invalid && !mutexes && !locks && !gpuState.device);
  CHECK(!state.initialized && !state.pipelines && !shellThread.stack.memory && !atomic_load(&ref));
  CHECK(!strcmp(trace, "idle,idle,callback,callback,retire,view,runtime-images,shell-free,device,instance,shell-free,"));
  return true;
}
static bool shellPartialRetry(void) {
  partialShell = true;
  bool result = shellRetry();
  partialShell = false;
  return result;
}
static bool earlyDisconnect(void) {
  beforeDestroy();
  return !disconnectFails;
}
static bool earlyInitBranch(gpu_config gpu) {
#define thread shellThread
#define lovrFree shellFree
#include "graphics_init_failure.inc"
#undef thread
#undef lovrFree
  return true;
}
static bool earlyInitFailure(void) {
  reset();
  memset(&state, 0, sizeof(state));
  atomic_store(&ref, 0);
  CHECK(lovrModuleAcquire(&ref));
  unsigned reference = atomic_load(&ref);
  shellThread.stack.memory = malloc(1);
  CHECK(shellThread.stack.memory);
  gpu_config config = gpuState.config;
  config.vk.beforeDestroy = earlyDisconnect;
  memset(&gpuState, 0, sizeof(gpuState));
  disconnectFails = true;
  CHECK(!earlyInitBranch(config));
  CHECK(!invalid && !strcmp(trace, "callback,callback,"));
  CHECK(atomic_load(&ref) == reference && shellThread.stack.memory);
  CHECK(gpuState.config.vk.beforeDestroy == earlyDisconnect && gpuState.teardownPrepared);
  gpu_config replacement = config;
  replacement.vk.beforeDestroy = beforeDestroy;
  CHECK(!gpu_init(&replacement));
  CHECK(gpuState.config.vk.beforeDestroy == earlyDisconnect && !strcmp(trace, "callback,callback,"));
  disconnectFails = false;
  lovrGraphicsDestroy();
  CHECK(!invalid && !atomic_load(&ref) && !shellThread.stack.memory && !gpuState.config.fnAlloc);
  CHECK(!strcmp(trace, "callback,callback,callback,shell-free,"));
  config.vk.beforeDestroy = beforeDestroy;
  CHECK(!gpu_init(&config));
  CHECK(!gpuState.config.fnAlloc && !invalid);
  reset();
  memset(&state, 0, sizeof(state));
  atomic_store(&ref, 0);
  CHECK(lovrModuleAcquire(&ref));
  shellThread.stack.memory = malloc(1);
  CHECK(shellThread.stack.memory);
  failMutexInit = true;
  state.lockReady = testMutexInit(&state.lock, mtx_plain) == thrd_success;
  CHECK(!state.lockReady);
  lovrGraphicsDestroy();
  CHECK(!invalid && !atomic_load(&ref) && !shellThread.stack.memory);
  CHECK(!strcmp(trace, "callback,shell-free,"));
  return true;
}
int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "graphics.teardown-retry", shellRetry },
    { "graphics.partial-teardown-retry", shellPartialRetry },
    { "graphics.early-init-failure", earlyInitFailure }
  };
  return nativeRunTests(argc, argv, tests, 3);
}
