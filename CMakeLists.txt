cmake_minimum_required(VERSION 3.28)
project(dd1367 LANGUAGES C CXX)

# External dependencies (e.g. SDL, ImGUI).
add_subdirectory(external/sdl EXCLUDE_FROM_ALL)

# Declare the main executable.
add_executable(main src/main.cpp)

# Link dependencies into the main executable.
target_link_libraries(main PRIVATE SDL3::SDL3)
