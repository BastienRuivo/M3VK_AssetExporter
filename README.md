# M3Vk Asset Exporter

This is a simple asset exporter, turning 3D models loaded by assimp into my own format to speed up loading in my other project

It does the following
- Load asset with assimp
  - Load textures with stbi or tinyddsloader
    - If not block compressed, compress everything in BC format
  - Write textures
  - Write vertices
- Write asset on disk as a .m3vkasset
