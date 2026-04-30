#include "application/ApplicationHelper.h"
#include "glm/ext/quaternion_float.hpp"


#include "application/DebugLayer.h"
#include "application/ApplicationInfo.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>


std::vector<char> ApplicationHelper::ReadFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if(!file.is_open())
    {
        DebugLayer::Log(DebugLayer::LogType::ERROR, "Can't open " + std::string(std::filesystem::current_path()) + "/" + filename);
        throw std::runtime_error("Can't open file " + filename);
    }

    size_t fileSize = file.tellg();
    std::vector<char> bytes(fileSize);

    file.seekg(0);
    file.read(bytes.data(), fileSize);
    file.close();

    return bytes;
}

ApplicationHelper::SwapChainSupportDetails ApplicationHelper::QuerySwapChainSupportDetail(VkPhysicalDevice physicalDevice, const VkSurfaceKHR& windowSurface)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, windowSurface, &details.Capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &formatCount, nullptr);
    details.Formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &formatCount, details.Formats.data());

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, nullptr);
    if(presentModeCount > 0)
    {
        details.PresentsModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, details.PresentsModes.data());
    }
    return details;
}

void ApplicationHelper::CopyBufferToBuffer(const VkQueue queue, const VkCommandPool& cmdPool, const VkBuffer& src, VkDeviceSize srcOffset, const VkBuffer& dst, VkDeviceSize dstOffset, VkDeviceSize size)
{
    VkDevice device = ApplicationInfo::Device();
    VkCommandBufferAllocateInfo allocInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmdPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(ApplicationInfo::Device(),&allocInfo, &cmdBuffer);

    VkCommandBufferBeginInfo beginInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    VkBufferCopy copyRegion
    {
        .srcOffset = srcOffset,
        .dstOffset = dstOffset,
        .size = size
    };
    vkCmdCopyBuffer(cmdBuffer, src, dst, 1, &copyRegion);

    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo submitInfo
    {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdBuffer
    };

    // wait for the queue idle, we can use a fence to submit multiple shit later
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device,cmdPool, 1, &cmdBuffer);
}

bool ApplicationHelper::IsFormatSupported(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(ApplicationInfo::PhysicalDevice(), format, &props);

    if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
        return true;
    } else if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
        return true;
    }

    return false;
}

VkImageAspectFlags ApplicationHelper::GetImageAspectFlags(VkFormat format)
{
    switch(format)
    {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_S8_UINT:
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

bool ApplicationHelper::HasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

uint32_t ApplicationHelper::GetFormatSize(VkFormat format)
{
    switch(format)
    {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return 4;
        case VK_FORMAT_S8_UINT:
            return 1;
        default: throw std::runtime_error("Unimplemented Format GetFormatSize");
    }
}

glm::quat ApplicationHelper::EulerToQuat(glm::vec3 euler)
{
    return glm::quat(glm::vec3(glm::radians(euler.x), glm::radians(euler.y), glm::radians(euler.z)));
}
