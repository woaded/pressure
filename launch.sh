#!/bin/bash

BINARY="build/Pressure-Linux"

if [ -f "$BINARY" ]; then
    chmod +x "$BINARY"
    ./"$BINARY"
else
    echo "Binary not found, building"
    chmod +x Build.sh
    ./Build.sh
    if [ $? -eq 0 ]; then
        echo "Build successful"
        echo "Launching"
        chmod +x "$BINARY"
        ./"$BINARY"
    else
        echo "Launch failed"
        exit 1
    fi
fi