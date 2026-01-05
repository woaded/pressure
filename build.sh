#!/bin/bash
mkdir -p build generated && \
windres resource.rc -O coff -o generated/resource.res && \
 g++ main.cpp generated/resource.res -o build/Pressure.exe -I/mingw64/include/SDL2 -Os -s -ffunction-sections -fdata-sections -Wl,--gc-sections -static -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lfreetype -lharfbuzz -lgraphite2 -lpng -lbz2 -lz -lbrotlidec -lbrotlicommon -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lrpcrt4 -ldwrite -ldwmapi -luxtheme -mwindows