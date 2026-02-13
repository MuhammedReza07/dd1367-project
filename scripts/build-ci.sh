#!/bin/sh

cmake --preset debug-ninja-ci -S .; cd build; ninja; cd ..
