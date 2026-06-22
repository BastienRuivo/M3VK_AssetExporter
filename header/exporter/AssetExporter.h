#pragma once

#include "exporter/Exporter.h"
#include "exporter/Vertex.h"

struct AssetExporter : public Exporter
{
    // V0 -> Initial Version
    // V1 -> Added BaseColor, Metallic, Roughness textures ID
    // V2 -> Added AABB
    // V3 -> Adding multiple shader supports
    // V4 -> Adding Tangeants
    // V5 -> Memory leak fix
    static constexpr uint32_t VERSION = 5;

    AssetExporterHeader Header;

    std::vector<MaterialExport> Materials;
    std::vector<TextureExport> Textures;
    std::vector<SubMeshExport> SubMeshes;
    std::vector<std::byte> TextureDatas;
    std::vector<Vertex> VertexDatas;
    std::vector<uint32_t> IndexDatas;

    std::vector<std::byte> UncompressedDataCache;

    AssetExporter() = default;
    AssetExporter(AssetExporter&& other) noexcept;
    AssetExporter& operator=(AssetExporter&& other) noexcept;

    static std::filesystem::path FindTextureRoot(std::filesystem::path current_path) ;
    static AssetExporter Load3DModel(const std::filesystem::path & modelPath, VkCommandPool uploadPool, VkQueue uploadQueue);

    void Write(const std::filesystem::path& destinationPath) const override;
    bool Load(const std::filesystem::path& sourcePath) override;
    void Clear() override;
};
