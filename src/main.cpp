#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_syswm.h>
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include <fstream>
#include <direct.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_RESIZE 1002
#define ID_TRAY_INVERT 1003
#define IDI_ICON_LIGHT 101
#define IDI_ICON_DARK 102

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

enum PreferredAppMode { Default, AllowDark, ForceDark, ForceLight, Max };
typedef PreferredAppMode(WINAPI* TSetPreferredAppMode)(PreferredAppMode appMode);

struct CachedGlyph {
    SDL_Texture* texMain;
    SDL_Texture* texOutline;
    int w, h;
};

NOTIFYICONDATA nid = { 0 };
bool isLocked = true, isResizing = false, isInverted = false, hasColon = true;
float currentAlpha = 0.1f;
int hoverTicks = 0, winW = 320, winH = 100, fontSize = 52;
const float ASPECT_RATIO = 100.0f / 320.0f;

int hoverTickThreshold = 100; 
float fadeIncrement = 0.05f;
float passiveAlpha = 0.1f;

std::map<char, CachedGlyph> glyphAtlas;
std::vector<unsigned char> fontBuffer;
HWND previousFocus = NULL;

bool IsWindowsLightMode() {
    HKEY hKey;
    DWORD value = 0, size = sizeof(value);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "AppsUseLightTheme", NULL, NULL, (LPBYTE)&value, &size);
        RegCloseKey(hKey);
    }
    return value != 0;
}

void UpdateIcons(HWND hwnd) {
    HINSTANCE hInst = GetModuleHandle(NULL);
    HICON hNewIcon = LoadIcon(hInst, MAKEINTRESOURCE(isInverted ? IDI_ICON_DARK : IDI_ICON_LIGHT));
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hNewIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hNewIcon);
    nid.hIcon = hNewIcon;
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void LoadConfiguration() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string fullPath = path;
    size_t lastSlash = fullPath.find_last_of("\\/");
    std::string configPath = fullPath.substr(0, lastSlash) + "\\config.ini";

    std::ifstream infile(configPath);
    if (!infile.good()) {
        std::ofstream outfile(configPath);
        outfile << "[Config]" << std::endl;
        outfile << "; Seconds required for the mouse to hover on the clock to make it clickable." << std::endl;
        outfile << "HoverTickThreshold=1" << std::endl;
        outfile << "; Clock fade speed" << std::endl;
        outfile << "FadeIncrement=0.05f" << std::endl;
        outfile << "; Transparency of the clock while passive" << std::endl;
        outfile << "PassiveAlpha=0.1f" << std::endl;
        outfile.close();
    }

    hoverTickThreshold = GetPrivateProfileIntA("Config", "HoverTickThreshold", 1, configPath.c_str()) * 100;
    char buf[32];


    GetPrivateProfileStringA("Config", "FadeIncrement", "0.05f", buf, sizeof(buf), configPath.c_str());
    std::string fs = buf; fs.erase(std::remove(fs.begin(), fs.end(), 'f'), fs.end());
    fadeIncrement = (float)atof(fs.c_str());

    GetPrivateProfileStringA("Config", "PassiveAlpha", "0.1f", buf, sizeof(buf), configPath.c_str());
    std::string as = buf; as.erase(std::remove(as.begin(), as.end(), 'f'), as.end());
    passiveAlpha = std::clamp((float)atof(as.c_str()), 0.0f, 1.0f);
    currentAlpha = passiveAlpha;
}

TTF_Font* LoadFontToRAM(const std::string& path, int size) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return nullptr;
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    fontBuffer.resize(fileSize);
    if (file.read((char*)fontBuffer.data(), fileSize)) {
        SDL_RWops* rw = SDL_RWFromMem(fontBuffer.data(), (int)fileSize);
        return TTF_OpenFontRW(rw, 1, size);
    }
    return nullptr;
}

void ApplyDarkTheme(HWND hwnd, bool dark) {
    BOOL useDarkMode = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
    HMODULE hUxtheme = LoadLibraryA("uxtheme.dll");
    if (hUxtheme) {
        TSetPreferredAppMode pSetPreferredAppMode = (TSetPreferredAppMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
        if (pSetPreferredAppMode) pSetPreferredAppMode(dark ? ForceDark : ForceLight);
        FreeLibrary(hUxtheme);
    }
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void ApplyVerticalGradient(SDL_Surface* surface, SDL_Color top, SDL_Color bottom) {
    if (!surface) return;
    SDL_LockSurface(surface);
    Uint32* pixels = (Uint32*)surface->pixels;
    for (int y = 0; y < surface->h; y++) {
        float factor = (float)y / (float)surface->h;
        Uint8 r = (Uint8)(top.r + factor * (bottom.r - top.r));
        Uint8 g = (Uint8)(top.g + factor * (bottom.g - top.g));
        Uint8 b = (Uint8)(top.b + factor * (bottom.b - top.b));
        for (int x = 0; x < surface->w; x++) {
            Uint32* pixel = &pixels[y * (surface->pitch / 4) + x];
            Uint8 a = (*pixel >> 24) & 0xFF;
            if (a > 0) {
                *pixel = (a << 24) | (((r * a) / 255) << 16) | (((g * a) / 255) << 8) | ((b * a) / 255);
            }
        }
    }
    SDL_UnlockSurface(surface);
}

void ClearAtlas() {
    for (auto const& [c, g] : glyphAtlas) {
        if (g.texMain) SDL_DestroyTexture(g.texMain);
        if (g.texOutline) SDL_DestroyTexture(g.texOutline);
    }
    glyphAtlas.clear();
}

void BuildAtlas(SDL_Renderer* rr, TTF_Font* font) {
    ClearAtlas();
    if (!font) return;
    hasColon = TTF_GlyphIsProvided(font, ':');
    std::string chars = "0123456789 "; if (hasColon) chars += ":";
    SDL_Color textColor = isInverted ? SDL_Color{23,23,23,255} : SDL_Color{255,255,255,255};
    SDL_Color gradColor = isInverted ? SDL_Color{12,12,12,255} : SDL_Color{208,208,208,255};
    SDL_Color outColor  = isInverted ? SDL_Color{240,240,240,255} : SDL_Color{10,10,10,255};
    for (char c : chars) {
        SDL_Surface* sm = TTF_RenderGlyph_Blended(font, (Uint16)c, { 255, 255, 255, 255 });
        SDL_Surface* so = TTF_RenderGlyph_Blended(font, (Uint16)c, outColor);
        if (sm && so) {
            ApplyVerticalGradient(sm, textColor, gradColor);
            glyphAtlas[c] = { SDL_CreateTextureFromSurface(rr, sm), SDL_CreateTextureFromSurface(rr, so), sm->w, sm->h };
            SDL_FreeSurface(sm); SDL_FreeSurface(so);
        }
    }
}

std::string GetCountdownString() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    struct tm lt; localtime_s(&lt, &tt);
    int d_to_sat = (6 - lt.tm_wday + 7) % 7;
    if (d_to_sat == 0 && (lt.tm_hour > 0 || lt.tm_min > 0 || lt.tm_sec > 0)) d_to_sat = 7;
    struct tm tgt = lt; tgt.tm_mday += d_to_sat; tgt.tm_hour = 0; tgt.tm_min = 0; tgt.tm_sec = 0; tgt.tm_isdst = -1;
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::from_time_t(std::mktime(&tgt)) - now);
    long long s = std::max(0LL, (long long)diff.count());
    int d = (int)(s / 86400); s %= 86400;
    int h = (int)(s / 3600); s %= 3600;
    int m = (int)(s / 60); s %= 60;
    char buf[24]; snprintf(buf, sizeof(buf), "%01d:%02d:%02d:%02d", d, h, m, (int)s);
    std::string res(buf); if (!hasColon) std::replace(res.begin(), res.end(), ':', ' ');
    return res;
}

#undef main
int main(int argc, char* argv[]) {
    LoadConfiguration();
    SDL_Init(SDL_INIT_VIDEO); TTF_Init();
    
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string fontPath = std::string(exePath).substr(0, std::string(exePath).find_last_of("\\/")) + "\\font.ttf";

    TTF_Font* font = LoadFontToRAM(fontPath, fontSize);
    if (!font) {
        TTF_Quit(); SDL_Quit();
        MessageBoxA(NULL, ("Missing: " + fontPath).c_str(), "Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    isInverted = IsWindowsLightMode();
    SDL_Window* window = SDL_CreateWindow("Pressure", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH, SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_HIDDEN);
    SDL_SysWMinfo wmInfo; SDL_VERSION(&wmInfo.version); SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    ApplyDarkTheme(hwnd, !isInverted);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TOOLWINDOW);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    BuildAtlas(renderer, font);
    SDL_ShowWindow(window);

    nid.cbSize = sizeof(NOTIFYICONDATA); nid.hWnd = hwnd; nid.uID = 1; nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON; strcpy_s(nid.szTip, "Pressure");
    UpdateIcons(hwnd);
    Shell_NotifyIcon(NIM_ADD, &nid);

    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_RESIZE, "Resize Mode");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_INVERT, "Invert Colors");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, "Quit");

    bool running = true, dragging = false;
    int mX = 0, mY = 0;
    SDL_Event ev; std::string lastStr = "";
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

    while (running) {
        bool needsRedraw = false;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_SYSWMEVENT) {
                auto& winMsg = ev.syswm.msg->msg.win;
                if (winMsg.msg == WM_TRAYICON && LOWORD(winMsg.lParam) == WM_RBUTTONUP) {
                    POINT p; GetCursorPos(&p); SetForegroundWindow(hwnd);
                    TrackPopupMenu(hMenu, TPM_LEFTALIGN, p.x, p.y, 0, hwnd, NULL);
                }
                if (winMsg.msg == WM_SETTINGCHANGE && winMsg.lParam && wcscmp((LPCWSTR)winMsg.lParam, L"ImmersiveColorSet") == 0) {
                    bool newMode = IsWindowsLightMode();
                    if (newMode != isInverted) {
                        isInverted = newMode;
                        ApplyDarkTheme(hwnd, !isInverted);
                        BuildAtlas(renderer, font);
                        UpdateIcons(hwnd);
                        needsRedraw = true;
                    }
                }
            }
            if (!isResizing && !isLocked) {
                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    if (ev.button.button == SDL_BUTTON_LEFT) { dragging = true; SDL_GetMouseState(&mX, &mY); }
                    else if (ev.button.button == SDL_BUTTON_RIGHT) {
                        POINT p; GetCursorPos(&p);
                        int id = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, hwnd, NULL);
                        if (id == ID_TRAY_EXIT) running = false;
                        if (id == ID_TRAY_RESIZE) isResizing = true;
                        if (id == ID_TRAY_INVERT) { isInverted = !isInverted; ApplyDarkTheme(hwnd, !isInverted); BuildAtlas(renderer, font); UpdateIcons(hwnd); needsRedraw = true; }
                    }
                }
                if (ev.type == SDL_MOUSEBUTTONUP) dragging = false;
            }
        }

        if (isResizing) {
            if (GetAsyncKeyState(VK_LBUTTON) & 0x8000 || GetAsyncKeyState(VK_RETURN) & 0x8000) {
                isResizing = false; fontSize = (int)(52 * (winW / 320.0f));
                TTF_CloseFont(font); font = LoadFontToRAM(fontPath, fontSize);
                BuildAtlas(renderer, font); needsRedraw = true;
            } else {
                int gx, gy, wx, wy; SDL_GetGlobalMouseState(&gx, &gy); SDL_GetWindowPosition(window, &wx, &wy);
                int newH = std::clamp(std::abs(gy - wy), 40, 600);
                winW = (int)(newH / ASPECT_RATIO); winH = newH; SDL_SetWindowSize(window, winW, winH);
                needsRedraw = true;
            }
        } else if (dragging) {
            int gx, gy; SDL_GetGlobalMouseState(&gx, &gy); SDL_SetWindowPosition(window, gx - mX, gy - mY);
            needsRedraw = true;
        }

        int winX, winY, mx, my; SDL_GetWindowPosition(window, &winX, &winY); SDL_GetGlobalMouseState(&mx, &my);
        bool isInside = (mx >= winX && mx <= winX + winW && my >= winY && my <= winY + winH);
        if (isInside || isResizing) {
            if (isLocked && !isResizing && ++hoverTicks >= hoverTickThreshold) {
                previousFocus = GetForegroundWindow(); isLocked = false; SetForegroundWindow(hwnd); needsRedraw = true;
            }
        } else {
            if (hoverTicks != 0 || !isLocked) {
                if (!isLocked && previousFocus && previousFocus != hwnd) SetForegroundWindow(previousFocus);
                needsRedraw = true; hoverTicks = 0; isLocked = true; dragging = false;
            }
        }

        float targetAlpha = (!isLocked || isResizing) ? 1.0f : passiveAlpha;
        if (std::abs(currentAlpha - targetAlpha) > 0.001f) { currentAlpha += (targetAlpha - currentAlpha) * fadeIncrement; needsRedraw = true; }

        std::string curStr = GetCountdownString();
        if (curStr != lastStr && !isResizing) { lastStr = curStr; needsRedraw = true; }

        if (needsRedraw) {
            SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), (BYTE)(255 * currentAlpha), LWA_COLORKEY | LWA_ALPHA);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
            if (!isResizing) {
                int totalTextW = 0, maxH = 0;
                for (char c : curStr) { totalTextW += glyphAtlas[c].w; maxH = std::max(maxH, glyphAtlas[c].h); }
                int targetWinW = (int)(totalTextW * 1.25f);
                if (winW != targetWinW) { winW = targetWinW; SDL_SetWindowSize(window, winW, winH); }
                int curX = (winW - totalTextW) / 2, startY = (winH - maxH) / 2;
                for (char c : curStr) {
                    CachedGlyph& g = glyphAtlas[c]; SDL_Rect r = { curX, startY, g.w, g.h };
                    int o[8][2] = {{1,1},{-1,-1},{1,-1},{-1,1},{1,0},{-1,0},{0,1},{0,-1}};
                    for (int i=0; i<8; i++) { SDL_Rect sr = { r.x+o[i][0], r.y+o[i][1], r.w, r.h }; SDL_RenderCopy(renderer, g.texOutline, NULL, &sr); }
                    SDL_RenderCopy(renderer, g.texMain, NULL, &r); curX += g.w;
                }
            } else {
                SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_Rect r = { 0, 0, winW, winH }; SDL_RenderDrawRect(renderer, &r);
            }
            SDL_RenderPresent(renderer);
        }
        SDL_Delay(10);
    }
    Shell_NotifyIcon(NIM_DELETE, &nid);
    ClearAtlas(); TTF_CloseFont(font); TTF_Quit(); SDL_Quit();
    return 0;
}