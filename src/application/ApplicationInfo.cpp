#include "application/ApplicationInfo.h"
#include "rendering/QueueFamilyIds.h"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

VkSampleCountFlagBits ApplicationInfo::GetMaxUsableSampleCount(VkSampleCountFlagBits maxSample) const
{
    VkSampleCountFlags counts = _properties.limits.framebufferColorSampleCounts & _properties.limits.framebufferDepthSampleCounts;

    if((counts & VK_SAMPLE_COUNT_64_BIT) && (maxSample >= VK_SAMPLE_COUNT_64_BIT)) return VK_SAMPLE_COUNT_64_BIT;
    else if((counts & VK_SAMPLE_COUNT_32_BIT) && (maxSample >= VK_SAMPLE_COUNT_32_BIT)) return VK_SAMPLE_COUNT_32_BIT;
    else if((counts & VK_SAMPLE_COUNT_16_BIT) && (maxSample >= VK_SAMPLE_COUNT_16_BIT)) return VK_SAMPLE_COUNT_16_BIT;
    else if((counts & VK_SAMPLE_COUNT_8_BIT) && (maxSample >= VK_SAMPLE_COUNT_8_BIT)) return VK_SAMPLE_COUNT_8_BIT;
    else if((counts & VK_SAMPLE_COUNT_4_BIT) && (maxSample >= VK_SAMPLE_COUNT_4_BIT)) return VK_SAMPLE_COUNT_4_BIT;
    else if((counts & VK_SAMPLE_COUNT_2_BIT) && (maxSample >= VK_SAMPLE_COUNT_2_BIT)) return VK_SAMPLE_COUNT_2_BIT;
    else return VK_SAMPLE_COUNT_1_BIT;
}

void ApplicationInfo::SetPhysicalDeviceInformation(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties properties, const QueueFamilyIds& queueFamilyIds)
{
    _physicalDevice = physicalDevice;
    _properties = properties;
    _queueFamilyIds = queueFamilyIds;
    _msaaSample = GetMaxUsableSampleCount(Constant::MaxMSAASample);
}

uint32_t ApplicationInfo::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(Get()._physicalDevice, &memoryProperties);

    for (uint32_t memoryType = 0; memoryType < memoryProperties.memoryTypeCount; ++memoryType)
    {
        // is suitable for buffer & writable by CPU
        if((typeFilter & (1 << memoryType)) && ((memoryProperties.memoryTypes[memoryType].propertyFlags & properties) == properties))
        {
            return memoryType;
        }
    }

    throw std::runtime_error("Can't find suitable memory type for buffer");
}
