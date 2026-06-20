cmake_minimum_required(VERSION 3.14)
project(M3VK_AssetExporter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

enable_language(ISPC)
find_program(ISPC_BIN ispc REQUIRED)

set(ISPC_WRAPPER "${CMAKE_BINARY_DIR}/ispc_wrapper.sh")
file(WRITE ${ISPC_WRAPPER} "#!/bin/sh\n${ISPC_BIN} --pic \"$@\"\n")
execute_process(COMMAND chmod +x ${ISPC_WRAPPER})

set(ISPC_COMPILER_LOCATION "${ISPC_WRAPPER}" CACHE PATH "Path to ISPC wrapper" FORCE)

set(ISPC_COMPILER_TARGET "avx2" CACHE STRING "Target ISA" FORCE)
set(CMAKE_ISPC_COMPILER_TARGET "avx2" CACHE STRING "Target ISA" FORCE)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- Dependencies ---

# STB
fetchcontent_declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)

# GLM
fetchcontent_declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
)

fetchcontent_declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0 # Use the specific version tag you require
)

set(ASSIMP_BUILD_ZLIB ON CACHE INTERNAL "")
set(ASSIMP_FORCE_INTERNAL_ZLIB ON CACHE INTERNAL "")
set(ASSIMP_BUILD_MINIZIP ON CACHE INTERNAL "")
set(CMAKE_POLICY_DEFAULT_CMP0074 NEW) # Respect ZLIB_ROOT if set

set(ASSIMP_BUILD_TESTS OFF CACHE INTERNAL "")
set(ASSIMP_INSTALL OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_SAMPLES OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_CACHED_INTERFACES OFF CACHE INTERNAL "")

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE INTERNAL "")
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE INTERNAL "")
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE INTERNAL "")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE INTERNAL "")

fetchcontent_declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v6.0.4 # Use a stable tagged release
)

fetchcontent_declare(
    ispc_texcomp_repo
    GIT_REPOSITORY https://github.com/BeardyKing/ispc_texture_compressor_cmake.git
    GIT_TAG        master
)

fetchcontent_makeavailable(stb glm assimp ispc_texcomp_repo json)

# --- Interface Library Setup ---

# stb
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

# GLM (Note: GLM's own CMake might provide a target, but this ensures manual control)
add_library(glm_lib INTERFACE)
target_include_directories(glm_lib INTERFACE ${glm_SOURCE_DIR})

# --- Vulkan and Shaders ---

find_package(Vulkan REQUIRED)

find_program(GLSLC_EXECUTABLE NAMES glslc HINTS "$ENV{VULKAN_SDK}/bin")
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found! Please ensure the Vulkan SDK is installed.")
endif()

# --- Executable ---

file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE HEADER_FILES "${CMAKE_SOURCE_DIR}/header/*.h" "${CMAKE_SOURCE_DIR}/header/*.hpp")

# define data source directory
add_definitions(-DDATA_SOURCE_DIR="data/")

add_executable(${PROJECT_NAME} ${SRC_FILES} ${HEADER_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/header
    ${CMAKE_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
)

# Link the new libraries here
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    stb
    glm::glm
    assimp::assimp
    ispc_texture_compressor
    nlohmann_json::nlohmann_json
)
