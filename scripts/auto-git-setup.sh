#!/bin/sh

echo "Checking out submodules to safe releases..."
cd external
cd SDL
git fetch origin tag release-3.4.2 --no-tags
git checkout tags/release-3.4.2 --force
cd ..
cd SDL_image
git fetch origin tag release-3.4.0 --no-tags
git checkout tags/release-3.4.0 --force
cd ..
cd bc7enc_rdo
git fetch
git checkout library
cd ..
cd imgui
git fetch origin tag v1.92.5-docking --no-tags
git checkout tags/v1.92.5-docking --force
cd ..
cd imgui-node-editor
git fetch
git checkout develop
cd ..
cd ..
echo "Finished checking out submodules."

