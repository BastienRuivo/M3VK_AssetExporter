#pragma once

#include "handler/Handlers.h"
#include "handler/VkPhysicalDeviceHandler.h"
#include <cstdint>
#include <filesystem>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <sys/types.h>
#include <vector>

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>

#include "application/DebugLayer.h"

class Application
{

    public:
    void ExportFile(u_char mode, std::filesystem::path sourcePath, std::filesystem::path targetPath);

    Application();
    ~Application();

    private:

    VkInstanceHandler _instance;
    DebugLayer _vkDebugLayer;
    VkPhysicalDeviceHandler _physicalDevice;
    VkDeviceHandler _device;
    VkQueueHandler _uploadQueue;
    VkCommandPoolHandler _uploadCommandPool;

    // Utils
    static inline const std::vector<const char*> _deviceExtensions = {
    };

    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
};
