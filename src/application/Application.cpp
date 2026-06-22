#include "application/Application.h"
#include "application/DebugLayer.h"
#include "exporter/AssetExporter.h"
#include "exporter/MaterialExporter.h"
#include <filesystem>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <glm/trigonometric.hpp>
#include <sys/types.h>

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

void RecursiveFileSearch(std::filesystem::path path, std::vector<std::filesystem::path>& toExport, const std::vector<std::string>& extensions)
{
    if(std::filesystem::is_directory(path))
    {
        for(const auto& entry : std::filesystem::directory_iterator(path))
        {
            RecursiveFileSearch(entry.path(), toExport, extensions);
        }
    }
    else if(std::filesystem::is_regular_file(path))
    {
        for(const auto& extension : extensions)
        {
            if(path.extension() == extension)
            {
                toExport.push_back(path);
                return;
            }
        }
    }
}

void Application::ExportFile(u_char mode, std::filesystem::path sourcePath, std::filesystem::path targetPath)
{
    bool sourceIsDir = std::filesystem::is_directory(sourcePath);
    bool targetIsDir = std::filesystem::is_directory(targetPath);

    if(!sourceIsDir && !targetIsDir)
    {
        AssetExporter exporter = AssetExporter::Load3DModel(sourcePath, _uploadCommandPool.Internal(), _uploadQueue.Internal());
        exporter.Write(targetPath);
        return;
    }
    else if(targetIsDir ^ sourceIsDir)
    {
        DebugLayer::Log(DebugLayer::LogType::WARNING, "Source and target must be both directories or both files");
        return;
    }

    std::vector<std::filesystem::path> toExport;
    if(mode == 'a')
    {
        RecursiveFileSearch(sourcePath, toExport, std::vector<std::string>({".obj", ".fbx", ".gltf"}));
    }
    else if(mode == 'm')
    {
        RecursiveFileSearch(sourcePath, toExport, std::vector<std::string>({".json"}));
    }

    for(const auto& path : toExport)
    {
        if(mode == 'a')
        {
            AssetExporter exporter = AssetExporter::Load3DModel(path, _uploadCommandPool.Internal(), _uploadQueue.Internal());
            std:std::filesystem::path target = targetPath / path.filename().replace_extension(".m3vkasset");
            exporter.Write(target);
        }
        else if(mode == 'm')
        {
            MaterialExporter exporter = MaterialExporter::LoadMaterial(path, _uploadCommandPool.Internal(), _uploadQueue.Internal());
            std::filesystem::path target = targetPath / path.filename().replace_extension(".m3vkmaterial");
            exporter.Write(target);
        }
    }

}
