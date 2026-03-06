cmake_minimum_required(VERSION 3.29)
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
        add_compile_options(/O3 /Ot)
    else()
        add_compile_options(-O3)
    endif()
endif()

# External dependencies with a CMakeLists.txt (e.g. SDL).
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

set(SDLIMAGE_VENDORED ON)
set(SDLIMAGE_AVIF OFF)
set(SDLIMAGE_WEBP OFF)
add_subdirectory(external/SDL_image EXCLUDE_FROM_ALL)

set(IMGUI_SOURCE 
    external/imgui/imgui.cpp 
    external/imgui/imgui_demo.cpp 
    external/imgui/imgui_widgets.cpp 
    external/imgui/imgui_draw.cpp 
    external/imgui/imgui_tables.cpp
    external/imgui/backends/imgui_impl_sdl3.cpp
    external/imgui/backends/imgui_impl_sdlrenderer3.cpp)

# Declare the main executable.
add_executable(main src/main.cpp ${IMGUI_SOURCE})

set(COMPILE_AS_LIBRARY TRUE)
add_subdirectory(external/bc7enc_rdo)
target_include_directories(main PRIVATE external/bc7enc_rdo)

target_include_directories(main PRIVATE external/imgui)

# To avoid problems with Debug/Release in Visual Studio builds.
set_target_properties(main PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin/$<0:>)

# Show all warnings for the project's targets.
if (MSVC)
    target_compile_options(main PUBLIC /W3)
else()
    target_compile_options(main PUBLIC -Wall -Wextra)

    # Strip all symbols in release builds.
    if (CMAKE_CXX_COMPILER_LINKER_ID MATCHES "GNU")
        target_link_options(main PUBLIC $<$<CONFIG:Release>:-Wl,--strip-all>)
    # Because Apple has their own identification for LLD.
    elseif (CMAKE_CXX_COMPILER_LINKER_ID MATCHES "LLD" OR CMAKE_CXX_COMPILER_LINKER_ID MATCHES "AppleClang")
        target_link_options(main PUBLIC $<$<CONFIG:Release>:-Xlinker -s>)
    else()
        message("-- Unrecognized linker identification \"${CMAKE_CXX_COMPILER_LINKER_ID}\", not stripping binary")
        message("-- If you are using MSVC, this is expected behavior")
    endif()
endif()

# Link dependencies into the main executable.
target_link_libraries(main PRIVATE SDL3_image::SDL3_image SDL3::SDL3 bc7enc)





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
