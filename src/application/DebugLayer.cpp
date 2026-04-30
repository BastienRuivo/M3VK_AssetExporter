#include "application/DebugLayer.h"
#include <ostream>
#include <vulkan/vulkan_core.h>
#include <iostream>
#include <cstring>

VkResult M3VK_CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void M3VK_DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

void DebugLayer::Log(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, const std::string& message)
{
    DebugLayer::LogType logType;
    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: logType = DebugLayer::VERBOSE; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: logType = DebugLayer::INFO; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: logType = DebugLayer::WARNING; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: logType = DebugLayer::ERROR; break;
        default: logType = DebugLayer::VERBOSE; break;
    }
    Log(logType, message);
}

void DebugLayer::Log(DebugLayer::LogType LogType, const std::string& message)
{
    if(!Enabled) return;

    const char * color = nullptr;
    const char * title = nullptr;
    std::ostream * stream = nullptr;

    // Stream selection
    switch (LogType)
    {
        case DebugLayer::LogType::WARNING:
        case DebugLayer::LogType::ERROR: stream = &std::cerr; break;
        default: stream = &std::clog; break;
    }

    // Color selection
    switch (LogType)
    {
        case DebugLayer::LogType::WARNING: color = TextColorYellow; break;
        case DebugLayer::LogType::ERROR: color = TextColorRed; break;
        default: color = TextColorGrey; break;
    }

    // Title selection

    switch (LogType)
    {
        case DebugLayer::LogType::WARNING: title = "Warning"; break;
        case DebugLayer::LogType::ERROR: title = "Error"; break;
        case DebugLayer::LogType::VERBOSE: title = "Verbose"; break;
        case DebugLayer::LogType::INFO: title = "Info"; break;
        case DebugLayer::LogType::CREATE: title = "Create"; break;
        case DebugLayer::LogType::DESTROY: title = "Destroy"; break;
        default: title = "Unknown"; break;
    }

    *stream << color << "[Validation Layer Message] - [M3VK] : [" << title << "] " << message << std::endl;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{

    #ifndef M3VK_VERBOSE_LOG
        if(messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) return true;
    #endif

    std::cerr << "[Validation Layer Message] - [Vulkan] : ";
    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            std::cerr << "\033[0m" << "[Verbose]";
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            std::cerr << "\033[0m" << "[Info]";
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            std::cerr << "\033[33m" << "[Warning]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            std::cerr << "\033[31m" << "[Error]";
            break;
        default:
            break;
    }

    switch (messageType)
    {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            std::cerr<<"[General]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            std::cerr<<"[Validation]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            std::cerr<<"[Performance]";
            break;
    }

    std::cerr << pCallbackData->pMessage << std::endl;

    return messageSeverity != VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
}

bool DebugLayer::CheckValidationLayerSupport()
{
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for(const char* layerName : ValidationLayer)
    {
        bool find = false;
        for(const VkLayerProperties& property : availableLayers)
        {
            if(strcmp(layerName, property.layerName) == 0)
            {
                find = true;
                break;
            }
        }

        if(!find)
        {
            std::cerr<<"Can't find validation layer "<<layerName<<std::endl;
            return false;
        }
    }

    return true;
}

void DebugLayer::SetupCreateInfo(VkInstanceCreateInfo& instanceCreateInfo, VkDebugUtilsMessengerCreateInfoEXT& debugCreateInfo)
{
    if(Enabled)
    {
        instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayer.size());
        instanceCreateInfo.ppEnabledLayerNames = ValidationLayer.data();
        PopulateDebugMessengerCreateInfo(debugCreateInfo);
        instanceCreateInfo.pNext = &debugCreateInfo;
    }
    else
    {
        instanceCreateInfo.enabledLayerCount = 0;
        instanceCreateInfo.pNext = nullptr;
    }
}

void DebugLayer::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& debugMessengerCreateInfo)
{
    debugMessengerCreateInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = DebugCallback
    };
}

DebugLayer::DebugLayer(VkInstance instance)
{
#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::CREATE, "DebugLayer Creation !");
#endif

    if(!Enabled) return;
    _instance = instance;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    PopulateDebugMessengerCreateInfo(createInfo);

    if (M3VK_CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &_debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

DebugLayer::~DebugLayer()
{
#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::DESTROY, "DebugLayer Destroyed !");
#endif

    if (!Enabled) return;
    M3VK_DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
}
