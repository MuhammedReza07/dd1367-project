cmake_minimum_required(VERSION 3.28)
project(dd1367 LANGUAGES C CXX)

# Select C++ standard.
set (CMAKE_CXX_STANDARD 17)

# Set the default compiler options for build configurations.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g3 -O0)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3)
endif()

# External dependencies with a CMakeLists.txt (e.g. SDL).
add_subdirectory(external/sdl EXCLUDE_FROM_ALL)

# ImGui.
add_library(imgui)

# Add all files in root directory with the extensions .cpp and .h.
file(GLOB IMGUI_CXX_FILES "external/imgui/*.cpp" "external/imgui/*.h")

target_include_directories(imgui PUBLIC external/imgui external/imgui/backends)
target_sources(
    imgui
    PUBLIC
    ${IMGUI_CXX_FILES}
    external/imgui/backends/imgui_impl_sdl3.cpp
    external/imgui/backends/imgui_impl_sdlrenderer3.cpp
    external/imgui/backends/imgui_impl_sdl3.h
    external/imgui/backends/imgui_impl_sdlrenderer3.h
)

target_link_libraries(imgui PRIVATE SDL3::SDL3)

# Declare the main executable.
add_executable(main src/main.cpp)

# Show all warnings for the project's targets.
target_compile_options(main PUBLIC -Wall -Wextra)

# Strip all symbols in release builds.
target_link_options(main PUBLIC $<$<CONFIG:Release>:-Wl,--strip-all>)

# Link dependencies into the main executable.
target_link_libraries(main PRIVATE SDL3::SDL3 imgui)