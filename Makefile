CXX      := g++
WINDRES  := windres
CONVERT  := magick

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf)
SDL_LIBS   := $(shell pkg-config --static --libs sdl2 SDL2_ttf)

CXXFLAGS   := -Os -flto -ffunction-sections -fdata-sections \
              -fno-exceptions -fno-rtti $(SDL_CFLAGS)

LDFLAGS    := -Wl,--gc-sections -flto -s -static -static-libgcc -static-libstdc++ -mwindows

LIBS       := $(SDL_LIBS) -luser32 -lgdi32 -lwinmm -limm32 -lole32 \
              -loleaut32 -lshell32 -lsetupapi -lversion -luuid \
              -lrpcrt4 -ldwrite -ldwmapi -luxtheme

BUILD_DIR  := build
GEN_DIR    := generated
TARGET     := $(BUILD_DIR)/Pressure.exe
SRC        := main.cpp
SVG_ICON   := icon.svg
ICO_FILE   := $(GEN_DIR)/icon.ico
RES_OBJ    := $(GEN_DIR)/resource.res

.PHONY: all clean

all: $(TARGET)

$(ICO_FILE): $(SVG_ICON)
	@mkdir -p $(GEN_DIR)
	$(CONVERT) -background none $(SVG_ICON) -define icon:auto-resize=16,32,48,64,128,256 $(ICO_FILE)

$(RES_OBJ): resource.rc $(ICO_FILE)
	@mkdir -p $(GEN_DIR)
	$(WINDRES) -I$(GEN_DIR) -I. resource.rc -O coff -o $(RES_OBJ)

$(TARGET): $(SRC) $(RES_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(SRC) $(RES_OBJ) -o $(TARGET) $(CXXFLAGS) $(LDFLAGS) $(LIBS)
	@cp INTER-LICENSE.txt $(BUILD_DIR)/ 2>/dev/null || true
	@cp font.ttf $(BUILD_DIR)/ 2>/dev/null || true

clean:
	rm -f $(TARGET)
	rm -rf $(GEN_DIR)
