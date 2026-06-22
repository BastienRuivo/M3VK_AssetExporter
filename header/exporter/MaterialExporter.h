#pragma once

#include "exporter/Exporter.h"

struct MaterialExporter : public Exporter
{
    static constexpr uint32_t VERSION = 0;

    struct Header
    {
        uint64_t TextureCount;
        uint64_t TextureDataCount;
        MaterialExport Material;
    };

    Header Header;

    std::vector<TextureExport> Textures;
    std::vector<std::byte> TextureDatas;

    std::vector<std::byte> UncompressedDataCache;

    MaterialExporter() = default;
    MaterialExporter(MaterialExporter&& other) noexcept;
    MaterialExporter& operator=(MaterialExporter&& other) noexcept;

    static std::filesystem::path FindTextureRoot(const std::filesystem::path& current_path) ;
    static MaterialExporter LoadMaterial(const std::filesystem::path & path, VkCommandPool uploadPool, VkQueue uploadQueue);

    void Write(const std::filesystem::path& destinationPath) const override;
    bool Load(const std::filesystem::path& sourcePath) override;
    void Clear() override;
};
