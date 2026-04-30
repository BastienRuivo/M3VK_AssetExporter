

#include <cstdio>

#include "asset/AssetExporter.h"
#include "application/DebugLayer.h"
#include "asset/AssetHelper.h"
#include "asset/CPUImage.h"
#include "assimp/Importer.hpp"
#include "assimp/defs.h"
#include "assimp/material.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "rendering/CommandBuffer.h"
#include "rendering/GPUImage.h"
#include "rendering/GraphicsBuffer.h"
#include "rendering/ImageHelper.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <vulkan/vulkan_core.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <ispc_texcomp.h>

VkFormat AssetExporter::TextureTypeToFormat(TextureType type)
{
    switch (type)
    {
    case TextureType::BaseColor:
        return VK_FORMAT_BC7_SRGB_BLOCK;
    case TextureType::NormalMap:
        return VK_FORMAT_BC7_UNORM_BLOCK;
    case TextureType::MRAO:
        return VK_FORMAT_BC7_UNORM_BLOCK;
    default:
        return VK_FORMAT_UNDEFINED;
    };
}

uint32_t BC7Compress(void* data, uint32_t size, std::vector<std::byte> &textureDatas, uint32_t width, uint32_t height, uint32_t depth, uint32_t channels)
{
    uint32_t blockXCount = (width + 3) / 4;
    uint32_t blockYCount = (height + 3) / 4;

    bc7_enc_settings settings;
    GetProfile_ultrafast(&settings);

    rgba_surface surface
    {
        .ptr = static_cast<uint8_t *>(data),
        .width = static_cast<int32_t>(width),
        .height = static_cast<int32_t>(height),
        .stride = static_cast<int32_t>(width * channels)
    };

    uint32_t offset = static_cast<uint32_t>(textureDatas.size());

    uint32_t compressedSize = blockXCount * blockYCount * 16;
    textureDatas.resize(textureDatas.size() + compressedSize);

    int gain = (1.0f - (compressedSize / static_cast<float>(width * height * channels))) * 100;

    DebugLayer::Log(DebugLayer::LogType::INFO, "Compressing texture " + std::to_string(width) + "x" + std::to_string(height) + "x" + std::to_string(depth) + " with " + std::to_string(channels) + " channels with a gain of " + std::to_string(gain) + "%");

    uint8_t* blockData = (uint8_t*)textureDatas.data() + offset;
    CompressBlocksBC7(&surface, blockData, &settings);
    return compressedSize;
}

std::filesystem::path FindTextureRoot(std::filesystem::path current_path) {
    // If the input is a file, start the search from its parent directory
    if (std::filesystem::is_regular_file(current_path)) {
        current_path = current_path.parent_path();
    }

    // Traverse upwards until we reach the root directory
    while (!current_path.empty() && current_path.has_relative_path()) {
        std::filesystem::path potential_textures = current_path / "textures";

        // Check if "textures" exists and is actually a directory
        if (std::filesystem::exists(potential_textures) && std::filesystem::is_directory(potential_textures)) {
            return potential_textures;
        }

        // Move to the parent directory for the next iteration
        current_path = current_path.parent_path();
    }

    DebugLayer::Log(DebugLayer::LogType::WARNING, "Could not find textures directory " + current_path.string());

    return {}; // Return empty path if not found
}

uint32_t AssetExporter::LoadTexture(AssetExporter& exporter, const aiMaterial* material, const std::filesystem::path rootPath, TextureType type, std::span<const aiTextureType> types, VkCommandPool uploadPool, VkQueue uploadQueue)
{
    uint32_t textureCount = 0;
    aiTextureType textureType = AssetHelper::SelectTextureType(types, material, textureCount);

    if(textureCount == 0) return UINT32_MAX;

    aiString path;
    material->GetTexture(textureType, 0, &path);

    if(path.length == 0)
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "Found a texture with a 0 length path");
        return UINT32_MAX;
    }

    std::string rawPath = path.C_Str();
    std::replace(rawPath.begin(), rawPath.end(), '\\', '/');

    std::filesystem::path texturePath = rootPath / std::filesystem::path(rawPath).filename();

    if(!std::filesystem::exists(texturePath))
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "Path does not exist " + texturePath.string());
        return UINT32_MAX;
    }
    else if(texturePath.extension() == ".dds") // handle compressed textures directly
    {
        tinyddsloader::DDSFile file;
        auto ret = file.Load(texturePath.string().c_str());
        if(ret != tinyddsloader::Result::Success)
        {
            DebugLayer::Log(DebugLayer::LogType::WARNING, "Failed to load compressed texture " + texturePath.string());
            return false;
        }

        auto mip0Data = file.GetImageData(0);
        TextureExport mip0
        {
            .Offset = static_cast<uint32_t>(exporter.TextureDatas.size()),
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
            return false;
        }

        uint32_t textureIndex = exporter.Textures.size();
        exporter.Textures.resize(textureIndex + mip0.MipCount);

        VkDeviceSize totalSize = 0;
        for (uint32_t i = 0; i < mip0.MipCount; i++)
        {
            totalSize += file.GetImageData(i)->m_memSlicePitch;
        }

        exporter.TextureDatas.resize(exporter.TextureDatas.size() + totalSize);

        memcpy(exporter.TextureDatas.data() + mip0.Offset, mip0Data->m_mem, mip0.Size);

        exporter.Textures[textureIndex] = mip0;

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

            memcpy(exporter.TextureDatas.data() + offset, fileData->m_mem, mip.Size);

            exporter.Textures[textureIndex + i] = mip;
            offset += mip.Size;
        }

        return textureIndex;
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

        size_t offset = exporter.TextureDatas.size();

        if(mip0.MipCount > 16)
        {
            // I don't think I will ever hit this but just in case
            DebugLayer::Log(DebugLayer::LogType::WARNING, "Texture has too many mip levels " + texturePath.string());
            return false;
        }

        uint32_t textureIndex = exporter.Textures.size();
        exporter.Textures.resize(exporter.Textures.size() + mip0.MipCount);

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
                exporter.Textures[textureIndex + i] =
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
        mip0.Size = mip0.Width * mip0.Height * bytePerPixel;
        exporter.Textures[textureIndex] = mip0;

        if(exporter.UncompressedDataCache.size() < size) exporter.UncompressedDataCache.resize(size);

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

        stagingBuffer.MapAndCopyToData(exporter.UncompressedDataCache.data(), size);

        uint32_t compressedOffset = mip0.Offset;
        for(uint32_t i = 0; i < mip0.MipCount; i++)
        {
            auto & texture = exporter.Textures[textureIndex + i];
            std::byte* uncompressedTextureData = exporter.UncompressedDataCache.data() + texture.Offset;
            uint32_t compressedSize = BC7Compress(uncompressedTextureData, texture.Size, exporter.TextureDatas, texture.Width, texture.Height, 1, ImageHelper::GetBytePerPixel(texture.Format));

            texture.Format = AssetExporter::TextureTypeToFormat(type);
            texture.Offset = compressedOffset;
            texture.Size = compressedSize;

            compressedOffset += compressedSize;
        }

        return textureIndex;
    }
}

AssetExporter AssetExporter::Load3DModel(const std::filesystem::path & modelPath, VkCommandPool uploadPool, VkQueue uploadQueue)
{
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

    AssetExporter exporter;
    DebugLayer::Log(DebugLayer::LogType::INFO, "Loading model: " + modelPath.string());
    AssetExporter::Clear(exporter);

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(modelPath,
        aiProcess_Triangulate
        | aiProcess_JoinIdenticalVertices
        | aiProcess_SortByPType
        | aiProcess_GenUVCoords
        | aiProcess_FlipUVs
        | aiProcess_GlobalScale     // Handles FBX unit scaling (cm to m)
        | aiProcess_PreTransformVertices // Collapses the node hierarchy into the verticess
        | aiProcess_GenNormals
    );

    if(scene == nullptr)
    {
        DebugLayer::Log(DebugLayer::LogType::ERROR, importer.GetErrorString());
        throw std::runtime_error(importer.GetErrorString());
    }

    std::filesystem::path textureRootPath = FindTextureRoot(modelPath);

    exporter.Materials.resize(scene->mNumMaterials);

    int textureCount = 1;

    for(unsigned int i = 0; i < scene->mNumMaterials; i++)
    {
        aiMaterial* material = scene->mMaterials[i];

        // get base color value
        aiColor4D color;
        material->Get(AI_MATKEY_COLOR_DIFFUSE, color);

        ai_real metallic;
        material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);

        ai_real roughness;
        material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);


        MaterialProperties gpuMaterial
        {
            .BaseColor = glm::vec4(static_cast<float>(color.r),
                static_cast<float>(color.g),
                static_cast<float>(color.b),
                static_cast<float>(color.a)),
            .Metallic = static_cast<float>(metallic),
            .Roughness = static_cast<float>(roughness)
        };

        uint32_t baseColorID = AssetExporter::LoadTexture(exporter, material, textureRootPath, TextureType::BaseColor, {{ aiTextureType::aiTextureType_BASE_COLOR, aiTextureType::aiTextureType_DIFFUSE }}, uploadPool, uploadQueue);
        uint32_t normalMapID = AssetExporter::LoadTexture(exporter, material, textureRootPath, TextureType::NormalMap, {{ aiTextureType::aiTextureType_NORMALS }}, uploadPool, uploadQueue);
        uint32_t mraoID = AssetExporter::LoadTexture(exporter, material, textureRootPath, TextureType::MRAO, {{ aiTextureType::aiTextureType_AMBIENT_OCCLUSION }}, uploadPool, uploadQueue);

        exporter.Materials[i]  =
        {
            .BaseColorTexId = baseColorID,
            .NormalMapTexId = normalMapID,
            .MRAOTexId = mraoID,
            .MatProperties = gpuMaterial
        };
    }

    uint32_t meshCount = scene->mNumMeshes;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;

    exporter.SubMeshes.resize(meshCount);

    for(unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
        const aiMesh* mesh = scene->mMeshes[i];
        vertexCount += mesh->mNumVertices;
        indexCount += mesh->mNumFaces * 3;
    }

    exporter.VertexDatas.resize(vertexCount);
    exporter.IndexDatas.resize(indexCount);

    uint32_t meshOffset = 0;
    uint32_t indexOffset = 0;

    for(unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
        const aiMesh* mesh = scene->mMeshes[i];

        bool hasTexcoords = mesh->HasTextureCoords(0);
        bool hasNormals = mesh->HasNormals();

        for(unsigned int j = 0; j < mesh->mNumVertices; j++)
        {
            exporter.VertexDatas[meshOffset + j] = Vertex
            {
                .pos = glm::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z),
                .normal = hasNormals ? glm::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z) : glm::vec3(0, 1, 0),
                .texCoord = hasTexcoords ? glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y) : glm::vec2(0.0f)
            };
        }

        for(unsigned int j = 0; j < mesh->mNumFaces; j++)
        {
            aiFace face = mesh->mFaces[j];
            exporter.IndexDatas[indexOffset + j * 3 + 0] = face.mIndices[0];
            exporter.IndexDatas[indexOffset + j * 3 + 1] = face.mIndices[1];
            exporter.IndexDatas[indexOffset + j * 3 + 2] = face.mIndices[2];
        }

        exporter.SubMeshes[i] =
        {
            .MaterialIndex = mesh->mMaterialIndex,
            .VertexOffset = meshOffset,
            .VertexCount = mesh->mNumVertices,
            .IndexOffset = indexOffset,
            .IndexCount = mesh->mNumFaces * 3
        };

        meshOffset += mesh->mNumVertices;
        indexOffset += mesh->mNumFaces * 3;
    }

    exporter.Version = VERSION;
    exporter.Header =
    {
        .MaterialCount = static_cast<uint64_t>(exporter.Materials.size()),
        .TextureCount = static_cast<uint64_t>(exporter.Textures.size()),
        .TextureDataCount = static_cast<uint64_t>(exporter.TextureDatas.size()),
        .SubMeshCount = static_cast<uint64_t>(exporter.SubMeshes.size()),
        .VertexCount = static_cast<uint64_t>(exporter.VertexDatas.size()),
        .IndexCount = static_cast<uint64_t>(exporter.IndexDatas.size())
    };

    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();

    DebugLayer::Log(DebugLayer::LogType::INFO, "Asset Exported in " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()) + " ms");

    return exporter;
}

AssetExporter::AssetExporter(AssetExporter&& other) noexcept
{
    Header = std::move(other.Header);
    Materials = std::move(other.Materials);
    Textures = std::move(other.Textures);
    SubMeshes = std::move(other.SubMeshes);
    TextureDatas = std::move(other.TextureDatas);
    VertexDatas = std::move(other.VertexDatas);
    IndexDatas = std::move(other.IndexDatas);
}

AssetExporter& AssetExporter::operator=(AssetExporter&& other) noexcept
{
    if(this != &other)
    {
        Header = std::move(other.Header);
        Materials = std::move(other.Materials);
        Textures = std::move(other.Textures);
        SubMeshes = std::move(other.SubMeshes);
        TextureDatas = std::move(other.TextureDatas);
        VertexDatas = std::move(other.VertexDatas);
        IndexDatas = std::move(other.IndexDatas);
    }
    return *this;
}

void AssetExporter::Write(const AssetExporter& exporter, const std::filesystem::path& destinationPath)
{
    remove(destinationPath.string().c_str());

    // if path does not exist, create it
    if(!std::filesystem::exists(destinationPath.parent_path()))
    {
        std::filesystem::create_directories(destinationPath.parent_path());
    }

    auto file = fopen(destinationPath.string().c_str(), "wb");

    if(file == nullptr)
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "Failed to open file " + destinationPath.string());
        return;
    }

    fwrite(&exporter.Version, sizeof(uint32_t), 1, file);
    fwrite(&exporter.Header, sizeof(AssetExporterHeader), 1, file);

    // DATAS
    fwrite(exporter.Materials.data(), sizeof(MaterialExport), exporter.Header.MaterialCount, file);
    fwrite(exporter.Textures.data(), sizeof(TextureExport), exporter.Header.TextureCount, file);
    fwrite(exporter.SubMeshes.data(), sizeof(SubMeshExport), exporter.Header.SubMeshCount, file);
    fwrite(exporter.VertexDatas.data(), sizeof(Vertex), exporter.Header.VertexCount, file);
    fwrite(exporter.IndexDatas.data(), sizeof(uint32_t), exporter.Header.IndexCount, file);
    fwrite(exporter.TextureDatas.data(), sizeof(std::byte), exporter.Header.TextureDataCount, file);

    fclose(file);

    // write all textures into bitmap file for debug purpose
    // for(uint32_t i = 0; i < exporter.Header.TextureCount; i++)
    // {
    //     auto & texture = exporter.Textures[i];
    //     std::filesystem::path texturePath = destinationPath.parent_path() / (std::to_string(i) + ".bmp");

    //     stbi_write_bmp(texturePath.c_str(), texture.Width, texture.Height, 4, exporter.TextureDatas.data() + texture.Offset);
    // }
}

bool AssetExporter::Load(AssetExporter& exporter, const std::filesystem::path& sourcePath)
{
    auto file = fopen(sourcePath.string().c_str(), "rb");

    if(file == nullptr)
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "Failed to open file " + sourcePath.string());
        return false;
    }

    fread(&exporter.Version, sizeof(uint32_t), 1, file);

    if(exporter.Version < VERSION)
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "File '" + sourcePath.string() + "' version is too old : " + std::to_string(exporter.Version) + " < " + std::to_string(VERSION));
        return false;
    }

    fread(&exporter.Header, sizeof(AssetExporterHeader), 1, file);

    // DATAS
    exporter.Materials.resize(exporter.Header.MaterialCount);
    fread(exporter.Materials.data(), sizeof(MaterialExport), exporter.Header.MaterialCount, file);

    exporter.Textures.resize(exporter.Header.TextureCount);
    fread(exporter.Textures.data(), sizeof(TextureExport), exporter.Header.TextureCount, file);

    exporter.SubMeshes.resize(exporter.Header.SubMeshCount);
    fread(exporter.SubMeshes.data(), sizeof(SubMeshExport), exporter.Header.SubMeshCount, file);

    exporter.VertexDatas.resize(exporter.Header.VertexCount);
    fread(exporter.VertexDatas.data(), sizeof(Vertex), exporter.Header.VertexCount, file);

    exporter.IndexDatas.resize(exporter.Header.IndexCount);
    fread(exporter.IndexDatas.data(), sizeof(uint32_t), exporter.Header.IndexCount, file);

    exporter.TextureDatas.resize(exporter.Header.TextureDataCount);
    fread(exporter.TextureDatas.data(), sizeof(std::byte), exporter.Header.TextureDataCount, file);

    fclose(file);

    return true;
}

void AssetExporter::Clear(AssetExporter& exporter)
{
    exporter.Materials.clear();
    exporter.Textures.clear();
    exporter.SubMeshes.clear();
    exporter.VertexDatas.clear();
    exporter.IndexDatas.clear();
    exporter.TextureDatas.clear();
}
