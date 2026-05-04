#pragma once

#include "asset/Vertex.h"
#include "assimp/material.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>
#include <vulkan/vulkan_core.h>

#define VERSION 1

enum TextureType
{
    BaseColor = 0,
    NormalMap = 1,
    MRAO = 2
};

struct TextureExport
{
    uint32_t Type;
    uint32_t Offset;
    uint32_t Size;
    uint32_t Width;
    uint32_t Height;
    VkFormat Format;
    uint32_t MipCount;
};

struct MaterialProperties
{
    alignas(4) uint32_t BaseColorTexId;
    alignas(4) uint32_t NormalMapTexId;
    alignas(4) uint32_t MRAOTexId;

    alignas(16) glm::vec4 BaseColor;
    alignas(4) float Metallic;
    alignas(4) float Roughness;

    static constexpr uint32_t Stride();

    static MaterialProperties Default()
    {
        return
        {
            .BaseColorTexId = UINT32_MAX,
            .NormalMapTexId = UINT32_MAX,
            .MRAOTexId = UINT32_MAX,
            .BaseColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .Metallic = 0.0f,
            .Roughness = 1.0f
        };
    }

    bool operator==(const MaterialProperties& other) const
    {
        return BaseColor == other.BaseColor;
    }
};

struct SubMeshExport
{
    uint32_t MaterialIndex;
    uint32_t VertexOffset;
    uint32_t VertexCount;
    uint32_t IndexOffset;
    uint32_t IndexCount;
};

struct AssetExporterHeader
{
    uint64_t MaterialCount;
    uint64_t TextureCount;
    uint64_t TextureDataCount;
    uint64_t SubMeshCount;
    uint64_t VertexCount;
    uint64_t IndexCount;
};

struct AssetExporter
{
    uint32_t Version;
    AssetExporterHeader Header;

    std::vector<MaterialProperties> Materials;
    std::vector<TextureExport> Textures;
    std::vector<SubMeshExport> SubMeshes;
    std::vector<std::byte> TextureDatas;
    std::vector<Vertex> VertexDatas;
    std::vector<uint32_t> IndexDatas;

    std::vector<std::byte> UncompressedDataCache;

    AssetExporter() = default;
    AssetExporter(AssetExporter&& other) noexcept;
    AssetExporter& operator=(AssetExporter&& other) noexcept;

    static VkFormat TextureTypeToFormat(TextureType type);

    static AssetExporter Load3DModel(const std::filesystem::path & modelPath, VkCommandPool uploadPool, VkQueue uploadQueue);
    static uint32_t LoadTexture(AssetExporter& exporter, const aiMaterial* material, const std::filesystem::path rootPath, TextureType type, std::span<const aiTextureType> types, VkCommandPool uploadPool, VkQueue uploadQueue);
    static void Write(const AssetExporter& exporter, const std::filesystem::path& destinationPath);
    static bool Load(AssetExporter& exporter, const std::filesystem::path& sourcePath);
    static void Clear(AssetExporter& exporter);
};
