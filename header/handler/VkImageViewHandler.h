#pragma once

#include "handler/Handlers.h"
#include <cstdint>
#include <vulkan/vulkan_core.h>

class VkImageViewHandler : public Handler<VkImageView>
{
    public:
    VkImageViewHandler() {};
    VkImageViewHandler(VkImage image, VkFormat format, uint32_t mipCount, VkImageAspectFlags aspectMask);
    VkImageViewHandler(VkImage image, VkFormat format, uint32_t mipCount);
    ~VkImageViewHandler();

    VkImageViewHandler(VkImageViewHandler&& other) noexcept;
    VkImageViewHandler& operator=(VkImageViewHandler&& other) noexcept;

    inline VkFormat Format() const
    {
        return _format;
    }

    private:
    VkFormat _format = VK_FORMAT_UNDEFINED;
};
