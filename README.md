# Who needs a README when you are a 1337 h4x0r?

Okay, but seriously... We probably should make one no matter how 1337 we may be :3

# Run
Run `./build/<build-type-lowercase>/bin/main` from the root directory to get an ImGui demo window!

# Dependencies
When building on Linux, make sure that all the dependencies of SDL are installed on your system. For more
information and installation instructions for your specific distribution, please refer to
[this page on the SDL wiki](https://wiki.libsdl.org/SDL3/README-linux).

# Build
All source code required to build the application, including the source code of external libraries such as SDL,
may be found in this repository. Building the application requires you to have CMake and a C/C++ compiler 
(e.g. GCC, Clang or MSVC) installed on your system.

## Build types
You may select a build configuration by setting the `CMAKE_BUILD_TYPE` variable when building the application,
e.g. by passing `-DCMAKE_BUILD_TYPE=<build-type>` to `cmake`. You may also use one of the presets provided in
`CMakePresets.json` using `--preset <preset-name>`, which set `CMAKE_BUILD_TYPE` to a suitable value for the
build in question (e.g. `Debug` when building with the `debug-msvc` preset).

Currently, the following build types (values of `CMAKE_BUILD_TYPE`) are available:
- `Debug`, which turns off compiler optimizations and includes debugging information in all binaries.
- `Release`, which turns compiler optimizations on and strips all binaries.

## Local build presets
The file `CMakePresets.json` contains global CMake presets for this project and is tracked by source control.
If you wish to add local presets, you may do so by creating a `CMakeUserPresets.json` (which is ignored)
and add them there.

## Linux, MacOS and other UNIX-like systems
If you have `make` installed on your system, you may build the application by running the following command
in your preferred shell from the project root directory:
```
cmake -G "Unix Makefiles" -S . -B build; cd build; make; cd ..
```
With `ninja` installed, you may also use one of the UNIX-compatible Ninja presets by instead running
```
cmake --preset <preset-name> -S .; cd build/<build-type-lowercase>; ninja; cd ../..
```

## Windows
Windows builds using the MSVC C/C++ compiler and the latest Visual Studio CMake generator 
(`Visual Studio 18 2026` with `CMAKE_GENERATOR_TOOLSET=v145`) are supported using presets for the
Visual Studio integrated development environment. There are presets for both debug and release builds
which may be selected when building in Visual Studio, where they appear as the build configurations
"Debug (MSVC)" and "Release (MSVC)" respectively.

Make sure to select `main.exe` as the startup item for the project after the CMake generation
has completed to avoid building one of the Visual Studio projects supplied by the dependencies
in `/external`. Also note that all build-related files will appear in `/build` instead of `/out`
when using one of the supplied presets.