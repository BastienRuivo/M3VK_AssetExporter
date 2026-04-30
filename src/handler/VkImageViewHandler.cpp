#include "handler/VkImageViewHandler.h"
#include "application/ApplicationInfo.h"
#include "application/ApplicationHelper.h"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#ifdef M3VK_MEMORYLOG
#include "application/DebugLayer.h"
#endif

VkImageViewHandler::VkImageViewHandler(VkImage image, VkFormat format, uint32_t mipCount) :
    VkImageViewHandler(image, format, mipCount, ApplicationHelper::GetImageAspectFlags(format)) {}

VkImageViewHandler::VkImageViewHandler(VkImage image, VkFormat format, uint32_t mipCount, VkImageAspectFlags aspectMask)
{
#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::CREATE, "VkImageViewHandler Creation !");
#endif

    VkImageViewCreateInfo createInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange =
        {
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = mipCount,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    if(vkCreateImageView(ApplicationInfo::Device(), &createInfo, nullptr, &_internal) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create swap chain images !");
    }
}

VkImageViewHandler::~VkImageViewHandler()
{
    if(_internal == VK_NULL_HANDLE) return;

    vkDestroyImageView(ApplicationInfo::Device(), _internal, nullptr);

#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::DESTROY, "VkImageViewHandler Destroyed !");
#endif
}

VkImageViewHandler::VkImageViewHandler(VkImageViewHandler&& other) noexcept
{
    _internal = std::exchange(other._internal, VK_NULL_HANDLE);
    _format = std::exchange(other._format, VK_FORMAT_UNDEFINED);
}

VkImageViewHandler& VkImageViewHandler::operator=(VkImageViewHandler&& other) noexcept
{
    if(this != &other)
    {
        _internal = std::exchange(other._internal, VK_NULL_HANDLE);
        _format = std::exchange(other._format, VK_FORMAT_UNDEFINED);
    }
    return *this;
}
