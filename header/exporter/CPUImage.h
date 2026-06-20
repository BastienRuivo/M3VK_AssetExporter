#pragma once

#pragma once

#include <stb_image.h>
#include <string>
#include <vulkan/vulkan_core.h>
class CPUImage
{
    public:
    CPUImage(const std::string& path, int channelFormat);
    ~CPUImage();

    CPUImage(CPUImage&& other) noexcept;
    CPUImage& operator=(CPUImage&& other) noexcept;

    CPUImage(const CPUImage&) = delete;
    CPUImage& operator=(const CPUImage&) = delete;

    VkFormat GetGPUFormat() const;

    inline VkDeviceSize Size() const { return _width * _height* _channels; }
    inline int Width() const { return _width; };
    inline int Height() const { return _height; };
    inline int Channels() const { return _channels; };
    inline stbi_uc* Data() const { return _data; };

    private:
    int _width = 0;
    int _height = 0;
    int _channels = 0;

    stbi_uc* _data = nullptr;
};
