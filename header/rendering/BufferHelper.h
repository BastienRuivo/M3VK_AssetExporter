#pragma once

#include "rendering/GraphicsBuffer.h"

namespace BufferHelper
{
    struct BufferBinding
    {
        VkDescriptorType DescriptorType;
        VkBuffer Buffer;
        VkDescriptorBufferInfo Descriptor;

        BufferBinding(const GraphicsBuffer& buffer, uint32_t index, uint32_t count = 1)
        {
            Buffer = buffer.Internal();
            DescriptorType = buffer.GetDescriptorType();
            Descriptor = buffer.GetDescriptorBufferInfo(index, count);
        }
    };
}
