#include "rendering/ImageHelper.h"
#include "application/ApplicationInfo.h"
#include "libs/tinyddsloader.h"
#include "rendering/GPUImage.h"
#include <cmath>
#include <cstdint>
#include <vulkan/vulkan_core.h>

void ImageHelper::TransitionLayoutCommand(const CommandBuffer& cmdBuffer, const ImageReference& image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    cmdBuffer.TransitionImageLayout(image.Image, image.Format, 0, image.MipCount, oldLayout, newLayout);
}

void ImageHelper::TransitionLayoutCommand(const CommandBuffer& cmdBuffer, const ImageReference& image, uint32_t mipLevel, uint32_t mipCount, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    cmdBuffer.TransitionImageLayout(image.Image, image.Format, mipLevel, mipCount, oldLayout, newLayout);
}

void ImageHelper::CopyToImageCommand(const CommandBuffer &cmdBuffer, const ImageReference &image, uint32_t mipLevel, VkBuffer srcData)
{
    ImageHelper::TransitionLayoutCommand(cmdBuffer, image, mipLevel, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy region
    {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = mipLevel,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset
        {
            .x = 0,
            .y = 0,
            .z = 0
        },
        .imageExtent
        {
            .width = static_cast<uint32_t>(image.Width),
            .height = static_cast<uint32_t>(image.Height),
            .depth = 1
        },
    };
    cmdBuffer.CopyBufferToImage(srcData, image.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &region, 1);
}

void ImageHelper::GenerateMipmapsCommand(const CommandBuffer& cmdBuffer, const ImageReference& image)
{
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(ApplicationInfo::PhysicalDevice(), image.Format, &formatProperties);
    if(!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        // TODO Someday : software mipmapping and storing the mipmaps
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    VkImageMemoryBarrier barrier
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image.Image,
        .subresourceRange
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    int32_t mipWidth = image.Width;
    int32_t mipHeight = image.Height;

    size_t mipCount = image.MipCount;
    for(uint32_t i = 1; i < mipCount; ++i)
    {
        ImageHelper::TransitionLayoutCommand(cmdBuffer, image, i, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        cmdBuffer.Barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, nullptr, 0, nullptr, 0, &barrier, 1);

        VkImageBlit blit
        {
            .srcSubresource
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = i - 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .srcOffsets =
            {
                {0, 0, 0},
                {mipWidth, mipHeight, 1}
            },
            .dstSubresource
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = i,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .dstOffsets =
            {
                {0, 0, 0},
                {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1}
            },

        };

        cmdBuffer.Blit(image.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        cmdBuffer.Barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, nullptr, 0, nullptr, 0, &barrier, 1);

        if(mipWidth > 1) mipWidth /= 2;
        if(mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipCount - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    cmdBuffer.Barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, nullptr, 0, nullptr, 0, &barrier, 1);
}

VkFormat ImageHelper::DXGIToVkFormat(tinyddsloader::DDSFile::DXGIFormat format)
{
    switch (format) {
        case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm:      return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm_SRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;

        case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm:      return VK_FORMAT_BC2_UNORM_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm_SRGB: return VK_FORMAT_BC2_SRGB_BLOCK;

        case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm:      return VK_FORMAT_BC3_UNORM_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm_SRGB: return VK_FORMAT_BC3_SRGB_BLOCK;

        case tinyddsloader::DDSFile::DXGIFormat::BC4_UNorm:      return VK_FORMAT_BC4_UNORM_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC4_SNorm:      return VK_FORMAT_BC4_SNORM_BLOCK;

        case tinyddsloader::DDSFile::DXGIFormat::BC5_UNorm:      return VK_FORMAT_BC5_UNORM_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC5_SNorm:      return VK_FORMAT_BC5_SNORM_BLOCK;

        case tinyddsloader::DDSFile::DXGIFormat::BC6H_UF16:      return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC6H_SF16:      return VK_FORMAT_BC6H_SFLOAT_BLOCK;

        case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm:      return VK_FORMAT_BC7_UNORM_BLOCK;
        case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;

        default: return VK_FORMAT_UNDEFINED;
    }
}

bool ImageHelper::IsTransparencyFormat(VkFormat format) {
    switch (format) {
        // BC formats
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
        case VK_FORMAT_BC7_UNORM_BLOCK:

        // uncompressed
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_R8G8B8A8_UNORM:
        return true;

        default: return false;
    }
}

uint32_t ImageHelper::GetMipCount(uint32_t width, uint32_t height) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

uint32_t ImageHelper::GetBytePerPixel(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_R8G8B8A8_SRGB: return 4;
        case VK_FORMAT_R8G8B8A8_UNORM: return 4;
        default: return 0;
    }
}
