#!/bin/bash

BINARY="Pressure-Linux"

if [ -f "build/$BINARY" ]; then
    cd build
    chmod +x "$BINARY"
    ./"$BINARY"
else
    echo "Binary not found, building"
    chmod +x build.sh
    ./build.sh
    if [ $? -eq 0 ]; then
        echo "Build successful"
        echo "Launching"
        cd build
        chmod +x "$BINARY"
        ./"$BINARY"
    else
        echo "Launch failed"
        exit 1
    fi
fi