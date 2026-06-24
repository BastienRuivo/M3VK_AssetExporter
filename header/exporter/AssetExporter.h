#pragma once

#include "assimp/material.h"
#include "exporter/Exporter.h"
#include "exporter/Vertex.h"
#include <span>



struct AssetExporter : public Exporter
{
    struct Header
    {
        uint64_t MaterialCount;
        uint64_t TextureCount;
        uint64_t TextureDataCount;
        uint64_t SubMeshCount;
        uint64_t VertexCount;
        uint64_t IndexCount;
    };
    // V0 -> Initial Version
    // V1 -> Added BaseColor, Metallic, Roughness textures ID
    // V2 -> Added AABB
    // V3 -> Adding multiple shader supports
    // V4 -> Adding Tangeants
    // V5 -> Memory leak fix
    static constexpr uint32_t VERSION = 5;

    Header Header;

    std::vector<MaterialExport> Materials;
    std::vector<TextureExport> Textures;
    std::vector<SubMeshExport> SubMeshes;
    std::vector<std::byte> TextureDatas;
    std::vector<Vertex> VertexDatas;
    std::vector<uint32_t> IndexDatas;

    AssetExporter() = default;
    AssetExporter(AssetExporter&& other) noexcept;
    AssetExporter& operator=(AssetExporter&& other) noexcept;

    static std::filesystem::path GetTexturePath(aiMaterial* material, std::span<const aiTextureType> types, const std::filesystem::path& rootPath);
    static std::filesystem::path FindTextureRoot(std::filesystem::path current_path) ;
    static AssetExporter Load(const std::filesystem::path & path, VkCommandPool uploadPool, VkQueue uploadQueue);

    void Write(const std::filesystem::path& destinationPath) const override;
    bool LoadData(const std::filesystem::path& sourcePath) override;
    void Clear() override;
};
