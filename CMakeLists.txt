cmake_minimum_required(VERSION 3.28)
project(dd1367 LANGUAGES C CXX)

# TODO: Only enable these flags in debug builds.
add_compile_options(-g -O0)

# External dependencies with a CMakeLists.txt (e.g. SDL).
add_subdirectory(external/sdl EXCLUDE_FROM_ALL)

# ImGUI.
add_library(imgui)

# Add all files in root directory with the extensions .cpp and .h.
file(GLOB IMGUI_CXX_FILES "external/imgui/*.cpp" "external/imgui/*.h")

target_include_directories(imgui PUBLIC external/imgui external/imgui/backends)
target_sources(
    imgui
    PUBLIC
    ${IMGUI_CXX_FILES}
    external/imgui/backends/imgui_impl_sdl3.cpp
    external/imgui/backends/imgui_impl_sdlgpu3.cpp
    external/imgui/backends/imgui_impl_sdl3.h
    external/imgui/backends/imgui_impl_sdlgpu3.h
)

target_link_libraries(imgui PRIVATE SDL3::SDL3)

# Declare the main executable.
add_executable(main src/main.cpp)

# TODO: Only enable these flags in debug builds.
target_compile_options(main PUBLIC -g -O0 -Wall -Wextra)

# Link dependencies into the main executable.
target_link_libraries(main PRIVATE SDL3::SDL3 imgui)