#pragma once

#include "assimp/material.h"
#include <cstdint>
#include <filesystem>
#include <span>

namespace AssetHelper
{
    aiTextureType SelectTextureType(std::span<const aiTextureType> types, const aiMaterial* material, uint32_t& textureCount);
    void DebugListMaterialTextures(aiMaterial* material);
    std::filesystem::path GetTexturePath(const std::filesystem::path& modelPath);
}
