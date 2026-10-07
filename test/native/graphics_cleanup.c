#define state gpuState
#define main gpuCleanupTestsMain
#include "gpu_cleanup.c"
#undef main
#undef state
#include "util.h"
#include "../../src/modules/graphics/graphics.h"

static unsigned graphicsDestroyedCalls;
static bool selectionFrozen;
void lovrHeadsetGraphicsDestroyed(void) {
  graphicsDestroyedCalls++;
  selectionFrozen = false;
}
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
  gpu_device_info device;
  gpu_features features;
  gpu_limits limits;
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
  graphicsDestroyedCalls = 0;
  selectionFrozen = true;
  unsigned reference = atomic_load(&ref);
  idleResult = VK_ERROR_OUT_OF_HOST_MEMORY;
  lovrGraphicsDestroy();
  CHECK(!invalid && !strcmp(trace, "idle,"));
  CHECK(selectionFrozen && !graphicsDestroyedCalls);
  CHECK(atomic_load(&ref) == reference && state.initialized == !partialShell && state.pipelines && shellThread.stack.memory);
  CHECK(gpuState.device && gpuState.config.vk.beforeDestroy == shellDisconnect && mutexes == 1);
  idleResult = VK_SUCCESS;
  disconnectFails = true;
  lovrGraphicsDestroy();
  CHECK(!invalid && !strcmp(trace, "idle,idle,callback,"));
  CHECK(selectionFrozen && !graphicsDestroyedCalls);
  CHECK(atomic_load(&ref) == reference && state.initialized == !partialShell && state.pipelines && shellThread.stack.memory);
  CHECK(gpuState.device && gpuState.config.vk.beforeDestroy == shellDisconnect && mutexes == 1);
  disconnectFails = false;
  lovrGraphicsDestroy();
  CHECK(!invalid && !mutexes && !locks && !gpuState.device);
  CHECK(!selectionFrozen && graphicsDestroyedCalls == 1);
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
static bool prepareSucceeds;
static bool connectedVR;
static bool requiresPhysicalDevice;
static bool initGpuSucceeds;
static bool destroyGpuSucceeds;
static unsigned prepareCalls;
static unsigned gpuCalls;
static unsigned destroyCalls;
static unsigned requirementCalls;
static gpu_config capturedGpu;

static bool prepareGraphics(void) {
  prepareCalls++;
  record("prepare,");
  if (prepareSucceeds) selectionFrozen = true;
  return prepareSucceeds;
}
static bool requiresAdapter(void) {
  requirementCalls++;
  if (!prepareCalls) invalid = true;
  return requiresPhysicalDevice;
}
static bool captureGpuInit(gpu_config* config) {
  gpuCalls++;
  if (!prepareCalls) invalid = true;
  record("gpu,");
  capturedGpu = *config;
  return initGpuSucceeds;
}
static bool captureGpuDestroy(void) {
  destroyCalls++;
  record("destroy,");
  return destroyGpuSucceeds;
}
static void getPhysicalDevice(void* instance, uintptr_t device) { (void) instance; (void) device; }
static uint32_t createInstance(void* info, void* allocator, uintptr_t instance, void* proc) {
  (void) info; (void) allocator; (void) instance; (void) proc;
  return 0;
}
static uint32_t createDevice(void* instance, void* info, void* allocator, uintptr_t device, void* proc) {
  (void) instance; (void) info; (void) allocator; (void) device; (void) proc;
  return 0;
}
static bool initSelection(GraphicsConfig* config) {
#define LOVR_VK
#define thread shellThread
#define initAllocator(allocator) (shellThread.stack.memory ? shellThread.stack.memory : (shellThread.stack.memory = malloc(1)))
#define onMessage NULL
#define lockQueue NULL
#define unlockQueue NULL
#define lovrHeadsetPrepareGraphics prepareGraphics
#define lovrHeadsetRequiresPhysicalDevice requiresAdapter
#define lovrHeadsetIsConnected() connectedVR
#define lovrHeadsetGetVulkanPhysicalDevice getPhysicalDevice
#define lovrHeadsetCreateVulkanInstance createInstance
#define lovrHeadsetCreateVulkanDevice createDevice
#define lovrHeadsetBeforeGraphicsDestroy earlyDisconnect
#define gpu_init captureGpuInit
#define gpu_destroy captureGpuDestroy
#define lovrFree shellFree
#include "graphics_init_selection.inc"
#undef lovrFree
#undef gpu_destroy
#undef gpu_init
#undef lovrHeadsetBeforeGraphicsDestroy
#undef lovrHeadsetCreateVulkanDevice
#undef lovrHeadsetCreateVulkanInstance
#undef lovrHeadsetGetVulkanPhysicalDevice
#undef lovrHeadsetIsConnected
#undef lovrHeadsetRequiresPhysicalDevice
#undef lovrHeadsetPrepareGraphics
#undef unlockQueue
#undef lockQueue
#undef onMessage
#undef initAllocator
#undef thread
#undef LOVR_VK
  return true;
}
static void resetSelection(void) {
  reset();
  memset(&state, 0, sizeof(state));
  memset(&capturedGpu, 0, sizeof(capturedGpu));
  memset(&shellThread, 0, sizeof(shellThread));
  atomic_store(&ref, 0);
  prepareCalls = gpuCalls = destroyCalls = requirementCalls = graphicsDestroyedCalls = 0;
  selectionFrozen = false;
  prepareSucceeds = destroyGpuSucceeds = true;
  connectedVR = requiresPhysicalDevice = initGpuSucceeds = false;
}
static bool initSelectionOrdering(void) {
  for (unsigned connected = 0; connected < 2; connected++) {
    for (unsigned partial = 0; partial < 2; partial++) {
      resetSelection();
      connectedVR = connected;
      destroyGpuSucceeds = !partial;
      GraphicsConfig config = { .debug = true, .lowPower = true, .cacheData = &config, .cacheSize = 17 };
      CHECK(!initSelection(&config));
      CHECK(!invalid && prepareCalls == 1 && gpuCalls == 1 && destroyCalls == 1 && requirementCalls == 1);
      CHECK(!strcmp(trace, partial ? "prepare,gpu,destroy," : "prepare,gpu,destroy,shell-free,"));
      CHECK(capturedGpu.debug && capturedGpu.lowPower && capturedGpu.vk.cacheData == &config && capturedGpu.vk.cacheSize == 17);
      CHECK(selectionFrozen == (partial != 0));
      CHECK(graphicsDestroyedCalls == !partial);
      CHECK((atomic_load(&ref) != 0) == (partial != 0));
      CHECK((shellThread.stack.memory != NULL) == (partial != 0));
      free(shellThread.stack.memory);
    }
  }
  return true;
}
static bool initSelectionBlocked(void) {
  resetSelection();
  prepareSucceeds = false;
  GraphicsConfig config = { 0 };
  CHECK(!initSelection(&config));
  CHECK(!invalid && prepareCalls == 1 && !gpuCalls && !destroyCalls && !requirementCalls);
  CHECK(!strcmp(trace, "prepare,") && !atomic_load(&ref));
  free(shellThread.stack.memory);
  return true;
}
static bool initSelectionAdapter(void) {
  for (unsigned connected = 0; connected < 2; connected++) {
    for (unsigned required = 0; required < 2; required++) {
      resetSelection();
      connectedVR = connected;
      requiresPhysicalDevice = required;
      initGpuSucceeds = true;
      GraphicsConfig config = { 0 };
      CHECK(initSelection(&config));
      CHECK(!invalid && prepareCalls == 1 && gpuCalls == 1 && !destroyCalls && requirementCalls == 1);
      CHECK(!strcmp(trace, "prepare,gpu,"));
      CHECK(capturedGpu.vk.requirePhysicalDevice == (required != 0));
      CHECK(capturedGpu.vk.getPhysicalDevice == (connected ? getPhysicalDevice : NULL));
      CHECK(capturedGpu.vk.createInstance == (connected ? createInstance : NULL));
      CHECK(capturedGpu.vk.createDevice == (connected ? createDevice : NULL));
      CHECK(capturedGpu.vk.beforeDestroy == earlyDisconnect);
      lovrModuleReady(&ref);
      CHECK(initSelection(&config));
      CHECK(selectionFrozen && prepareCalls == 1 && gpuCalls == 1 && !graphicsDestroyedCalls);
      free(shellThread.stack.memory);
    }
  }
  return true;
}
int main(int argc, char** argv) {
  const NativeTest tests[] = {
    { "graphics.teardown-retry", shellRetry },
    { "graphics.partial-teardown-retry", shellPartialRetry },
    { "graphics.early-init-failure", earlyInitFailure },
    { "graphics.init-selection-ordering", initSelectionOrdering },
    { "graphics.init-selection-blocked", initSelectionBlocked },
    { "graphics.init-selection-adapter", initSelectionAdapter }
  };
  return nativeRunTests(argc, argv, tests, COUNTOF(tests));
}
