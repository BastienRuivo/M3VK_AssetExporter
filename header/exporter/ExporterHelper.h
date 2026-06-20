#pragma once

#include "assimp/material.h"
#include "exporter/Exporter.h"
#include <cstdint>
#include <filesystem>
#include <span>
#include <vulkan/vulkan_core.h>

namespace ExporterHelper
{
    aiTextureType SelectTextureType(std::span<const aiTextureType> types, const aiMaterial* material, uint32_t& textureCount);
    void DebugListMaterialTextures(aiMaterial* material);
    std::filesystem::path GetTexturePath(const std::filesystem::path& modelPath);

    FILE* OpenFile(std::filesystem::path filePath, uint32_t maxVersion);

    VkFormat TextureTypeToFormat(TextureType type);

    MaterialType GetMaterialType(const aiMaterial* material);
    TextureLoadingInfo LoadTexture(const aiMaterial* material, const std::filesystem::path rootPath, TextureType type, std::span<const aiTextureType> types, VkCommandPool uploadPool, VkQueue uploadQueue, std::vector<TextureExport>& texturesExportInfo, std::vector<std::byte>& textureDatas, std::vector<std::byte>& uncompressedDataCache);
    uint32_t BC7Compress(void* data, uint32_t size, std::vector<std::byte> &textureDatas, uint32_t width, uint32_t height, uint32_t depth, uint32_t channels);
}
