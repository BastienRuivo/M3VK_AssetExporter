#pragma once

#include <cstdint>
#include <optional>

#include <vulkan/vulkan_core.h>

#include <vector>

struct QueueFamilyIds
{
    public:
    std::optional<uint32_t> Graphics;
    // std::optional<uint32_t> Copy;

    static bool AreAllQueueAvailable(const QueueFamilyIds& queueIds)
    {
        return queueIds.Graphics.has_value();
    }

    static QueueFamilyIds QueryQueueFamilies(VkPhysicalDevice physicalDevice)
    {
        QueueFamilyIds queueIds;

        uint32_t queueFamiliesCount = 0;

        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(queueFamiliesCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, families.data());

        for(int i = 0; i < families.size(); ++i)
        {
            if(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                queueIds.Graphics = i;
            }
        }

        return queueIds;
    }
};
