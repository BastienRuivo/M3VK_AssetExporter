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
