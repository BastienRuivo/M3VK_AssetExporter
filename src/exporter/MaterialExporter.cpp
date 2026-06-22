#include <cstdio>

#include "exporter/MaterialExporter.h"
#include "application/DebugLayer.h"
#include "exporter/Exporter.h"
#include "exporter/ExporterHelper.h"
#include "nlohmann/json_fwd.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

MaterialExporter MaterialExporter::LoadMaterial(const std::filesystem::path & path, VkCommandPool uploadPool, VkQueue uploadQueue)
{
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

    MaterialExporter exporter;
    DebugLayer::Log(DebugLayer::LogType::INFO, "Loading material: " + path.string());
    exporter.Clear();

    std::ifstream file(path);

    if(!file.is_open())
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "Failed to open file " + path.string());
        return exporter;
    }

    MaterialExport materialProperties{};
    try
    {
        json j = json::parse(file);
        uint32_t version = j["version"];
        if(version != VERSION)
        {
            DebugLayer::Log(DebugLayer::LogType::WARNING, "File '" + path.string() + "' version is too old : " + std::to_string(version) + " < " + std::to_string(VERSION));
            return exporter;
        }

        materialProperties.BaseColorTexId = j["baseColorTexId"];
        materialProperties.NormalMapTexId = j["normalMapTexId"];
        materialProperties.MRAOTexId = j["mraoTexId"];
        materialProperties.MaterialType = j["materialType"];

        // base color is defined as "baseColor : {"r", "g", "b", "a"}"
        materialProperties.BaseColor = glm::vec4(static_cast<float>(j["baseColor"]["r"],
            j["baseColor"]["g"],
            j["baseColor"]["b"],
            j["baseColor"]["a"]));
        materialProperties.Metallic = static_cast<float>(j["metallic"]);
        materialProperties.Roughness = static_cast<float>(j["roughness"]);

        std::filesystem::path textureRootPath = MaterialExporter::FindTextureRoot(path);

        if(materialProperties.BaseColorTexId == 0)
        {
            std::string baseColor = j["baseColorTexPath"];
            auto baseColorInfo = ExporterHelper::LoadTexture(textureRootPath / baseColor, TextureType::BaseColor, uploadPool, uploadQueue, exporter.Textures, exporter.TextureDatas, exporter.UncompressedDataCache);
            materialProperties.BaseColorTexId = baseColorInfo.Id;
        }

        if(materialProperties.NormalMapTexId == 0)
        {
            std::string normalMap = j["normalMapTexPath"];
            auto normalMapInfo = ExporterHelper::LoadTexture(textureRootPath / normalMap, TextureType::NormalMap, uploadPool, uploadQueue, exporter.Textures, exporter.TextureDatas, exporter.UncompressedDataCache);
            materialProperties.NormalMapTexId = normalMapInfo.Id;
        }

        if(materialProperties.MRAOTexId == 0)
        {
            std::string mrao = j["mraoTexPath"];
            auto mraoInfo = ExporterHelper::LoadTexture(textureRootPath / mrao, TextureType::MRAO, uploadPool, uploadQueue, exporter.Textures, exporter.TextureDatas, exporter.UncompressedDataCache);
            materialProperties.BaseColorTexId = mraoInfo.Id;
        }
    }
    catch (const json::exception& e)
    {
        DebugLayer::Log(DebugLayer::LogType::ERROR, "Failed to parse file " + path.string() + " : " + e.what());
        return exporter;
    }

    exporter.Version = VERSION;
    exporter.Header =
    {
        .TextureCount = static_cast<uint64_t>(exporter.Textures.size()),
        .TextureDataCount = static_cast<uint64_t>(exporter.TextureDatas.size()),
        .Material = materialProperties
    };

    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();

    DebugLayer::Log(DebugLayer::LogType::INFO, "Material Exported in " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()) + " ms");

    return exporter;
}

MaterialExporter::MaterialExporter(MaterialExporter&& other) noexcept
{
    Header = std::move(other.Header);
    Textures = std::move(other.Textures);
    TextureDatas = std::move(other.TextureDatas);
}

MaterialExporter& MaterialExporter::operator=(MaterialExporter&& other) noexcept
{
    if(this != &other)
    {
        Header = std::move(other.Header);
        Textures = std::move(other.Textures);
        TextureDatas = std::move(other.TextureDatas);
    }
    return *this;
}

void MaterialExporter::Write(const std::filesystem::path& destinationPath) const
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
    fwrite(&Header, sizeof(Header), 1, file);

    fwrite(Textures.data(), sizeof(TextureExport), Header.TextureCount, file);
    fwrite(TextureDatas.data(), sizeof(std::byte), Header.TextureDataCount, file);

    fclose(file);
}

bool MaterialExporter::Load(const std::filesystem::path& sourcePath)
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

    fread(&Header, sizeof(Header), 1, file);

    Textures.resize(Header.TextureCount);
    fread(Textures.data(), sizeof(TextureExport), Header.TextureCount, file);

    TextureDatas.resize(Header.TextureDataCount);
    fread(TextureDatas.data(), sizeof(std::byte), Header.TextureDataCount, file);

    fclose(file);

    return true;
}

void MaterialExporter::Clear()
{
    Textures.clear();
    TextureDatas.clear();
}

std::filesystem::path MaterialExporter::FindTextureRoot(const std::filesystem::path& current_path)
{
    std::filesystem::path textureRoothPath = current_path.parent_path() / "textures";
    if(!std::filesystem::exists(textureRoothPath) || !std::filesystem::is_directory(textureRoothPath))
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "Could not find textures directory " + textureRoothPath.string());
    }
    return textureRoothPath;
}
