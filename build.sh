#!/bin/bash

echo "Checking dependencies"

if command -v apt-get &> /dev/null; then
    PKG_MGR="apt"
    DEPS="libsdl2-dev libsdl2-ttf-dev libasound2-dev libpulse-dev libx11-dev libxext-dev libxcursor-dev libxinerama-dev libxi-dev libxrandr-dev libxss-dev libwayland-dev libxkbcommon-dev libdrm-dev libgbm-dev libfreetype-dev libbz2-dev libpng-dev libbrotli-dev libdecor-0-dev libharfbuzz-dev"
elif command -v dnf &> /dev/null; then
    PKG_MGR="dnf"
    DEPS="SDL2-devel SDL2_ttf-devel alsa-lib-devel pulseaudio-libs-devel libX11-devel libXext-devel libXcursor-devel libXinerama-devel libXi-devel libXrandr-devel libXScrnSaver-devel wayland-devel libxkbcommon-devel libdrm-devel mesa-libgbm-devel freetype-devel bzip2-devel libpng-devel brotli-devel libdecor-devel harfbuzz-devel"
elif command -v pacman &> /dev/null; then
    PKG_MGR="pacman"
    DEPS="sdl2 sdl2_ttf alsa-lib libpulse libx11 libxext libxcursor libxinerama libxi libxrandr libxss wayland libxkbcommon libdrm gbm freetype2 bzip2 libpng brotli libdecor harfbuzz"
else
    PKG_MGR="unknown"
    DEPS="sdl2 sdl2_ttf"
fi

if ! pkg-config --exists sdl2 SDL2_ttf; then
    echo "Missing dependencies for build."
    if [ "$PKG_MGR" != "unknown" ]; then
        echo "Run: sudo $PKG_MGR install $DEPS"
    fi
    exit 1
fi

echo "Starting build"
cd src && make clean && make

if [ $? -eq 0 ]; then
    echo "Build complete"
    cd ..
else
    echo "Build failed"
    exit 1
fi