#pragma once

#include "libs/tinyddsloader.h"
#include "rendering/GPUImage.h"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace ImageHelper
{
    void TransitionLayoutCommand(const CommandBuffer& cmdBuffer, const ImageReference& image, VkImageLayout oldLayout, VkImageLayout newLayout);
    void TransitionLayoutCommand(const CommandBuffer& cmdBuffer, const ImageReference& image, uint32_t mipLevel, uint32_t mipCount, VkImageLayout oldLayout, VkImageLayout newLayout);

    void CopyToImageCommand(const CommandBuffer& cmdBuffer, const ImageReference& image, uint32_t mipLevel, VkBuffer srcData);

    void GenerateMipmapsCommand(const CommandBuffer& cmdBuffer, const ImageReference& image);

    VkFormat DXGIToVkFormat(tinyddsloader::DDSFile::DXGIFormat dxgiFormat);
    uint32_t GetMipCount(uint32_t width, uint32_t height);
    uint32_t GetBytePerPixel(VkFormat format);
}
