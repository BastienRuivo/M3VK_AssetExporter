#pragma once

#include "rendering/QueueFamilyIds.h"
#include "handler/Handlers.h"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

class VkPhysicalDeviceHandler : public Handler<VkPhysicalDevice>
{
    public:

    VkPhysicalDeviceHandler(VkInstance instance, const std::vector<const char *>& deviceExtensions);
    ~VkPhysicalDeviceHandler() override;

    VkPhysicalDeviceHandler(VkPhysicalDeviceHandler&& other) noexcept = default;
    VkPhysicalDeviceHandler& operator=(VkPhysicalDeviceHandler&& other) noexcept = default;

    int ScoreDeviceSuitability(VkPhysicalDevice physicalDevice, const std::vector<const char *>& deviceExtensions, VkPhysicalDeviceProperties& deviceProperties, QueueFamilyIds& familyIds) const;
    bool CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice, const std::vector<const char *>& deviceExtensions) const;
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    friend class ApplicationInfo;
};
