#pragma once
#include <string>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

#include <vector>

#define M3VK_VERBOSE_LOG 1
//#define M3VK_MEMORYLOG 1

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

// this is not always create so we do it this way
VkResult M3VK_CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
void M3VK_DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);


class DebugLayer
{
    public:
    #ifdef NDEBUG
    static const bool Enabled = false;
    #else
    static const bool Enabled = true;
    #endif
    inline static const std::vector<const char*> ValidationLayer {
        "VK_LAYER_KHRONOS_validation"
    };

    enum LogType
    {
        VERBOSE,
        INFO,
        WARNING,
        ERROR,
        CREATE,
        DESTROY
    };

    static inline const char* TextColorGrey = "\033[0m";
    static inline const char* TextColorYellow = "\033[33m";
    static inline const char* TextColorRed = "\033[31m";

    DebugLayer(VkInstance instance);
    ~DebugLayer();

    static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    static void SetupCreateInfo(VkInstanceCreateInfo& instanceCreateInfo, VkDebugUtilsMessengerCreateInfoEXT& debugInfoCreate);
    static void Log(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, const std::string& message);
    static void Log(LogType LogType, const std::string& message);
    static bool CheckValidationLayerSupport();

    private:
    VkDebugUtilsMessengerEXT _debugMessenger;
    VkInstance _instance;

};
