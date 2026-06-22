#pragma once

#include "glm/fwd.hpp"
#include <cstdint>
#include <filesystem>
#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>

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

enum MaterialType
{
    Opaque,
    Cutout,
    CutoutTwoSided,
    Transparent
};

struct MaterialExport
{
    uint32_t BaseColorTexId;
    uint32_t NormalMapTexId;
    uint32_t MRAOTexId;
    uint32_t MaterialType;

    glm::vec4 BaseColor;
    float Metallic;
    float Roughness;

    static constexpr uint32_t Stride();

    static MaterialExport Default()
    {
        return
        {
            .BaseColorTexId = UINT32_MAX,
            .NormalMapTexId = UINT32_MAX,
            .MRAOTexId = UINT32_MAX,
            .MaterialType = Opaque,

            .BaseColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .Metallic = 0.0f,
            .Roughness = 1.0f
        };
    }

    bool operator==(const MaterialExport& other) const
    {
        return BaseColor == other.BaseColor
            && Metallic == other.Metallic
            && Roughness == other.Roughness
            && BaseColorTexId == other.BaseColorTexId
            && NormalMapTexId == other.NormalMapTexId
            && MRAOTexId == other.MRAOTexId
            && MaterialType == other.MaterialType;
    }
};

struct SubMeshExport
{
    uint32_t MaterialIndex;
    uint32_t VertexOffset;
    uint32_t VertexCount;
    uint32_t IndexOffset;
    uint32_t IndexCount;
    glm::vec3 AABBMin;
    glm::vec3 AABBMax;
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

struct TextureLoadingInfo
{
    uint32_t Id;
    VkFormat Format;
};

struct Exporter
{
    uint32_t Version;

    virtual void Write(const std::filesystem::path& destinationPath) const = 0;
    virtual bool Load(const std::filesystem::path& sourcePath) = 0;
    virtual void Clear() = 0;
};
