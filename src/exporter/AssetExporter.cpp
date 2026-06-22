#include <cstdio>

#include "exporter/AssetExporter.h"
#include "application/DebugLayer.h"
#include "exporter/ExporterHelper.h"
#include "assimp/Importer.hpp"
#include "assimp/defs.h"
#include "assimp/material.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <vulkan/vulkan_core.h>


AssetExporter AssetExporter::Load3DModel(const std::filesystem::path & modelPath, VkCommandPool uploadPool, VkQueue uploadQueue)
{
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

    AssetExporter exporter;
    DebugLayer::Log(DebugLayer::LogType::INFO, "Loading model: " + modelPath.string());
    exporter.Clear();

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
        | aiProcess_GenBoundingBoxes
        | aiProcess_CalcTangentSpace
    );

    if(scene == nullptr)
    {
        DebugLayer::Log(DebugLayer::LogType::ERROR, importer.GetErrorString());
        throw std::runtime_error(importer.GetErrorString());
    }

    std::filesystem::path textureRootPath = AssetExporter::FindTextureRoot(modelPath);

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

        bool isTwoSided = false;
        material->Get(AI_MATKEY_TWOSIDED, isTwoSided);

        MaterialType materialType = ExporterHelper::GetMaterialType(material);


        if(isTwoSided)
        {
            if(materialType == MaterialType::Cutout)
            {
                materialType = MaterialType::CutoutTwoSided;
            }
            else
            {
                DebugLayer::Log(DebugLayer::LogType::WARNING, "Two sided material is not supported for material type: " + std::to_string(materialType));
            }
        }

        auto baseColorInfo = ExporterHelper::LoadTexture(material, textureRootPath, TextureType::BaseColor, {{ aiTextureType::aiTextureType_BASE_COLOR, aiTextureType::aiTextureType_DIFFUSE }}, uploadPool, uploadQueue, exporter.Textures, exporter.TextureDatas, exporter.UncompressedDataCache);
        auto normalMapInfo = ExporterHelper::LoadTexture(material, textureRootPath, TextureType::NormalMap, {{ aiTextureType::aiTextureType_NORMALS }}, uploadPool, uploadQueue, exporter.Textures, exporter.TextureDatas, exporter.UncompressedDataCache);
        auto mraoInfo = ExporterHelper::LoadTexture(material, textureRootPath, TextureType::MRAO, {{ aiTextureType::aiTextureType_AMBIENT_OCCLUSION }}, uploadPool, uploadQueue, exporter.Textures, exporter.TextureDatas, exporter.UncompressedDataCache);

        if(baseColorInfo.Format == VK_FORMAT_BC3_UNORM_BLOCK || baseColorInfo.Format == VK_FORMAT_BC3_SRGB_BLOCK)
        {
            materialType = isTwoSided ? MaterialType::CutoutTwoSided : MaterialType::Cutout;
        }

        MaterialExport materialProperties =
        {
            .BaseColorTexId = baseColorInfo.Id,
            .NormalMapTexId = normalMapInfo.Id,
            .MRAOTexId = mraoInfo.Id,
            .MaterialType = materialType,

            .BaseColor = glm::vec4(static_cast<float>(color.r),
                static_cast<float>(color.g),
                static_cast<float>(color.b),
                static_cast<float>(color.a)),
            .Metallic = static_cast<float>(metallic),
            .Roughness = static_cast<float>(roughness)
        };

        exporter.Materials[i] = materialProperties;
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

        for(unsigned int j = 0; j < mesh->mNumVertices; j++)
        {
            exporter.VertexDatas[meshOffset + j] = Vertex
            {
                .pos = glm::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z),
                .normal =  glm::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z),
                .tangent = glm::vec4(mesh->mTangents[j].x, mesh->mTangents[j].y, mesh->mTangents[j].z, 0.0f),
                .texCoord = hasTexcoords ? glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y) : glm::vec2(0.0f)
            };

            glm::vec3 tangent = glm::vec3(mesh->mTangents[j].x, mesh->mTangents[j].y, mesh->mTangents[j].z);
            glm::vec3 expectedBitangent = glm::cross(exporter.VertexDatas[meshOffset + j].normal, tangent);

            float handedness = glm::dot(expectedBitangent, glm::vec3(mesh->mBitangents[j].x, mesh->mBitangents[j].y, mesh->mBitangents[j].z));
            exporter.VertexDatas[meshOffset + j].tangent.w = handedness < 0.0f ? -1.0f : 1.0f;
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
            .IndexCount = mesh->mNumFaces * 3,
            .AABBMin = glm::vec3(mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z),
            .AABBMax = glm::vec3(mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z)
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

void AssetExporter::Write(const std::filesystem::path& destinationPath) const
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

    fwrite(&Version, sizeof(uint32_t), 1, file);
    fwrite(&Header, sizeof(AssetExporterHeader), 1, file);

    // DATAS
    fwrite(Materials.data(), sizeof(MaterialExport), Header.MaterialCount, file);
    fwrite(Textures.data(), sizeof(TextureExport), Header.TextureCount, file);
    fwrite(SubMeshes.data(), sizeof(SubMeshExport), Header.SubMeshCount, file);
    fwrite(VertexDatas.data(), sizeof(Vertex), Header.VertexCount, file);
    fwrite(IndexDatas.data(), sizeof(uint32_t), Header.IndexCount, file);
    fwrite(TextureDatas.data(), sizeof(std::byte), Header.TextureDataCount, file);

    fclose(file);
}

bool AssetExporter::Load(const std::filesystem::path& sourcePath)
{
    auto file = fopen(sourcePath.string().c_str(), "rb");

    if(file == nullptr)
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "Failed to open file " + sourcePath.string());
        return false;
    }

    fread(&Version, sizeof(uint32_t), 1, file);

    if(Version < VERSION)
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "File '" + sourcePath.string() + "' version is too old : " + std::to_string(Version) + " < " + std::to_string(VERSION));
        return false;
    }

    fread(&Header, sizeof(AssetExporterHeader), 1, file);

    // DATAS
    Materials.resize(Header.MaterialCount);
    fread(Materials.data(), sizeof(MaterialExport), Header.MaterialCount, file);

    Textures.resize(Header.TextureCount);
    fread(Textures.data(), sizeof(TextureExport), Header.TextureCount, file);

    SubMeshes.resize(Header.SubMeshCount);
    fread(SubMeshes.data(), sizeof(SubMeshExport), Header.SubMeshCount, file);

    VertexDatas.resize(Header.VertexCount);
    fread(VertexDatas.data(), sizeof(Vertex), Header.VertexCount, file);

    IndexDatas.resize(Header.IndexCount);
    fread(IndexDatas.data(), sizeof(uint32_t), Header.IndexCount, file);

    TextureDatas.resize(Header.TextureDataCount);
    fread(TextureDatas.data(), sizeof(std::byte), Header.TextureDataCount, file);

    fclose(file);

    return true;
}

void AssetExporter::Clear()
{
    Materials.clear();
    Textures.clear();
    SubMeshes.clear();
    VertexDatas.clear();
    IndexDatas.clear();
    TextureDatas.clear();
}

std::filesystem::path AssetExporter::FindTextureRoot(std::filesystem::path current_path)
{
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

    return current_path.remove_filename();
}
