#pragma once

#include "rendering/GraphicsBuffer.h"
#include <cstdint>
#include <vulkan/vulkan_core.h>
class CommandBuffer
{
    public:
    enum Usage
    {
        SingleTime,
        MultipleTime
    };

    CommandBuffer(VkCommandPool pool, VkQueue queue);
    ~CommandBuffer();

    CommandBuffer(CommandBuffer&& other) noexcept;
    CommandBuffer& operator=(CommandBuffer&& other) noexcept;

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;

    void Begin(VkCommandBufferUsageFlags flags = 0) const;

    void End() const;

    void Submit(VkSemaphore waitSemaphores[], int waitCount,
        VkPipelineStageFlags waitStages[],
        VkSemaphore signalSemaphores[], int signalCount,
        VkFence fence = VK_NULL_HANDLE) const;

    inline void BeginSingleTime() const
    {
        Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    }

    void WaitCompletion() const;

    // Todo: how to handle this properly (both need different params)
    void BindBuffer(const GraphicsBuffer& buffer) const;
    void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) const;
    void SetScissor(const VkRect2D& scissors) const;
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const;
    void TransitionImageLayout(VkImage img, VkFormat format, uint32_t mipLevel, uint32_t mipCount, VkImageLayout oldLayout, VkImageLayout newLayout) const;

    inline void BindDescriptorSets(const VkPipelineLayout& pipelineLayout, const VkDescriptorSet& set, uint32_t location) const
    {
        vkCmdBindDescriptorSets(_internal, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, location, 1, &set, 0, nullptr);
    }

    inline void DrawIndexed(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexOffset) const
    {
        vkCmdDrawIndexed(_internal, indexCount, 1, firstIndex, vertexOffset, 0);
    }

    inline void BindPipeline(VkPipeline pipeline, VkPipelineBindPoint bindPoint) const
    {
        vkCmdBindPipeline(_internal, bindPoint, pipeline);
    }

    void Reset(VkCommandBufferResetFlags flags = 0) const
    {
        vkResetCommandBuffer(_internal, flags);
    }

    inline void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) const
    {
        vkCmdPushConstants(_internal, layout, stageFlags, offset, size, pValues);
    }

    inline void Barrier(VkPipelineStageFlags srcAccesMask, VkPipelineStageFlags dstAccesMask, VkMemoryBarrier* memoryBarriers, uint32_t memoryBarrierCount, VkBufferMemoryBarrier* bufferBarriers, uint32_t bufferBarrierCount, VkImageMemoryBarrier* imgBarriers, uint32_t imgBarrierCount) const
    {
        vkCmdPipelineBarrier(_internal,
            srcAccesMask, dstAccesMask,
            0,
            memoryBarrierCount, memoryBarriers,
            bufferBarrierCount, bufferBarriers,
            imgBarrierCount, imgBarriers
        );
    }

    inline void Blit(VkImage src, VkImageLayout srcLayout, VkImage dst, VkImageLayout dstLayout, uint32_t regionCount, VkImageBlit* pRegions, VkFilter filter) const
    {
        vkCmdBlitImage(_internal, src, srcLayout, dst, dstLayout, regionCount, pRegions, filter);
    }

    inline void CopyBufferToImage(VkBuffer buffer, VkImage image, VkImageLayout layout, VkBufferImageCopy* pRegions, int regionCount) const
    {
        vkCmdCopyBufferToImage(_internal, buffer, image, layout, regionCount, pRegions);
    }

    inline void CopyImageToBuffer(VkImage image, VkImageLayout layout, VkBuffer buffer, VkBufferImageCopy* pRegions, int regionCount) const
    {
        vkCmdCopyImageToBuffer(_internal, image, layout, buffer, regionCount, pRegions);
    }

    inline void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkBufferCopy* pRegions, int regionCount)
    {
        vkCmdCopyBuffer(_internal, src, dst, regionCount, pRegions);
    }

    inline void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, uint32_t srcIndex = 0, uint32_t dstIndex = 0)
    {
        VkBufferCopy region
        {
            .srcOffset = srcIndex,
            .dstOffset = dstIndex,
            .size = size
        };

        vkCmdCopyBuffer(_internal, src, dst, 1, &region);
    }

    inline void BeginRendering(VkRect2D renderArea, const VkRenderingAttachmentInfo * colorAttachment, uint32_t colorAttachmentCount, const VkRenderingAttachmentInfo& depthAttachment, const VkRenderingAttachmentInfo& stencilAttachment) const
    {
        VkRenderingInfo renderingInfo
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = renderArea,
            .layerCount = 1,
            .colorAttachmentCount = colorAttachmentCount,
            .pColorAttachments = colorAttachment,
            .pDepthAttachment = &depthAttachment,
            .pStencilAttachment = &stencilAttachment
        };
        vkCmdBeginRendering(_internal, &renderingInfo);
    }

    inline void EndRendering() const
    {
        vkCmdEndRendering(_internal);
    }

    inline VkCommandBuffer GetInternal() const { return _internal; }

    private:

    void CreateSingleTime();
    void CreateMultiUsage();

    VkCommandPool _pool = VK_NULL_HANDLE;
    VkQueue _queue = VK_NULL_HANDLE;
    VkCommandBuffer _internal = VK_NULL_HANDLE;
};
