cmake_minimum_required(VERSION 3.28)
project(dd1367 LANGUAGES C CXX)

# Select C++ standard.
set(CMAKE_CXX_STANDARD 17)

# It is probably reasonable to assume that users not compiling
# with MSVC are compiling with GCC/Clang instead.

# Set the default compiler options for build configurations.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (MSVC)
        add_compile_options(/Zi /Od)
    else()
        add_compile_options(-g3 -O0)
    endif()
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    if (MSVC)
        add_compile_options(/O1 /O2 /Ot)
    else()
        add_compile_options(-O3)
    endif()
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
if (MSVC)
    target_compile_options(main PRIVATE /W3)
else()
    target_compile_options(main PUBLIC -Wall -Wextra)

    # Strip all symbols in release builds.
    target_link_options(main PUBLIC $<$<CONFIG:Release>:-Wl,--strip-all>)
endif()

# Link dependencies into the main executable.
target_link_libraries(main PRIVATE SDL3::SDL3 imgui)

# Because Windows likes to have its DLLs in the same directory
# as the binaries using them. The canonical way of doing this
# is using install(), but for some reason
#
# install(FILES $<TARGET_RUNTIME_DLLS:main> TYPE BIN)
#
# seems to break on Windows (the install() command has seemingly
# no effect, at least when using Visual Studio's CMake), so this
# slightly "hacky" solution is used instead.
#
# If more binaries ("runtime targets") are added in the future,
# we should probably look into more robust installation methods.
if (WIN32)
    add_custom_command(
        TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
        $<TARGET_RUNTIME_DLLS:main> $<TARGET_FILE_DIR:main>
        COMMAND_EXPAND_LISTS
    )
endif()
