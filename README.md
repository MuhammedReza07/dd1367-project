# Who needs a README when you are a 1337 h4x0r?

Okay, but seriously... We probably should make one no matter how 1337 we may be :3

# Build
Something, something, CMake.

## Linux, MacOS, and other UNIX-like systems
Run the following in your preferred shell from the root directory :3
```
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++; cd build; make
```
Make sure that you have the Clang C/C++ toolchain for your platform of choice installed
before running, since you need Clang to compile using Clang.

## Windows
Good luck <3
