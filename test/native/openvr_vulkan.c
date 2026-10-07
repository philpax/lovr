#include "test.h"
#include "headset/openvr_vulkan.h"
#include <stdint.h>
#include <stdlib.h>

static struct {
  const char* extensions;
  unsigned queryMode;
  unsigned queries;
  unsigned allocations;
  unsigned failAllocation;
  unsigned live;
  unsigned creations;
  uint64_t adapter;
  VkResult result;
  VkResult enumerateResult;
  bool mutateEnumeration;
  unsigned enumerations;
  bool empty;
  bool zeroCapacity;
  unsigned enumerationMode;
  const VkAllocationCallbacks* allocator;
} state;

static void* allocate(size_t size) {
  if (++state.allocations == state.failAllocation) return NULL;
  void* pointer = malloc(size);
  if (pointer) state.live++;
  return pointer;
}

static void deallocate(void* pointer) {
  if (pointer) state.live--;
  free(pointer);
}

static uint32_t OPENVR_FNTABLE_CALLTYPE instanceExtensions(char* buffer, uint32_t size) {
  state.queries++;
  if (state.queryMode == 1) return 0;
  if (state.queryMode == 2) return UINT32_MAX;
  if (state.queryMode == 8 && buffer) return 0;
  uint32_t needed = (uint32_t) strlen(state.extensions) + 1;
  if (state.queryMode == 3 && !buffer) return 2;
  if (state.queryMode == 7 && !buffer) return needed + 64;
  if (state.queryMode == 5) return size + 1;
  if (buffer && size >= needed) {
    memcpy(buffer, state.extensions, needed);
    if (state.queryMode == 4) buffer[needed - 1] = 'x';
    if (state.queryMode == 6 && needed > 2) buffer[1] = '\0';
  }
  return needed;
}

static uint32_t OPENVR_FNTABLE_CALLTYPE deviceExtensions(struct VkPhysicalDevice_T* device, char* buffer, uint32_t size) {
  if (device != (VkPhysicalDevice) (uintptr_t) 2) return 0;
  return instanceExtensions(buffer, size);
}

static void OPENVR_FNTABLE_CALLTYPE outputDevice(uint64_t* device, ETextureType type, struct VkInstance_T* instance) {
  *device = type == ETextureType_TextureType_Vulkan && instance == (VkInstance) (uintptr_t) 1 ? state.adapter : 0;
}

static VkResult VKAPI_CALL enumerate(VkInstance instance, uint32_t* count, VkPhysicalDevice* devices) {
  if (instance != (VkInstance) (uintptr_t) 1) return VK_ERROR_INITIALIZATION_FAILED;
  if (state.enumerateResult != VK_SUCCESS) return state.enumerateResult;
  state.enumerations++;
  if (!devices) {
    if (state.enumerationMode == 1) { *count = 0; return VK_SUCCESS; }
    if (state.enumerationMode == 2) { *count = UINT32_MAX; return VK_SUCCESS; }
    *count = state.mutateEnumeration && state.enumerations == 1 ? 1 : 2;
    return VK_SUCCESS;
  }
  if (state.enumerationMode == 3) return VK_INCOMPLETE;
  if (state.enumerationMode == 4) return VK_ERROR_DEVICE_LOST;
  if (*count < 2) return VK_INCOMPLETE;
  devices[0] = (VkPhysicalDevice) (uintptr_t) 3;
  devices[1] = (VkPhysicalDevice) (uintptr_t) 2;
  *count = 2;
  return VK_SUCCESS;
}

static bool merged(uint32_t count, const char* const* names) {
  if (state.zeroCapacity) return count == 0 && names == NULL;
  if (state.empty) return count == 2 && !strcmp(names[0], "VK_APP") && !strcmp(names[1], "VK_SHARED");
  return count == 3 && !strcmp(names[0], "VK_APP") && !strcmp(names[1], "VK_SHARED") && !strcmp(names[2], "VK_RUNTIME");
}

static VkInstanceCreateInfo instanceInfo;
static VkDeviceCreateInfo deviceInfo;

static VkResult VKAPI_CALL createInstance(const VkInstanceCreateInfo* info, const VkAllocationCallbacks* allocator, VkInstance* instance) {
  state.creations++;
  if (!merged(info->enabledExtensionCount, info->ppEnabledExtensionNames) || allocator != state.allocator || info->flags != 7 || info->pNext != &state) return VK_ERROR_UNKNOWN;
  if (info->pApplicationInfo != instanceInfo.pApplicationInfo || info->enabledLayerCount != instanceInfo.enabledLayerCount ||
      info->ppEnabledLayerNames != instanceInfo.ppEnabledLayerNames) return VK_ERROR_UNKNOWN;
  *instance = (VkInstance) (uintptr_t) 1;
  return state.result;
}

static VkResult VKAPI_CALL createDevice(VkPhysicalDevice device, const VkDeviceCreateInfo* info, const VkAllocationCallbacks* allocator, VkDevice* output) {
  state.creations++;
  if (device != (VkPhysicalDevice) (uintptr_t) 2 || !merged(info->enabledExtensionCount, info->ppEnabledExtensionNames) || allocator != state.allocator || info->flags != 7 || info->pNext != &state) return VK_ERROR_UNKNOWN;
  if (info->queueCreateInfoCount != deviceInfo.queueCreateInfoCount || info->pQueueCreateInfos != deviceInfo.pQueueCreateInfos ||
      info->pEnabledFeatures != deviceInfo.pEnabledFeatures || info->enabledLayerCount != deviceInfo.enabledLayerCount ||
      info->ppEnabledLayerNames != deviceInfo.ppEnabledLayerNames) return VK_ERROR_UNKNOWN;
  *output = (VkDevice) (uintptr_t) 4;
  return state.result;
}

static OpenVRVulkan context;

static void reset(void) {
  memset(&state, 0, sizeof(state));
  state.extensions = " VK_SHARED  VK_RUNTIME VK_RUNTIME ";
  state.adapter = 2;
  static struct VR_IVRSystem_FnTable system = { .GetOutputDevice = outputDevice };
  static struct VR_IVRCompositor_FnTable compositor = {
    .GetVulkanInstanceExtensionsRequired = instanceExtensions,
    .GetVulkanDeviceExtensionsRequired = deviceExtensions
  };
  static OpenVRRuntime runtime = { .system = &system, .compositor = &compositor, .initialized = true };
  context = (OpenVRVulkan) { &runtime, createInstance, createDevice, enumerate, allocate, deallocate };
  static const char* names[] = { "VK_APP", "VK_SHARED", "VK_APP" };
  instanceInfo = (VkInstanceCreateInfo) { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pNext = &state,
    .flags = 7, .enabledExtensionCount = 3, .ppEnabledExtensionNames = names };
  deviceInfo = (VkDeviceCreateInfo) { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = &state,
    .flags = 7, .enabledExtensionCount = 3, .ppEnabledExtensionNames = names };
}

static bool negotiation(void) {
  reset();
  VkInstance instance;
  VkPhysicalDevice physical;
  VkDevice device;
  CHECK(lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, NULL, &instance) == VK_SUCCESS);
  CHECK(instance == (VkInstance) (uintptr_t) 1);
  CHECK(lovrOpenVRVulkanGetPhysicalDevice(&context, instance, &physical) == VK_SUCCESS);
  CHECK(physical == (VkPhysicalDevice) (uintptr_t) 2);
  CHECK(lovrOpenVRVulkanCreateDevice(&context, instance, physical, &deviceInfo, NULL, &device) == VK_SUCCESS);
  CHECK(state.live == 0 && state.creations == 2);
  CHECK(instanceInfo.enabledExtensionCount == 3 && deviceInfo.enabledExtensionCount == 3);
  VkAllocationCallbacks allocator = { 0 };
  VkApplicationInfo application = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_1 };
  const char* layers[] = { "VK_LAYER_fixture" };
  VkDeviceQueueCreateInfo queue = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = 5 };
  VkPhysicalDeviceFeatures features = { .geometryShader = VK_TRUE };
  state.allocator = &allocator;
  instanceInfo.pApplicationInfo = &application;
  instanceInfo.enabledLayerCount = 1;
  instanceInfo.ppEnabledLayerNames = layers;
  deviceInfo.enabledLayerCount = 1;
  deviceInfo.ppEnabledLayerNames = layers;
  deviceInfo.queueCreateInfoCount = 1;
  deviceInfo.pQueueCreateInfos = &queue;
  deviceInfo.pEnabledFeatures = &features;
  CHECK(lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, &allocator, &instance) == VK_SUCCESS);
  CHECK(lovrOpenVRVulkanCreateDevice(&context, instance, physical, &deviceInfo, &allocator, &device) == VK_SUCCESS);
  CHECK(state.live == 0 && state.creations == 4);
  return true;
}

static bool queryErrors(void) {
  for (unsigned mode = 1; mode <= 8; mode++) {
    reset();
    state.queryMode = mode;
    VkInstance instance = (VkInstance) (uintptr_t) 99;
    VkResult result = lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, NULL, &instance);
    CHECK((mode == 3 || mode == 7) ? result == VK_SUCCESS : result == VK_ERROR_INITIALIZATION_FAILED);
    CHECK((mode == 3 || mode == 7) ? state.creations == 1 : state.creations == 0);
    CHECK((mode == 3 || mode == 7) ? instance == (VkInstance) (uintptr_t) 1 : instance == VK_NULL_HANDLE);
    CHECK(state.live == 0 && state.queries < 12);
    reset();
    state.queryMode = mode;
    VkDevice device = (VkDevice) (uintptr_t) 99;
    result = lovrOpenVRVulkanCreateDevice(&context, (VkInstance) (uintptr_t) 1,
      (VkPhysicalDevice) (uintptr_t) 2, &deviceInfo, NULL, &device);
    CHECK((mode == 3 || mode == 7) ? result == VK_SUCCESS : result == VK_ERROR_INITIALIZATION_FAILED);
    CHECK((mode == 3 || mode == 7) ? state.creations == 1 : state.creations == 0);
    CHECK((mode == 3 || mode == 7) ? device == (VkDevice) (uintptr_t) 4 : device == VK_NULL_HANDLE);
    CHECK(state.live == 0 && state.queries < 12);
  }
  return true;
}

static bool emptyAndInvalid(void) {
  reset();
  state.extensions = "";
  state.empty = true;
  VkInstance instance;
  CHECK(lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, NULL, &instance) == VK_SUCCESS);
  CHECK(state.creations == 1 && state.live == 0);
  state.extensions = "   ";
  CHECK(lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, NULL, &instance) == VK_SUCCESS);
  VkDevice device;
  CHECK(lovrOpenVRVulkanCreateDevice(&context, (VkInstance) (uintptr_t) 1, (VkPhysicalDevice) (uintptr_t) 2,
    &deviceInfo, NULL, &device) == VK_SUCCESS);
  CHECK(state.live == 0);
  reset();
  state.zeroCapacity = true;
  state.extensions = "";
  instanceInfo.enabledExtensionCount = 0;
  instanceInfo.ppEnabledExtensionNames = NULL;
  deviceInfo.enabledExtensionCount = 0;
  deviceInfo.ppEnabledExtensionNames = NULL;
  CHECK(lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, NULL, &instance) == VK_SUCCESS);
  CHECK(lovrOpenVRVulkanCreateDevice(&context, (VkInstance) (uintptr_t) 1, (VkPhysicalDevice) (uintptr_t) 2,
    &deviceInfo, NULL, &device) == VK_SUCCESS);
  CHECK(state.live == 0);
  reset();
  state.extensions = "VK_RUNTIME invalid-name";
  CHECK(lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, NULL, &instance) == VK_ERROR_INITIALIZATION_FAILED);
  CHECK(state.creations == 0 && state.live == 0);
  reset();
  instanceInfo.ppEnabledExtensionNames = NULL;
  CHECK(lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, NULL, &instance) == VK_ERROR_INITIALIZATION_FAILED);
  CHECK(state.live == 0);
  reset();
  const char* invalid[] = { "VK_APP VK_OTHER" };
  instanceInfo.enabledExtensionCount = 1;
  instanceInfo.ppEnabledExtensionNames = invalid;
  CHECK(lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, NULL, &instance) == VK_ERROR_INITIALIZATION_FAILED);
  CHECK(state.creations == 0 && state.live == 0);
  reset();
  instanceInfo.enabledExtensionCount = UINT32_MAX;
  CHECK(lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, NULL, &instance) == VK_ERROR_INITIALIZATION_FAILED);
  CHECK(state.creations == 0 && state.queries == 0 && state.live == 0);
  reset();
  state.extensions = "VK_RUNTIME\tVK_OTHER";
  CHECK(lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, NULL, &instance) == VK_ERROR_INITIALIZATION_FAILED);
  CHECK(state.creations == 0 && state.live == 0);
  return true;
}

static bool allocationErrors(void) {
  reset();
  VkInstance instance;
  CHECK(lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, NULL, &instance) == VK_SUCCESS);
  unsigned allocations = state.allocations;
  for (unsigned i = 1; i <= allocations; i++) {
    reset();
    state.failAllocation = i;
    instance = (VkInstance) (uintptr_t) 99;
    CHECK(lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, NULL, &instance) == VK_ERROR_OUT_OF_HOST_MEMORY);
    CHECK(state.live == 0 && state.creations == 0 && instance == VK_NULL_HANDLE);
  }
  reset();
  VkDevice device;
  CHECK(lovrOpenVRVulkanCreateDevice(&context, (VkInstance) (uintptr_t) 1, (VkPhysicalDevice) (uintptr_t) 2, &deviceInfo, NULL, &device) == VK_SUCCESS);
  allocations = state.allocations;
  for (unsigned i = 1; i <= allocations; i++) {
    reset();
    state.failAllocation = i;
    device = (VkDevice) (uintptr_t) 99;
    CHECK(lovrOpenVRVulkanCreateDevice(&context, (VkInstance) (uintptr_t) 1, (VkPhysicalDevice) (uintptr_t) 2, &deviceInfo, NULL, &device) == VK_ERROR_OUT_OF_HOST_MEMORY);
    CHECK(state.live == 0 && state.creations == 0 && device == VK_NULL_HANDLE);
  }
  return true;
}

static bool adapterErrors(void) {
  for (unsigned i = 0; i < 3; i++) {
    reset();
    if (i == 0) state.adapter = 0;
    if (i == 1) state.adapter = 9;
    if (i == 2) state.enumerateResult = VK_ERROR_DEVICE_LOST;
    VkDevice device = (VkDevice) (uintptr_t) 99;
    VkResult result = lovrOpenVRVulkanCreateDevice(&context, (VkInstance) (uintptr_t) 1,
      (VkPhysicalDevice) (uintptr_t) 2, &deviceInfo, NULL, &device);
    CHECK(result == (i == 2 ? VK_ERROR_DEVICE_LOST : VK_ERROR_INITIALIZATION_FAILED));
    CHECK(state.creations == 0 && state.live == 0 && device == VK_NULL_HANDLE);
  }
  reset();
  VkDevice device;
  CHECK(lovrOpenVRVulkanCreateDevice(&context, (VkInstance) (uintptr_t) 1,
    (VkPhysicalDevice) (uintptr_t) 3, &deviceInfo, NULL, &device) == VK_ERROR_INITIALIZATION_FAILED);
  CHECK(state.creations == 0 && state.live == 0);
  reset();
  state.mutateEnumeration = true;
  VkPhysicalDevice physical;
  CHECK(lovrOpenVRVulkanGetPhysicalDevice(&context, (VkInstance) (uintptr_t) 1, &physical) == VK_SUCCESS);
  CHECK(physical == (VkPhysicalDevice) (uintptr_t) 2 && state.live == 0);
  for (unsigned mode = 1; mode <= 4; mode++) {
    reset();
    state.enumerationMode = mode;
    physical = (VkPhysicalDevice) (uintptr_t) 99;
    CHECK(lovrOpenVRVulkanGetPhysicalDevice(&context, (VkInstance) (uintptr_t) 1, &physical) ==
      (mode == 4 ? VK_ERROR_DEVICE_LOST : VK_ERROR_INITIALIZATION_FAILED));
    CHECK(physical == VK_NULL_HANDLE && state.live == 0 && state.creations == 0);
    CHECK(state.enumerations == (mode == 3 ? 8 : mode == 4 ? 2 : 1));
  }
  return true;
}

static bool vulkanErrors(void) {
  reset();
  state.result = VK_ERROR_EXTENSION_NOT_PRESENT;
  VkInstance instance;
  CHECK(lovrOpenVRVulkanCreateInstance(&context, &instanceInfo, NULL, &instance) == state.result);
  CHECK(state.live == 0 && state.creations == 1 && instance == VK_NULL_HANDLE);
  VkDevice device;
  CHECK(lovrOpenVRVulkanCreateDevice(&context, (VkInstance) (uintptr_t) 1, (VkPhysicalDevice) (uintptr_t) 2,
    &deviceInfo, NULL, &device) == state.result);
  CHECK(state.live == 0 && state.creations == 2 && device == VK_NULL_HANDLE);
  return true;
}

int main(int argc, char** argv) {
  NativeTest tests[] = {
    { "openvr.vulkan.negotiation", negotiation },
    { "openvr.vulkan.query-errors", queryErrors },
    { "openvr.vulkan.empty-and-invalid", emptyAndInvalid },
    { "openvr.vulkan.allocation-errors", allocationErrors },
    { "openvr.vulkan.adapter-errors", adapterErrors },
    { "openvr.vulkan.vulkan-errors", vulkanErrors }
  };
  return nativeRunTests(argc, argv, tests, sizeof(tests) / sizeof(tests[0]));
}
