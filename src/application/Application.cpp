#include "application/Application.h"
#include "application/DebugLayer.h"
#include "asset/AssetExporter.h"
#include <filesystem>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <glm/trigonometric.hpp>

#ifdef M3VK_MEMORYLOG
#include <string>
#endif

#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

Application::Application() :
    _instance(),
    _vkDebugLayer(_instance.Internal()),
    _physicalDevice(_instance.Internal(), _deviceExtensions),
    _device(_deviceExtensions),

    // Queues & Swapchain
    _uploadQueue(VkQueueHandler::Graphics),
    _uploadCommandPool()
{

}

Application::~Application()
{
}

void RecursiveModelSearch(std::filesystem::path path, std::vector<std::filesystem::path>& toExport)
{
    if(std::filesystem::is_directory(path))
    {
        for(const auto& entry : std::filesystem::directory_iterator(path))
        {
            RecursiveModelSearch(entry.path(), toExport);
        }
    }
    else if(std::filesystem::is_regular_file(path) && path.extension() == ".fbx" || path.extension() == ".obj" || path.extension() == ".gltf")
    {
        toExport.push_back(path);
    }
}

void Application::Export(std::filesystem::path sourcePath, std::filesystem::path targetPath)
{
    bool sourceIsDir = std::filesystem::is_directory(sourcePath);
    bool targetIsDir = std::filesystem::is_directory(targetPath);

    if(!sourceIsDir && !targetIsDir)
    {
        AssetExporter exporter = AssetExporter::Load3DModel(sourcePath, _uploadCommandPool.Internal(), _uploadQueue.Internal());
        AssetExporter::Write(exporter, targetPath);
        return;
    }
    else if(targetIsDir ^ sourceIsDir)
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "Source and target must be both directories or both files");
        return;
    }

    std::vector<std::filesystem::path> toExport;
    RecursiveModelSearch(sourcePath, toExport);

    for(const auto& path : toExport)
    {
        AssetExporter exporter = AssetExporter::Load3DModel(path, _uploadCommandPool.Internal(), _uploadQueue.Internal());
        std:std::filesystem::path target = targetPath / path.filename().replace_extension(".m3vkasset");
        AssetExporter::Write(exporter, target);
    }

}
