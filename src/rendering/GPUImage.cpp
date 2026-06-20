#include "rendering/GPUImage.h"
#include "rendering/ImageHelper.h"
#include "application/ApplicationInfo.h"
#include "exporter/CPUImage.h"
#include "rendering/CommandBuffer.h"
#include "rendering/GraphicsBuffer.h"
#include "handler/VkImageViewHandler.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>


#ifdef M3VK_MEMORYLOG
#include "application/DebugLayer.h"
#endif

/* --- GPU Allocated Image --- */

GPUAllocatedImage::GPUAllocatedImage(const CPUImage& cpuImg, VkCommandPool pool, VkQueue queue)
    : GPUAllocatedImage(cpuImg.Width(),
        cpuImg.Height(),
        cpuImg.GetGPUFormat(),
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
{
    UploadAndGenerateMip(cpuImg.Data(), cpuImg.Width(), cpuImg.Height(), cpuImg.Channels(), pool, queue);
}

GPUAllocatedImage::GPUAllocatedImage(const tinyddsloader::DDSFile& file, VkCommandPool pool, VkQueue queue)
    : GPUAllocatedImage(file.GetWidth(),
        file.GetHeight(),
        file.GetMipCount(),
        ImageHelper::DXGIToVkFormat(file.GetFormat()),
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
{
    VkDeviceSize totalSize = 0;
    for (uint32_t i = 0; i < _internal.MipCount; i++) {
        totalSize += file.GetImageData(i)->m_memSlicePitch;
    }

    StageBuffer stage(totalSize, StageBuffer::Usage::Upload);
    void* dstPtr = stage.Map(0, totalSize);

    if(_internal.MipCount >= 16)
    {
        throw std::runtime_error("Too many mip levels !");
    }

    static std::array<VkBufferImageCopy, 16> copyRegions;

    VkDeviceSize offset = 0;
    for (uint32_t i = 0; i < _internal.MipCount; i++) {
        auto imageData = file.GetImageData(i);

        void* srcPtr = imageData->m_mem;
        memcpy(dstPtr, srcPtr, imageData->m_memSlicePitch);

        copyRegions[i] =
        {
            .bufferOffset = offset,
            .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = i,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .imageExtent
            {
                .width = static_cast<uint32_t>(imageData->m_width),
                .height = static_cast<uint32_t>(imageData->m_height),
                .depth = 1
            }
        };

        offset += imageData->m_memSlicePitch;
        dstPtr = (uint8_t*)dstPtr + imageData->m_memSlicePitch;
    }
    stage.Unmap();

    CommandBuffer cmdBuffer(pool, queue);
    cmdBuffer.BeginSingleTime();
    {
        ImageHelper::TransitionLayoutCommand(cmdBuffer, _internal, 0, _internal.MipCount, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        cmdBuffer.CopyBufferToImage(stage.Internal(), _internal.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copyRegions.data(), _internal.MipCount);
        ImageHelper::TransitionLayoutCommand(cmdBuffer, _internal, 0, _internal.MipCount, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    cmdBuffer.End();
    cmdBuffer.WaitCompletion();
}

GPUAllocatedImage::GPUAllocatedImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags imageUsageFlags, VkMemoryPropertyFlags memoryFlags)
: GPUAllocatedImage(width, height, ImageHelper::GetMipCount(width, height), format, tiling, imageUsageFlags, memoryFlags)
{

}

GPUAllocatedImage::GPUAllocatedImage(uint32_t width, uint32_t height, uint32_t mipCount, VkFormat format, VkImageTiling tiling, VkImageUsageFlags imageUsageFlags, VkMemoryPropertyFlags memoryFlags)
: GPUAllocatedImage(width, height, VK_SAMPLE_COUNT_1_BIT, mipCount, format, tiling, imageUsageFlags, memoryFlags)
{

}

GPUAllocatedImage::GPUAllocatedImage(uint32_t width, uint32_t height, VkSampleCountFlagBits msaaSampleCount, uint32_t mipCount, VkFormat format, VkImageTiling tiling, VkImageUsageFlags imageUsageFlags, VkMemoryPropertyFlags memoryFlags)
{
#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::CREATE, "GPUImage Create !");
#endif
    VkImageCreateInfo createInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent
        {
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .depth = 1
        },
        .mipLevels = mipCount,
        .arrayLayers = 1,
        .samples = msaaSampleCount,
        .tiling = tiling, // Optimal tiling data, if need to write / acces directly to the texture need LINEAR wich is classical row column
        .usage = imageUsageFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, // only used by the graphics queue
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImage image = VK_NULL_HANDLE;

    if(vkCreateImage(ApplicationInfo::Device(), &createInfo, nullptr, &image) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create GPU image !");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(ApplicationInfo::Device(), image, &memRequirements);

    VkMemoryAllocateInfo allocInfo
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = ApplicationInfo::FindMemoryType(memRequirements.memoryTypeBits, memoryFlags)
    };

    if(vkAllocateMemory(ApplicationInfo::Device(), &allocInfo, nullptr, &_memoryInternal) != VK_SUCCESS)
    {
        vkDestroyImage(ApplicationInfo::Device(), image, nullptr);
        throw  std::runtime_error("Can't allocate image memory !");
    }

    if(vkBindImageMemory(ApplicationInfo::Device(), image, _memoryInternal, 0) != VK_SUCCESS)
    {
        vkDestroyImage(ApplicationInfo::Device(), image, nullptr);
        vkFreeMemory(ApplicationInfo::Device(), _memoryInternal, nullptr);
        throw  std::runtime_error("Can't bind image memory !");
    }

    _view = VkImageViewHandler(image, format, mipCount);

    _internal =
    {
        .Image = image,
        .View = _view.Internal(),
        .Format = format,
        .Width = static_cast<uint32_t>(width),
        .Height = static_cast<uint32_t>(height),
        .MipCount = mipCount,
        .Size = memRequirements.size
    };
}


void GPUAllocatedImage::TransitionLayout(VkCommandPool pool, VkQueue queue, VkImageLayout oldLayout, VkImageLayout newLayout) const
{
    CommandBuffer cmdBuffer(pool, queue);
    cmdBuffer.BeginSingleTime();
    {
        ImageHelper::TransitionLayoutCommand(cmdBuffer, _internal, oldLayout, newLayout);
    }
    cmdBuffer.End();
    cmdBuffer.WaitCompletion();
}

void GPUAllocatedImage::UploadAndGenerateMip(void* data, uint32_t width, uint32_t height, uint32_t pixelStride, VkCommandPool pool, VkQueue queue)
{
    VkDeviceSize size = width * height * pixelStride;
    StageBuffer stage(size, StageBuffer::Usage::Upload);
    stage.MapAndCopyToBuffer(data, size);

    CommandBuffer cmdBuffer(pool, queue);
    cmdBuffer.BeginSingleTime();
    {
        ImageHelper::CopyToImageCommand(cmdBuffer, _internal, 0, stage.Internal());
        ImageHelper::GenerateMipmapsCommand(cmdBuffer, _internal);
    }
    cmdBuffer.End();
    cmdBuffer.WaitCompletion();
}

GPUAllocatedImage::~GPUAllocatedImage()
{
    vkDestroyImage(ApplicationInfo::Device(), _internal.Image, nullptr);
    vkFreeMemory(ApplicationInfo::Device(), _memoryInternal, nullptr);

#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::DESTROY, "GPUImage Destroyed !");
#endif
}

GPUAllocatedImage::GPUAllocatedImage(GPUAllocatedImage&& other) noexcept
{
    _internal = std::exchange(other._internal, {});
    _memoryInternal = std::exchange(other._memoryInternal, VK_NULL_HANDLE);
    _view = std::move(other._view);
}

GPUAllocatedImage& GPUAllocatedImage::operator=(GPUAllocatedImage&& other) noexcept
{
    if(this != &other)
    {
        _internal = std::exchange(other._internal, {});
        _memoryInternal = std::exchange(other._memoryInternal, VK_NULL_HANDLE);
        _view = std::move(other._view);
    }
    return *this;
}
