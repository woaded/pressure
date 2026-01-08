CC        := gcc
WINDRES   := windres
CONVERT   := magick

ifeq ($(OS),Windows_NT)
    PLATFORM_OS := windows
    TARGET      := ../build/Pressure-Win64.exe
else
    PLATFORM_OS := linux
    TARGET      := ../build/Pressure-Linux
endif

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf)
CFLAGS     := -Os -flto -ffunction-sections -fdata-sections -std=c11 $(SDL_CFLAGS)
BUILD_DIR  := ../build
GEN_DIR    := ../generated
ASSET_DIR  := ../assets

ifeq ($(PLATFORM_OS),windows)
    LIBS    := $(shell pkg-config --static --libs sdl2 SDL2_ttf) -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lrpcrt4 -ldwrite -ldwmapi -luxtheme -lstdc++
    CFLAGS  += -D_WIN32_WINNT=0x0600
    LDFLAGS := -Wl,--gc-sections -flto -s -static -static-libgcc -mwindows
    RES_OBJ := $(GEN_DIR)/resource.res
else
    SDL_STATIC_LIBS := $(shell pkg-config --static --libs sdl2 SDL2_ttf)
    LDFLAGS  := -Wl,--gc-sections -flto -s -static-libgcc -static-libstdc++
    LIBS     := -Wl,-Bstatic -lSDL2_ttf -lSDL2 -Wl,-Bdynamic $(SDL_STATIC_LIBS) -lwayland-client -lwayland-cursor -lwayland-egl -lxkbcommon -lpthread -lm -ldl
    RES_OBJ  :=
endif

ICO_LIGHT  := $(GEN_DIR)/icon_light.ico
ICO_DARK   := $(GEN_DIR)/icon_dark.ico

.PHONY: all clean

all: $(TARGET)

$(ICO_LIGHT): $(ASSET_DIR)/icon_light.svg
	@mkdir -p $(GEN_DIR)
	$(CONVERT) -background none $< -gravity center -extent 256x256 -define icon:auto-resize=16,32,48,64,128,256 $@

$(ICO_DARK): $(ASSET_DIR)/icon_dark.svg
	@mkdir -p $(GEN_DIR)
	$(CONVERT) -background none $< -gravity center -extent 256x256 -define icon:auto-resize=16,32,48,64,128,256 $@

$(RES_OBJ): resource.rc $(ICO_LIGHT) $(ICO_DARK)
	@mkdir -p $(GEN_DIR)
	$(WINDRES) -I$(GEN_DIR) -I. resource.rc -O coff -o $(RES_OBJ)

$(TARGET): main.c $(RES_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CC) main.c $(RES_OBJ) -o $(TARGET) $(CFLAGS) $(LDFLAGS) $(LIBS)
	@cp $(ASSET_DIR)/defaultfont/font.ttf $(BUILD_DIR)/ 2>/dev/null || true

clean:
	rm -f $(BUILD_DIR)/Pressure-Win64.exe $(BUILD_DIR)/Pressure-Linux
	rm -rf $(GEN_DIR)