#include "exporter/ExporterHelper.h"
#include "application/DebugLayer.h"
#include "assimp/material.h"
#include <cstdint>
#include <span>
#include <filesystem>
#include <string>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>

#include "assimp/types.h"
#include "exporter/Exporter.h"
#include "libs/tinyddsloader.h"
#include "rendering/ImageHelper.h"



#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <ispc_texcomp.h>

aiTextureType ExporterHelper::SelectTextureType(std::span<const aiTextureType> types, const aiMaterial* material, uint32_t& textureCount)
{
    for(const auto& type : types)
    {
        int count = material->GetTextureCount(type);
        if(count > 0)
        {
            textureCount = count;
            return type;
        }
    }
    return aiTextureType::aiTextureType_NONE;
}

std::filesystem::path ExporterHelper::GetTexturePath(const std::filesystem::path& modelPath)
{
    int pathIndex = 0;
    const int modelRoot = 1;
    std::filesystem::path texturePath;

    for(const auto& path : modelPath)
    {
        texturePath /= path;
        pathIndex++;
        if(pathIndex == modelRoot + 1)
        {
            break;
        }
    }

    return texturePath / "textures";
}

void ExporterHelper::DebugListMaterialTextures(aiMaterial* material) {
    // Iterate through all possible Assimp texture types
    for (unsigned int type = aiTextureType_NONE; type < AI_TEXTURE_TYPE_MAX; ++type)
    {
        aiTextureType textureType = static_cast<aiTextureType>(type);
        unsigned int count = material->GetTextureCount(textureType);

        for (unsigned int i = 0; i < count; ++i)
        {
            aiString path;
            if (material->GetTexture(textureType, i, &path) == AI_SUCCESS)
            {
                DebugLayer::Log(DebugLayer::LogType::INFO, "[Texture Found] Type: " + std::to_string(type) + " | Index: " + std::to_string(i) + " | Path: " + path.C_Str());
            }
        }
    }
}

FILE* ExporterHelper::OpenFile(std::filesystem::path filePath, uint32_t maxVersion)
{
    auto file = fopen(filePath.string().c_str(), "rb");

    if(file == nullptr)
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "Failed to open file " + filePath.string());
    }

    uint32_t version;
    fread(&version, sizeof(uint32_t), 1, file);

    if(version != maxVersion)
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "File '" + filePath.string() + "' version is too old : " + std::to_string(version) + " < " + std::to_string(maxVersion));
        fclose(file);
        file = nullptr;
    }

    return file;
}

VkFormat ExporterHelper::TextureTypeToFormat(TextureType type)
{
    switch (type)
    {
    case TextureType::BaseColor:
        return VK_FORMAT_BC7_SRGB_BLOCK;
    case TextureType::NormalMap:
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case TextureType::MRAO:
        return VK_FORMAT_BC7_UNORM_BLOCK;
    default:
        return VK_FORMAT_UNDEFINED;
    };
}

uint32_t ExporterHelper::BCCompress(TextureType type, void* data, std::vector<std::byte> &textureDatas, uint32_t width, uint32_t height)
{
    uint32_t blockXCount = (width + 3) / 4;
    uint32_t blockYCount = (height + 3) / 4;

    rgba_surface surface
    {
        .ptr = static_cast<uint8_t *>(data),
        .width = static_cast<int32_t>(width),
        .height = static_cast<int32_t>(height),
        .stride = static_cast<int32_t>(width * 4)
    };

    uint32_t offset = static_cast<uint32_t>(textureDatas.size());

    uint32_t compressedSize = blockXCount * blockYCount * 16;
    textureDatas.resize(textureDatas.size() + compressedSize);

    int gain = (1.0f - (compressedSize / static_cast<float>(width * height * 4))) * 100;

    DebugLayer::Log(DebugLayer::LogType::INFO, "Compressing texture " + std::to_string(width) + "x" + std::to_string(height) + " with a gain of " + std::to_string(gain) + "%");

    uint8_t* blockData = (uint8_t*)textureDatas.data() + offset;

    if(type == NormalMap)
    {
        CompressBlocksBC5(&surface, blockData);
    }
    else
    {

        bc7_enc_settings settings;
        GetProfile_ultrafast(&settings);
        CompressBlocksBC7(&surface, blockData, &settings);
    }
    return compressedSize;
}

MaterialType ExporterHelper::GetMaterialType(const aiMaterial* material)
{
    return MaterialType::Opaque;
}

TextureLoadingInfo ExporterHelper::LoadTexture(const aiMaterial* material, const std::filesystem::path rootPath, TextureType type, std::span<const aiTextureType> types, VkCommandPool uploadPool, VkQueue uploadQueue, std::vector<TextureExport>& texturesExportInfo, std::vector<std::byte>& textureDatas, std::vector<std::byte>& uncompressedDataCache)
{
    TextureLoadingInfo info
    {
        .Id = static_cast<uint32_t>(UINT32_MAX),
        .Format = VK_FORMAT_UNDEFINED
    };
    uint32_t textureCount = 0;
    aiTextureType textureType = ExporterHelper::SelectTextureType(types, material, textureCount);
    if(textureCount == 0) return info;

    aiString path;
    material->GetTexture(textureType, 0, &path);

    if(path.length == 0)
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "Found a texture with a 0 length path");
        return info;
    }

    std::string rawPath = path.C_Str();
    std::replace(rawPath.begin(), rawPath.end(), '\\', '/');

    std::filesystem::path texturePath = rootPath / std::filesystem::path(rawPath).filename();

    if(!std::filesystem::exists(texturePath))
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "Path does not exist " + texturePath.string());
        return info;
    }
    else if(texturePath.extension() == ".dds") // handle compressed textures directly
    {
        tinyddsloader::DDSFile file;
        auto ret = file.Load(texturePath.string().c_str());
        if(ret != tinyddsloader::Result::Success)
        {
            DebugLayer::Log(DebugLayer::LogType::WARNING, "Failed to load compressed texture " + texturePath.string());
            return info;
        }

        auto mip0Data = file.GetImageData(0);
        TextureExport mip0
        {
            .Offset = static_cast<uint32_t>(textureDatas.size()),
            .Size = mip0Data->m_memSlicePitch,
            .Width = mip0Data->m_width,
            .Height = mip0Data->m_height,
            .Format = ImageHelper::DXGIToVkFormat(file.GetFormat()),
            .MipCount = file.GetMipCount()
        };

        if(mip0.MipCount > 16)
        {
            // I don't think I will ever hit this but just in case
            DebugLayer::Log(DebugLayer::LogType::WARNING, "Texture has too many mip levels " + texturePath.string());
            return info;
        }

        uint32_t textureIndex = texturesExportInfo.size();
        texturesExportInfo.resize(textureIndex + mip0.MipCount);

        VkDeviceSize totalSize = 0;
        for (uint32_t i = 0; i < mip0.MipCount; i++)
        {
            totalSize += file.GetImageData(i)->m_memSlicePitch;
        }

        textureDatas.resize(textureDatas.size() + totalSize);

        memcpy(textureDatas.data() + mip0.Offset, mip0Data->m_mem, mip0.Size);

        texturesExportInfo[textureIndex] = mip0;

        uint32_t offset = mip0.Offset + mip0.Size;

        for (uint32_t i = 1; i < mip0.MipCount; i++)
        {
            auto fileData = file.GetImageData(i);

            TextureExport mip
            {
                .Type = type,
                .Offset = mip0.Offset + offset,
                .Size = fileData->m_memSlicePitch,
                .Width = fileData->m_width,
                .Height = fileData->m_height,
                .Format = mip0.Format,
                .MipCount = 0
            };

            memcpy(textureDatas.data() + offset, fileData->m_mem, mip.Size);

            texturesExportInfo[textureIndex + i] = mip;
            offset += mip.Size;
        }

        info.Id = textureIndex;
        info.Format = mip0.Format;

        return info;
    }
    else
    {
        GPUAllocatedImage image(CPUImage(texturePath.string(), STBI_rgb_alpha), uploadPool, uploadQueue);

        TextureExport mip0
        {
            .Offset = 0,
            .Size = 0,
            .Width = (uint32_t)image.Width(),
            .Height = (uint32_t)image.Height(),
            .Format = image.Internal().Format,
            .MipCount = image.MipCount()
        };

        size_t offset = textureDatas.size();

        if(mip0.MipCount > 16)
        {
            // I don't think I will ever hit this but just in case
            DebugLayer::Log(DebugLayer::LogType::WARNING, "Texture has too many mip levels " + texturePath.string());
            return info;
        }

        uint32_t textureIndex = texturesExportInfo.size();
        texturesExportInfo.resize(texturesExportInfo.size() + mip0.MipCount);

        VkBufferImageCopy region[16];
        uint32_t curentWidth = mip0.Width;
        uint32_t currentHeight = mip0.Height;
        uint32_t size = 0;

        uint32_t bytePerPixel = ImageHelper::GetBytePerPixel(mip0.Format);

        for(uint32_t i = 0; i < mip0.MipCount; i++)
        {
            region[i] =
            {
                .bufferOffset = size,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = i,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
                .imageOffset =
                {
                    .x = 0,
                    .y = 0,
                    .z = 0
                },
                .imageExtent =
                {
                    .width = curentWidth,
                    .height = currentHeight,
                    .depth = 1
                }
            };

            if(i > 0)
            {
                texturesExportInfo[textureIndex + i] =
                {
                    .Offset = size,
                    .Size = curentWidth * currentHeight * bytePerPixel,
                    .Width = curentWidth,
                    .Height = currentHeight,
                    .Format = mip0.Format,
                    .MipCount = 0
                };
            }

            size += curentWidth * currentHeight * bytePerPixel;
            curentWidth = std::max(1u, curentWidth / 2);
            currentHeight = std::max(1u, currentHeight / 2);
        }
        texturesExportInfo[textureIndex] = mip0;

        if(uncompressedDataCache.size() < size) uncompressedDataCache.resize(size);

        StageBuffer stagingBuffer(size, StageBuffer::Usage::Readback);

        CommandBuffer cmdBuffer(uploadPool, uploadQueue);
        cmdBuffer.BeginSingleTime();
        {
            ImageHelper::TransitionLayoutCommand(cmdBuffer, image.Internal(), 0, mip0.MipCount, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            cmdBuffer.CopyImageToBuffer(image.Internal().Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.Internal(), region, mip0.MipCount);
            ImageHelper::TransitionLayoutCommand(cmdBuffer, image.Internal(), 0, mip0.MipCount, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        cmdBuffer.End();
        cmdBuffer.WaitCompletion();

        stagingBuffer.MapAndCopyToData(uncompressedDataCache.data(), size);

        uint32_t compressedOffset = mip0.Offset + offset;
        for(uint32_t i = 0; i < mip0.MipCount; i++)
        {
            auto & texture = texturesExportInfo[textureIndex + i];
            std::byte* uncompressedTextureData = uncompressedDataCache.data() + texture.Offset;
            uint32_t compressedSize = ExporterHelper::BCCompress(type, uncompressedTextureData, textureDatas, texture.Width, texture.Height);

            texture.Format = ExporterHelper::TextureTypeToFormat(type);
            texture.Offset = compressedOffset;
            texture.Size = compressedSize;

            compressedOffset += compressedSize;
        }

        info.Format = ExporterHelper::TextureTypeToFormat(type);
        info.Id = textureIndex;

        return info;
    }
}