#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>
#include "rendering/QueueFamilyIds.h"
#include "handler/Handlers.h"
#include "handler/VkPhysicalDeviceHandler.h"

class ApplicationInfo
{
    public:

    ApplicationInfo(const ApplicationInfo&) = delete;
    void operator=(const ApplicationInfo&) = delete;

    static ApplicationInfo& Get()
    {
        static ApplicationInfo instance;
        return instance;
    }

    static inline VkDevice Device() { ApplicationInfo::Get(); return ApplicationInfo::Get()._device; }
    static inline VkPhysicalDevice PhysicalDevice() { return ApplicationInfo::Get()._physicalDevice; }

    struct Constant
    {
        static inline constexpr VkSampleCountFlagBits MaxMSAASample = VK_SAMPLE_COUNT_8_BIT;
        static inline constexpr uint32_t MaxFrameInCount = 2;
        static inline constexpr size_t VertexBufferMaxSize = 16777216; // 2^23
        static inline constexpr size_t IndexBufferMaxSize = 16777216;
        static inline constexpr size_t MaterialBufferMaxSize = 2048;
        static inline constexpr VkFormat DepthFormat = VK_FORMAT_D32_SFLOAT;
        static inline constexpr int InputPrevent = 60;
    };

    static inline const QueueFamilyIds& GetQueueFamilyIds()
    {
        return ApplicationInfo::Get()._queueFamilyIds;
    }

    static inline uint32_t GetGraphicsQueueId()
    {
        return ApplicationInfo::Get()._queueFamilyIds.Graphics.value();
    }

    static inline const VkPhysicalDeviceProperties& GetProperties()
    {
        return ApplicationInfo::Get()._properties;
    }

    static inline VkSampleCountFlagBits GetMsaaSample()
    {
        return ApplicationInfo::Get()._msaaSample;
    }

    static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    private:
    void SetPhysicalDeviceInformation(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties properties, const QueueFamilyIds& queueFamilyIds);

    ApplicationInfo() {}
    VkSampleCountFlagBits GetMaxUsableSampleCount(VkSampleCountFlagBits maxSample) const;
    QueueFamilyIds _queueFamilyIds;
    VkPhysicalDeviceProperties _properties;
    VkSampleCountFlagBits  _msaaSample = VK_SAMPLE_COUNT_1_BIT;
    VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
    VkDevice _device = VK_NULL_HANDLE;

    friend class VkPhysicalDeviceHandler;
    friend class VkDeviceHandler;
};
