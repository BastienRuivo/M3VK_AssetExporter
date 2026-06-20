#include "exporter/CPUImage.h"
#include "application/DebugLayer.h"
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vulkan/vulkan_core.h>


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

CPUImage::CPUImage(const std::string& path, int channelFormat)
{
#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::CREATE, "CPUImage Create !");
#endif
    _data = stbi_load(path.c_str(), &_width, &_height, &_channels, channelFormat);

    _channels = channelFormat;


    if(_data == nullptr)
    {
        const char* reason = stbi_failure_reason();
        if (reason)
        {
            DebugLayer::Log(DebugLayer::LogType::ERROR, "Failed to load texture image : " + std::filesystem::current_path().string() + "/" + path + " : " + reason);
        }

        throw std::runtime_error("Failed to load texture image");
    }
}

CPUImage::~CPUImage()
{
    stbi_image_free(_data);
    _width = _height = _channels = 0;
#ifdef M3VK_MEMORYLOG
    DebugLayer::Log(DebugLayer::LogType::DESTROY, "GPUImage Destroyed !");
#endif
}

CPUImage::CPUImage(CPUImage&& other) noexcept
{
    _width = other._width;
    _height = other._height;
    _channels = other._channels;
    _data = other._data;

    other._data = nullptr;
    _width = _height = _channels = 0;
}

CPUImage& CPUImage::operator=(CPUImage&& other) noexcept
{
    if(this != &other)
    {
        _width = other._width;
        _height = other._height;
        _channels = other._channels;
        _data = other._data;

        other._data = nullptr;
        _width = _height = _channels = 0;
    }
    return *this;
}

VkFormat CPUImage::GetGPUFormat() const
{
    switch (_channels)
    {
        case STBI_rgb_alpha: return VK_FORMAT_R8G8B8A8_SRGB;
        default: throw std::runtime_error("Unimplemented Color Format");
    }
}
