#include "handler/Handlers.h"
#include "application/ApplicationInfo.h"
#include <cstdint>
#include <cstring>
#include <set>
#include <stdexcept>
#include <vulkan/vulkan_core.h>
#include "application/DebugLayer.h"

VkCommandPoolHandler::VkCommandPoolHandler()
: Handler<VkCommandPool>()
{
#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::CREATE, "VkCommandPoolHandler Creation !");
#endif

    VkCommandPoolCreateInfo poolInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ApplicationInfo::Get().GetGraphicsQueueId()
    };

    if(vkCreateCommandPool(ApplicationInfo::Device(), &poolInfo, nullptr, &_internal) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create command pool !");
    }
}

VkCommandPoolHandler::~VkCommandPoolHandler()
{
    if(_internal == VK_NULL_HANDLE) return;

    vkDestroyCommandPool(ApplicationInfo::Device(), _internal, nullptr);

#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::DESTROY, "VkCommandPoolHandler Destroyed !");
#endif
}

VkDeviceHandler::VkDeviceHandler(const std::vector<const char*>& deviceExtensions)
{
#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::CREATE, "VkDeviceHandler Creation !");
#endif

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueIds =
    {
        ApplicationInfo::Get().GetGraphicsQueueId()
    };

    float queuePriority = 1.0f;
    for(uint32_t queueId : uniqueQueueIds)
    {
        VkDeviceQueueCreateInfo queueCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueId,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = VK_TRUE
    };

    VkPhysicalDeviceFeatures deviceFeatures
    {
        .sampleRateShading = VK_TRUE,
        .samplerAnisotropy = VK_TRUE,
    };

    VkDeviceCreateInfo deviceCreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &dynamicRenderingFeatures,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),

        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,

        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &deviceFeatures
    };

    VkResult deviceCreation = vkCreateDevice(ApplicationInfo::PhysicalDevice(), &deviceCreateInfo, nullptr, &_internal);
    if(deviceCreation != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create VK Logical Device !");
    }

    ApplicationInfo::Get()._device = _internal;
}

VkDeviceHandler::~VkDeviceHandler()
{
    if(_internal == VK_NULL_HANDLE) return;

    vkDestroyDevice(_internal, nullptr);

#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::DESTROY, "VkDeviceHandler Destroyed !");
#endif
}

VkInstanceHandler::VkInstanceHandler()
{
#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::CREATE, "VkInstanceHandler creation !");
#endif

    VkApplicationInfo appInfo
    {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "M3VK",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4
    };

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, supportedExtensions.data());

    if(DebugLayer::Enabled)
    {
#ifdef M3VK_VERBOSE_LOG
        DebugLayer::Log(DebugLayer::LogType::INFO, "List of actives VK Extensions");
#endif
        for(const VkExtensionProperties& extension : supportedExtensions)
        {
#ifdef M3VK_VERBOSE_LOG
            DebugLayer::Log(DebugLayer::LogType::INFO, std::string("\t - ") + extension.extensionName);
#endif
        }
    }

    std::vector<const char *> requiredExtensions = GetRequiredExtensions();
    for(int i = 0; i < requiredExtensions.size(); ++i)
    {
        bool isPresent = false;
        for(const VkExtensionProperties& extension : supportedExtensions)
        {
            if(strcmp(requiredExtensions[i], extension.extensionName) == 0)
            {
                isPresent = true;
                break;
            }
        }

        if(!isPresent)
        {
            throw std::runtime_error("Vulkan Extension" + std::string(requiredExtensions[i]) + " Is not supported but required");
        }
    }

    if(!DebugLayer::CheckValidationLayerSupport())
    {
        throw std::runtime_error("Validation layer requested but not available !");
    }

    VkInstanceCreateInfo createInfo
    {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data(),
    };

    // ensure it is not destroyed before vkCreateInstance
    VkDebugUtilsMessengerCreateInfoEXT debugInfoCreate;
    DebugLayer::SetupCreateInfo(createInfo, debugInfoCreate);

    VkResult result = vkCreateInstance(&createInfo, nullptr, &_internal);
    if(result != VK_SUCCESS)
    {
        if(result == VK_ERROR_LAYER_NOT_PRESENT)
        {
            throw std::runtime_error("Error : A VK Layer is not present on computer");
        }
        else
        {
            throw std::runtime_error("Failed to create VK_Instance");
        }
    }
}

std::vector<const char *> VkInstanceHandler::GetRequiredExtensions() const
{
    std::vector<const char*> extensions(0);

    if(DebugLayer::Enabled)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}
VkInstanceHandler::~VkInstanceHandler()
{
    if(_internal == VK_NULL_HANDLE) return;

    vkDestroyInstance(_internal, nullptr);

#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::DESTROY, "VkInstanceHandler Destroyed !");
#endif
}

VkQueueHandler::VkQueueHandler(VkQueueHandler::QueueTypeEnum queueType)
{
#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::CREATE, "VkQueueHandler Creation !");
#endif

    uint32_t family;

    switch (queueType)
    {
        case Graphics: family = ApplicationInfo::Get().GetGraphicsQueueId(); break;
        default: throw std::runtime_error("Unimplemented graphics queue type");
    }

    vkGetDeviceQueue(ApplicationInfo::Device(),family, 0, &_internal);
    _queueFamilyIndex = family;
    _type = queueType;
}

VkQueueHandler::~VkQueueHandler()
{
#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::DESTROY, "VkQueueHandler Destroyed !");
#endif
}

VkQueueHandler::VkQueueHandler(VkQueueHandler && other) noexcept
{
    _internal = std::exchange(other._internal, VK_NULL_HANDLE);
    _queueFamilyIndex = std::exchange(other._queueFamilyIndex, 0);
    _type = std::exchange(other._type, QueueTypeEnum::Graphics);
}

VkQueueHandler& VkQueueHandler::operator=(VkQueueHandler&& other) noexcept
{
    if(this != &other)
    {
        _internal = std::exchange(other._internal, VK_NULL_HANDLE);
        _queueFamilyIndex = std::exchange(other._queueFamilyIndex, 0);
        _type = std::exchange(other._type, QueueTypeEnum::Graphics);
    }
    return *this;
}
