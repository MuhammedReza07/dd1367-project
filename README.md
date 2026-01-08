# Who needs a README when you are a 1337 h4x0r?

Okay, but seriously... We probably should make one no matter how 1337 we may be :3

# Run
Run `/build/main` from the root directory to get a window!

# Build
All source code required to build the application, including the source code of external libraries such as SDL,
may be found in this repository. Building the application requires the Clang C/C++ toolchain of the target platform
and CMake being installed on your system.

## Using a C/C++ integrated development environment (IDE)
If you are using an IDE with built-in support for building and running C/C++ programs with CMake,
you may need to modify the CMake flags it uses to e.g. ensure that building works as expected.
To ensure that, among other things, Clang is used to compile the code you may use the following flags:
```
-G <your-generator-of-choice> -S . -B build -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
```
Execute `cmake --help` if you want to know which generators are supported on your platform. Make sure
to select a generator that is compatible with your IDE.

## Linux, MacOS, and other UNIX-like systems
Run the following in your preferred shell from the root directory :3
```
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++; cd build; make; cd ..
```

## Windows
Good luck <3
