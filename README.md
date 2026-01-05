# Pressure 

This Windows 10 widget lets you watch you waste your precious time procrastinating in real time with any font of your choosing!

<a href="https://github.com/woaded/pressure/releases/latest/download/Pressure.zip"><img src ="button.svg" width="300" href="https://github.com/woaded/pressure/releases/latest/download/Pressure.zip" /></a>

## Building
Dependencies:
- gcc
- make
- imagemagick
- SDL2
- SDL2_ttf

Quick run on Windows:
```bash
pacman -S mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-make \
          mingw-w64-x86_64-SDL2 \
          mingw-w64-x86_64-SDL2_ttf \
          mingw-w64-x86_64-imagemagick \
	      mingw-w64-x86_64-librsvg \
	      mingw-w64-x86_64-libwmf \
&& make clean && make
```