#include "handler/VkPhysicalDeviceHandler.h"
#include "application/ApplicationInfo.h"
#include "application/DebugLayer.h"
#include "rendering/QueueFamilyIds.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_core.h>
#include <vector>
#include <string.h>

bool VkPhysicalDeviceHandler::CheckDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char *>& deviceExtensions) const
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> properties(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, properties.data());

    for(const auto& extension : deviceExtensions)
    {
        bool foundExtension = false;
        for(const VkExtensionProperties& property : properties)
        {
            if(strcmp(extension, property.extensionName) == 0)
            {
                foundExtension = true;
                break;
            }
        }
        if(!foundExtension)
        {
            if(DebugLayer::Enabled)
            {
                DebugLayer::Log(DebugLayer::LogType::ERROR, (std::string("Extension not supported : ") + std::string(extension)).c_str());
            }
            return false;
        }
    }

    return true;
}

uint32_t VkPhysicalDeviceHandler::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(_internal, &memoryProperties);

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

int VkPhysicalDeviceHandler::ScoreDeviceSuitability(VkPhysicalDevice physicalDevice, const std::vector<const char *>& deviceExtensions, VkPhysicalDeviceProperties& deviceProperties, QueueFamilyIds& familyIds) const
{
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    // support of addtionnal feature (texture compression, 64bit double, multi viewport rendering)
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    int score = 0;


    familyIds = QueueFamilyIds::QueryQueueFamilies(physicalDevice);

    bool areAllRequiredExtensionsSupported = CheckDeviceExtensionSupport(physicalDevice, deviceExtensions);

    // Mandatory feature, if any return 0 and this will be the only way to have 0 score meaning there's no suitable GPU
    if(!QueueFamilyIds::AreAllQueueAvailable(familyIds)
        || !areAllRequiredExtensionsSupported
        || !deviceFeatures.samplerAnisotropy)
    {
        return 0;
    }

    // Else we try to find the best available GPU for our criteria
    switch (deviceProperties.deviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: score += 600; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score += 800; break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score += 1000; break;
        default: break;
    }

    return score;
}

VkPhysicalDeviceHandler::VkPhysicalDeviceHandler(VkInstance instance, const std::vector<const char *>& deviceExtensions)
{
#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::CREATE, "VkPhysicalDeviceHandler Creation !");
#endif

    _internal = VK_NULL_HANDLE;
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if(deviceCount == 0)
    {
        throw std::runtime_error("Failed to find a Vulkan compatible GPU on this device");
    }

    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

    QueueFamilyIds queueFamilyIds;
    VkPhysicalDeviceProperties properties;

    int bestScore = 0;
    for(const VkPhysicalDevice& physicalDevice : physicalDevices)
    {
        QueueFamilyIds localQueueIds;
        VkPhysicalDeviceProperties localProperties;
        int score = ScoreDeviceSuitability(physicalDevice, deviceExtensions, localProperties, localQueueIds);
        if(score > bestScore)
        {
            bestScore = score;
            _internal = physicalDevice;
            queueFamilyIds = localQueueIds;
            properties = localProperties;
        }
    }

    if(_internal == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Failed to find a suitable GPU on this device");
    }

    ApplicationInfo::Get().SetPhysicalDeviceInformation(_internal, properties, queueFamilyIds);
}

VkPhysicalDeviceHandler::~VkPhysicalDeviceHandler()
{
    _internal = VK_NULL_HANDLE;
#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::DESTROY, "VkPhysicalDeviceHandler Destroyed !");
#endif
}
