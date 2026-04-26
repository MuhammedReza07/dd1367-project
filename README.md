# Who needs a README when you are a 1337 h4x0r?

Okay, but seriously... We probably should make one no matter how 1337 we may be :3

# Run
Run `./build/<build-type-lowercase>/bin/main` from the root directory to get an ImGui demo window!

# Dependencies
When building on Linux, make sure that all the dependencies of SDL are installed on your system. For more
information and installation instructions for your specific distribution, please refer to
[the SDL wiki](https://wiki.libsdl.org/SDL3/README-linux).

# Build
Building the application requires you to have CMake (version >= 3.29) and a C/C++ compiler (e.g. GCC, Clang or MSVC) 
installed on your system.

## Updating submodules
This repository uses Git submodules to manage dependencies. To avoid making the `.git` folder bloated with gigabytes 
worth of history for the external libraries, most submodules are set to be "shallow" per default. To initiate the 
submodules after cloning the repository, run the following commands in the project root: 
```
git submodule init
git submodule update --recursive
```
Or, if you (for some reason) do not want shallow submodules and instead want the full history (and potentially unused
nested submodules), you can instead run the following: 
```
git submodule update --init --recursive
```
Do note that the size of the project folder will be more than three times as large using this method, so if you care about
your storage at all and do not have a good reason to, we recommend using the first set of commands, especially since most
of the extra data is pretty much useless for the average developer. 

In the majority of cases, any of these options will be enough to start development with all necessary external libraries. 

However, if you wish to pull upstream changes from the submodules, you will have to manually switch branch or checkout the
submodule, since they per default are in a "detatched HEAD"-state. To do this, simply navigate to the submodule you wish to 
update and run `git checkout <name_of_desired_branch>` and then run `git pull`. For example, lets say you are in the project
root and wish to pull upstream changes from the library-branch of bc7enc\_rdo. To do that, you would do the following (after
already running the commands above):
```
cd external
cd bc7enc_rdo
git checkout library
git pull
```
If Git complains about local changes, either `git stash` them, or (if you do not want to keep your changes) use `git checkout 
library --force`.

If you are unfamiliar with how Git submodules work, you should know that pulling upstream changes inside a submodule will count
as a modified file in the project repository. If you were to run `git diff` in the project root after modifying a submodule, it
will look something like this: 
```diff
-Subproject commit 4a35aba8bffa67f250537479cc33892f3c224e8f
+Subproject commit 0561eb8d49edcd55560e9d7bca79ac11cf4209df
```
This means that the submodule you modified has changed from pointing to the commit 4a35... to now instead point to commit 0561...
This modification can be committed pushed to origin just like any other modification, but if you do not know what you are doing,
or if you did not intend to modify the submodule, you should not push those changes before figuring out what has changed.

If you receive errors such as `fatal: remote error: upload-pack: not our ref` after running `git submodule update --recursive`
you may have to run the `scripts/auto-git-setup.sh` in the project root. This script ensures that the submodules are checked out 
to specific, existing and stable releases of the library. On UNIX-like systems, this can be done by typing 
`sh scripts/auto-git-setup.sh` in the project root. If the errors *still* persist, the specified commits to our
submodules have most likely been deleted via a rebase or force-push, and you will have to find your own commit from the libraries
GitHub that you want to add as a submodule. 

If you do decide to run the script, you are of course first advised to **look at the script yourself** before blindly following a random guide 
on the internet, both for your own safety and so that you know what is going on behind the scenes.
If you are using Windows, you will have to use something like Git Bash or WSL in order to run the shell-script. 

Note that this script assumes you to already have set up your Git SSH keys properly.

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
Windows builds using the MSVC C/C++ compiler and the Visual Studio CMake generator 
(`Visual Studio 18 2026` with `CMAKE_GENERATOR_TOOLSET=v145`) are supported using presets for the
Visual Studio integrated development environment. There are presets for both debug and release builds
which may be selected when building in Visual Studio, where they appear as the build configurations
"Debug (MSVC)" and "Release (MSVC)" respectively.

Make sure to select `main.exe` as the startup item for the project after the CMake generation
has completed to avoid building one of the Visual Studio projects supplied by the dependencies
in `/external`. Also note that all build-related files will appear in `/build` instead of `/out`
when using one of the supplied presets.
