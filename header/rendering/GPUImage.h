#pragma once

#include <cstddef>
#include <cstdint>
#include <stb_image.h>
#include "exporter/CPUImage.h"
#include "libs/tinyddsloader.h"
#include "rendering/CommandBuffer.h"
#include "handler/VkImageViewHandler.h"
#include <vulkan/vulkan_core.h>

struct ImageReference
{
    VkImage Image = VK_NULL_HANDLE;
    VkImageView View = VK_NULL_HANDLE;
    VkFormat Format = VK_FORMAT_UNDEFINED;
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t MipCount = 0;
    size_t Size = 0;
};

class GPUAllocatedImage
{
    public:

    GPUAllocatedImage(const CPUImage& cpuImg, VkCommandPool pool, VkQueue queue);
    GPUAllocatedImage(const tinyddsloader::DDSFile& file, VkCommandPool pool, VkQueue queue);
    GPUAllocatedImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags imageUsageFlags, VkMemoryPropertyFlags memoryFlags);
    GPUAllocatedImage(uint32_t width, uint32_t height, uint32_t mipCount, VkFormat format, VkImageTiling tiling, VkImageUsageFlags imageUsageFlags, VkMemoryPropertyFlags memoryFlags);
    GPUAllocatedImage(uint32_t width, uint32_t height, VkSampleCountFlagBits msaaSampleCount, uint32_t mipCount, VkFormat format, VkImageTiling tiling, VkImageUsageFlags imageUsageFlags, VkMemoryPropertyFlags memoryFlags);
    ~GPUAllocatedImage();

    GPUAllocatedImage(GPUAllocatedImage&& other) noexcept;
    GPUAllocatedImage& operator=(GPUAllocatedImage&& other) noexcept;

    GPUAllocatedImage(const GPUAllocatedImage&) = delete;
    GPUAllocatedImage& operator=(const GPUAllocatedImage&) = delete;

    void UploadAndGenerateMip(void* data, uint32_t width, uint32_t height, uint32_t pixelStride, VkCommandPool pool, VkQueue queue);
    void TransitionLayout(VkCommandPool pool, VkQueue queue, VkImageLayout oldLayout, VkImageLayout newLayout) const;
    void GenerateMipmapsCommand(const CommandBuffer& cmdBuffer) const;

    inline ImageReference Internal() const { return _internal; }
    inline VkImageView View() const { return _internal.View; }
    inline uint32_t Width() const { return _internal.Width; }
    inline uint32_t Height() const { return _internal.Height; }
    inline uint32_t MipCount() const { return _internal.MipCount; }

    private:
    VkDeviceMemory _memoryInternal = VK_NULL_HANDLE;
    ImageReference _internal;
    VkImageViewHandler _view;
};
