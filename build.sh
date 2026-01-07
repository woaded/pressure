#!/bin/bash

echo "Checking dependencies"
DEPENDENCIES=(sdl2 SDL2_ttf)
for dep in "${DEPENDENCIES[@]}"; do
    if ! pkg-config --exists "$dep"; then
        echo "Dependency $dep not found"
        exit 1
    fi
done

echo "Starting build"
cd src && make clean && make

if [ $? -eq 0 ]; then
    echo "Build complete"
    cd ..
else
    echo "Build failed"
    exit 1
fi