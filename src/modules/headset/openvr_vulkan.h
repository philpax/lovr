#ifndef LOVR_OPENVR_VULKAN_H
#define LOVR_OPENVR_VULKAN_H

#include <stddef.h>
#include <vulkan/vulkan.h>
#include "openvr_runtime.h"

typedef struct OpenVRVulkan {
  OpenVRRuntime* runtime;
  PFN_vkCreateInstance createInstance;
  PFN_vkCreateDevice createDevice;
  PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices;
  void* (*allocate)(size_t size);
  void (*deallocate)(void* pointer);
} OpenVRVulkan;

VkResult lovrOpenVRVulkanCreateInstance(const OpenVRVulkan* context, const VkInstanceCreateInfo* info,
  const VkAllocationCallbacks* allocator, VkInstance* instance);
VkResult lovrOpenVRVulkanGetPhysicalDevice(const OpenVRVulkan* context, VkInstance instance, VkPhysicalDevice* device);
VkResult lovrOpenVRVulkanCreateDevice(const OpenVRVulkan* context, VkInstance instance, VkPhysicalDevice device,
  const VkDeviceCreateInfo* info, const VkAllocationCallbacks* allocator, VkDevice* output);

#endif
