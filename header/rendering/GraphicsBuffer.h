#pragma once

#include "application/DebugLayer.h"
#include <cstdint>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

class StageBuffer
{
    friend class GraphicsBuffer;
    public:

    enum Usage
    {
        Upload,
        Readback
    };

    StageBuffer(VkDeviceSize size, Usage usage);
    ~StageBuffer();

    StageBuffer(StageBuffer&& other) noexcept;
    StageBuffer& operator=(StageBuffer&& other) noexcept;

    StageBuffer(const StageBuffer&) = delete;
    StageBuffer& operator=(const StageBuffer&) = delete;

    void* Map(VkDeviceSize offset, VkDeviceSize size);
    void Unmap();

    void MapAndCopyToBuffer(void* srcData, VkDeviceSize copySize);
    void MapAndCopyToData(void* dstData, VkDeviceSize copySize);
    inline VkBuffer Internal() const { return _internal; };

    private:
    VkBuffer _internal = VK_NULL_HANDLE;
    VkDeviceMemory _memoryInternal = VK_NULL_HANDLE;
    Usage _usage;
};


class GraphicsBuffer
{
    public:
    enum BufferType
    {
        INDEX = 0,
        VERTEX = 1,
        DYNAMIC_UNIFORM = 2,
        UNIFORM = 3,
        STORAGE = 4
    };
    GraphicsBuffer(VkDeviceSize count, VkDeviceSize stride, BufferType type);
    ~GraphicsBuffer();

    GraphicsBuffer(GraphicsBuffer&& other) noexcept;
    GraphicsBuffer& operator=(GraphicsBuffer&& other) noexcept;

    GraphicsBuffer(const GraphicsBuffer&) = delete;
    GraphicsBuffer& operator=(const GraphicsBuffer&) = delete;

    void CopyToBuffer(const VkQueue& queue,
        const VkCommandPool& pool,
        void* srcData,
        VkDeviceSize size,
        uint32_t srcIndex = 0,
        uint32_t dstIndex = 0);

    VkBuffer Internal() const { return _internal; }
    BufferType GetType() const { return _type; }

    inline VkDeviceSize GetSize() const { return _count * _stride; }
    inline VkDeviceSize GetCount() const { return _count; }
    inline VkDeviceSize GetStride() const { return _stride; }
    inline void* GetDataPtr() const
    {
        if(_type != BufferType::DYNAMIC_UNIFORM)
        {
            DebugLayer::Log(DebugLayer::LogType::ERROR, "Trying to get data pointer for non uniform buffer");
        }
        return _dataPtr;
    }

    inline VkDescriptorBufferInfo GetDescriptorBufferInfo(uint32_t index, uint32_t count) const
    {
        return
        {
            .buffer = _internal,
            .offset = index * _stride,
            .range = _stride * count
        };
    }

    inline VkDescriptorType GetDescriptorType() const
    {
        switch (_type) {
        case BufferType::DYNAMIC_UNIFORM:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case BufferType::UNIFORM:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case BufferType::STORAGE:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        default: throw std::runtime_error("Unimplemented Descriptor Type");
        }
    }

    protected:
    VkBuffer _internal = VK_NULL_HANDLE;
    VkDeviceMemory _memoryInternal = VK_NULL_HANDLE;
    BufferType _type;
    void* _dataPtr = nullptr; // Currently used for persistent mapping for Uniform buffers, we only map it once to avoid the cost of mapping it each time
    VkDeviceSize _stride = 0;
    VkDeviceSize _count = 0;
};

// this class handle a graphics buffer with an arbitrary huge size where we can append data
class GeometryBuffer : public GraphicsBuffer
{
    public:
    using GraphicsBuffer::GraphicsBuffer;

    void CopyToBuffer(const VkQueue& queue,
        const VkCommandPool& cmdPool,
        void* srcData,
        VkDeviceSize size
    );

    inline VkDeviceSize GetCurrentSize() const { return _currentSize; }
    inline uint32_t GetCurrentIndex() const { return _currentSize / _stride; }

    private:
    VkDeviceSize _currentSize = 0;
};
