CXX      := g++
WINDRES  := windres
CONVERT  := magick

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf)
SDL_LIBS   := $(shell pkg-config --static --libs sdl2 SDL2_ttf)

CXXFLAGS   := -Os -flto -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -D_WIN32_WINNT=0x0600 $(SDL_CFLAGS)
LDFLAGS    := -Wl,--gc-sections -flto -s -static -static-libgcc -static-libstdc++ -mwindows
LIBS       := $(SDL_LIBS) -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lrpcrt4 -ldwrite -ldwmapi -luxtheme

BUILD_DIR  := ../build
GEN_DIR    := ../generated
ASSET_DIR  := ../assets
TARGET     := $(BUILD_DIR)/Pressure.exe

SVG_LIGHT  := $(ASSET_DIR)/icon_light.svg
SVG_DARK   := $(ASSET_DIR)/icon_dark.svg
ICO_LIGHT  := $(GEN_DIR)/icon_light.ico
ICO_DARK   := $(GEN_DIR)/icon_dark.ico
RES_OBJ    := $(GEN_DIR)/resource.res

.PHONY: all clean

all: $(TARGET)

$(ICO_LIGHT): $(SVG_LIGHT)
	@mkdir -p $(GEN_DIR)
	$(CONVERT) -background none $(SVG_LIGHT) -gravity center -extent 256x256 -define icon:auto-resize=16,32,48,64,128,256 $(ICO_LIGHT)

$(ICO_DARK): $(SVG_DARK)
	@mkdir -p $(GEN_DIR)
	$(CONVERT) -background none $(SVG_DARK) -gravity center -extent 256x256 -define icon:auto-resize=16,32,48,64,128,256 $(ICO_DARK)

$(RES_OBJ): resource.rc $(ICO_LIGHT) $(ICO_DARK)
	@mkdir -p $(GEN_DIR)
	$(WINDRES) -I$(GEN_DIR) -I. resource.rc -O coff -o $(RES_OBJ)

$(TARGET): main.cpp $(RES_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CXX) main.cpp $(RES_OBJ) -o $(TARGET) $(CXXFLAGS) $(LDFLAGS) $(LIBS)
	@cp ../LICENSE $(BUILD_DIR)/PRESSURE-LICENSE.txt 2>/dev/null || true
	@cp $(ASSET_DIR)/inter/INTER-LICENSE.txt $(BUILD_DIR)/ 2>/dev/null || true
	@cp $(ASSET_DIR)/inter/font.ttf $(BUILD_DIR)/ 2>/dev/null || true

clean:
	rm -f $(TARGET)
	rm -rf $(GEN_DIR)