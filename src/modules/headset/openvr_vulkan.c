#include "openvr_vulkan.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EXTENSION_ENTRIES 4096u
#define MAX_EXTENSION_BYTES (MAX_EXTENSION_ENTRIES * VK_MAX_EXTENSION_NAME_SIZE)
#define MAX_PHYSICAL_DEVICES 4096u

static bool validContext(const OpenVRVulkan* context);
static void* allocate(const OpenVRVulkan* context, size_t size);
static void deallocate(const OpenVRVulkan* context, void* pointer);
static VkResult extensions(const OpenVRVulkan* context, VkPhysicalDevice device, uint32_t appCount,
  const char* const* appNames, char** buffer, const char*** names, uint32_t* count);

VkResult lovrOpenVRVulkanCreateInstance(const OpenVRVulkan* context, const VkInstanceCreateInfo* info,
  const VkAllocationCallbacks* allocator, VkInstance* instance) {
  if (!instance) return VK_ERROR_INITIALIZATION_FAILED;
  *instance = VK_NULL_HANDLE;
  if (!validContext(context) || !context->createInstance || !info ||
      info->sType != VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO) return VK_ERROR_INITIALIZATION_FAILED;
  char* buffer = NULL;
  const char** names = NULL;
  VkInstanceCreateInfo merged = *info;
  VkResult result = extensions(context, VK_NULL_HANDLE, info->enabledExtensionCount,
    info->ppEnabledExtensionNames, &buffer, &names, &merged.enabledExtensionCount);
  if (result == VK_SUCCESS) {
    merged.ppEnabledExtensionNames = names;
    result = context->createInstance(&merged, allocator, instance);
    if (result != VK_SUCCESS) *instance = VK_NULL_HANDLE;
  }
  deallocate(context, names);
  deallocate(context, buffer);
  return result;
}

VkResult lovrOpenVRVulkanGetPhysicalDevice(const OpenVRVulkan* context, VkInstance instance, VkPhysicalDevice* device) {
  if (!device) return VK_ERROR_INITIALIZATION_FAILED;
  *device = VK_NULL_HANDLE;
  if (!validContext(context) || !instance || !context->enumeratePhysicalDevices || !context->runtime->system ||
      !context->runtime->system->GetOutputDevice) return VK_ERROR_INITIALIZATION_FAILED;
  uint64_t output = 0;
  context->runtime->system->GetOutputDevice(&output, ETextureType_TextureType_Vulkan, instance);
  if (!output || output > UINTPTR_MAX) return VK_ERROR_INITIALIZATION_FAILED;
  VkPhysicalDevice required = (VkPhysicalDevice) (uintptr_t) output;
  for (unsigned attempt = 0; attempt < 4; attempt++) {
    uint32_t count = 0;
    VkResult result = context->enumeratePhysicalDevices(instance, &count, NULL);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) return result;
    if (!count || count > MAX_PHYSICAL_DEVICES) return VK_ERROR_INITIALIZATION_FAILED;
    VkPhysicalDevice* devices = allocate(context, count * sizeof(*devices));
    if (!devices) return VK_ERROR_OUT_OF_HOST_MEMORY;
    uint32_t capacity = count;
    result = context->enumeratePhysicalDevices(instance, &count, devices);
    if (result == VK_SUCCESS && count <= capacity) {
      for (uint32_t i = 0; i < count; i++) {
        if (devices[i] == required) *device = required;
      }
    }
    deallocate(context, devices);
    if (result == VK_INCOMPLETE) continue;
    if (result != VK_SUCCESS) return result;
    return *device ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
  }
  return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult lovrOpenVRVulkanCreateDevice(const OpenVRVulkan* context, VkInstance instance, VkPhysicalDevice device,
  const VkDeviceCreateInfo* info, const VkAllocationCallbacks* allocator, VkDevice* output) {
  if (!output) return VK_ERROR_INITIALIZATION_FAILED;
  *output = VK_NULL_HANDLE;
  if (!validContext(context) || !context->createDevice || !info ||
      info->sType != VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO) return VK_ERROR_INITIALIZATION_FAILED;
  VkPhysicalDevice required;
  VkResult result = lovrOpenVRVulkanGetPhysicalDevice(context, instance, &required);
  if (result != VK_SUCCESS) return result;
  if (device != required) return VK_ERROR_INITIALIZATION_FAILED;
  char* buffer = NULL;
  const char** names = NULL;
  VkDeviceCreateInfo merged = *info;
  result = extensions(context, device, info->enabledExtensionCount,
    info->ppEnabledExtensionNames, &buffer, &names, &merged.enabledExtensionCount);
  if (result == VK_SUCCESS) {
    merged.ppEnabledExtensionNames = names;
    result = context->createDevice(device, &merged, allocator, output);
    if (result != VK_SUCCESS) *output = VK_NULL_HANDLE;
  }
  deallocate(context, names);
  deallocate(context, buffer);
  return result;
}

static bool validContext(const OpenVRVulkan* context) {
  return context && context->runtime && context->runtime->initialized && context->runtime->compositor &&
    context->runtime->compositor->GetVulkanInstanceExtensionsRequired &&
    context->runtime->compositor->GetVulkanDeviceExtensionsRequired &&
    ((context->allocate && context->deallocate) || (!context->allocate && !context->deallocate));
}

static void* allocate(const OpenVRVulkan* context, size_t size) {
  return context->allocate ? context->allocate(size) : malloc(size);
}

static void deallocate(const OpenVRVulkan* context, void* pointer) {
  if (!pointer) return;
  if (context->deallocate) context->deallocate(pointer);
  else free(pointer);
}

static uint32_t query(const OpenVRVulkan* context, VkPhysicalDevice device, char* buffer, uint32_t size) {
  return device ? context->runtime->compositor->GetVulkanDeviceExtensionsRequired(device, buffer, size) :
    context->runtime->compositor->GetVulkanInstanceExtensionsRequired(buffer, size);
}

static bool validName(const char* name) {
  if (!name || !*name) return false;
  size_t length = 0;
  while (*name) {
    char c = *name++;
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    if (++length >= VK_MAX_EXTENSION_NAME_SIZE) return false;
  }
  return true;
}

static void append(const char** names, uint32_t* count, const char* name) {
  for (uint32_t i = 0; i < *count; i++) {
    if (!strcmp(names[i], name)) return;
  }
  names[(*count)++] = name;
}

static VkResult extensions(const OpenVRVulkan* context, VkPhysicalDevice device, uint32_t appCount,
  const char* const* appNames, char** buffer, const char*** names, uint32_t* count) {
  if (appCount > MAX_EXTENSION_ENTRIES || (appCount && !appNames)) return VK_ERROR_INITIALIZATION_FAILED;
  for (uint32_t i = 0; i < appCount; i++) {
    if (!validName(appNames[i])) return VK_ERROR_INITIALIZATION_FAILED;
  }
  uint32_t size = query(context, device, NULL, 0);
  for (unsigned attempt = 0; attempt < 4; attempt++) {
    if (!size || size > MAX_EXTENSION_BYTES) return VK_ERROR_INITIALIZATION_FAILED;
    *buffer = allocate(context, size);
    if (!*buffer) return VK_ERROR_OUT_OF_HOST_MEMORY;
    memset(*buffer, 0xff, size);
    uint32_t actual = query(context, device, *buffer, size);
    if (!actual) return VK_ERROR_INITIALIZATION_FAILED;
    if (actual > size) {
      deallocate(context, *buffer);
      *buffer = NULL;
      size = actual;
      continue;
    }
    if ((*buffer)[actual - 1] != '\0' || memchr(*buffer, '\0', actual - 1)) return VK_ERROR_INITIALIZATION_FAILED;
    uint32_t runtimeCount = 0;
    bool inName = false;
    for (uint32_t i = 0; i < actual - 1; i++) {
      if ((*buffer)[i] == ' ') {
        (*buffer)[i] = '\0';
        inName = false;
      } else if (!inName) {
        runtimeCount++;
        inName = true;
      }
    }
    if (runtimeCount > MAX_EXTENSION_ENTRIES - appCount) return VK_ERROR_INITIALIZATION_FAILED;
    if ((uint64_t) appCount + runtimeCount > SIZE_MAX / sizeof(**names)) {
      return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    size_t capacity = (size_t) appCount + runtimeCount;
    *names = capacity ? allocate(context, capacity * sizeof(**names)) : NULL;
    if (capacity && !*names) return VK_ERROR_OUT_OF_HOST_MEMORY;
    *count = 0;
    for (uint32_t i = 0; i < appCount; i++) append(*names, count, appNames[i]);
    for (uint32_t i = 0; i < actual - 1;) {
      if (!(*buffer)[i]) { i++; continue; }
      char* name = *buffer + i;
      if (!validName(name)) return VK_ERROR_INITIALIZATION_FAILED;
      append(*names, count, name);
      i += (uint32_t) strlen(name) + 1;
    }
    return VK_SUCCESS;
  }
  return VK_ERROR_INITIALIZATION_FAILED;
}
