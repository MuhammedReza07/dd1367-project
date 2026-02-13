#!/bin/sh

# Should work on both Linux and MacOS.
cmake --preset debug-ninja-ci -S .; cd build; ninja; cd ..