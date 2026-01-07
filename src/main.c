#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_syswm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
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

typedef enum { MODE_DEFAULT, MODE_ALLOW_DARK, MODE_FORCE_DARK, MODE_FORCE_LIGHT, MODE_MAX } PREFERRED_APP_MODE;
typedef PREFERRED_APP_MODE(WINAPI* SET_PREFERRED_APP_MODE)(PREFERRED_APP_MODE mode);
NOTIFYICONDATA g_nid = { 0 };
HWND g_prev_focus = NULL;
#endif

#define VERSION_STR "1.4"

typedef struct {
    SDL_Texture* main_tex;
    SDL_Texture* outline_tex;
    int w;
    int h;
} GLYPH_CACHE;

typedef struct {
    const char* key;
    const char* def;
    const char* desc;
} CONFIG_PAIR;

bool g_is_locked = true;
bool g_is_resizing = false;
bool g_is_inverted = false;
bool g_has_colon = true;
bool g_show_seconds = true;
float g_current_alpha = 0.1f;
int g_hover_ticks = 0;
int g_win_w = 320;
int g_win_h = 100;
int g_font_size = 52;
int g_hover_threshold = 100;
float g_alpha_step = 0.05f;
float g_passive_alpha = 0.1f;
const float G_ASPECT_RATIO = 100.0f / 320.0f;

GLYPH_CACHE g_atlas[256] = { 0 };
unsigned char* g_font_mem = NULL;

int version_to_int(const char* ptr) {
    int val = 0;
    while (*ptr) {
        if (*ptr >= '0' && *ptr <= '9') val = val * 10 + (*ptr - '0');
        ptr++;
    }
    return val;
}

bool is_system_light_mode(void) {
#ifdef _WIN32
    HKEY key;
    DWORD val = 0;
    DWORD sz = sizeof(val);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        RegQueryValueExA(key, "AppsUseLightTheme", NULL, NULL, (LPBYTE)&val, &sz);
        RegCloseKey(key);
    }
    return val != 0;
#else
    return false;
#endif
}

void update_tray_icon(SDL_Window* win) {
#ifdef _WIN32
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    SDL_GetWindowWMInfo(win, &wm);
    HWND hwnd = wm.info.win.window;
    HINSTANCE inst = GetModuleHandle(NULL);
    HICON icon = LoadIcon(inst, MAKEINTRESOURCE(g_is_inverted ? IDI_ICON_DARK : IDI_ICON_LIGHT));
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
    g_nid.hIcon = icon;
    Shell_NotifyIcon(NIM_MODIFY, &g_nid);
#endif
}

void load_config(void) {
    char path[512], ini[512], buf[64], ver[16];
    CONFIG_PAIR pairs[] = {
        {"HoverThreshold", "1", "Delay before window becomes interactive (seconds)"},
        {"AlphaIncrement", "0.05", "Speed of the fade animation"},
        {"PassiveAlpha", "0.1", "Opacity when not hovered (0.0 to 1.0)"},
        {"ShowSeconds", "true", "Display seconds in countdown"}
    };
    int pair_count = sizeof(pairs) / sizeof(pairs[0]);

#ifdef _WIN32
    GetModuleFileNameA(NULL, path, 512);
    char* slash = strrchr(path, '\\');
    if (slash) *slash = '\0';
    snprintf(ini, 512, "%s\\Pressure.ini", path);
    GetPrivateProfileStringA("Settings", "Version", "0.0", ver, sizeof(ver), ini);
    
    int cur_v = version_to_int(VERSION_STR);
    int ini_v = version_to_int(ver);

    if (ini_v > cur_v) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Please update Pressure to v%s", ver);
        if (MessageBoxA(NULL, msg, "Update Available", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
            ShellExecuteA(NULL, "open", "https://github.com/woaded/pressure/releases/latest/", NULL, NULL, SW_SHOWNORMAL);
            exit(0);
        }
    } else if (ini_v < cur_v) {
        char temp_vals[16][64];
        for (int i = 0; i < pair_count; i++) {
            GetPrivateProfileStringA("Settings", pairs[i].key, pairs[i].def, temp_vals[i], 64, ini);
        }

        FILE* f = fopen(ini, "w");
        if (f) {
            fprintf(f, "[Settings]\nVersion=%s\n", VERSION_STR);
            for (int i = 0; i < pair_count; i++) {
                fprintf(f, "; %s\n%s=%s\n", pairs[i].desc, pairs[i].key, temp_vals[i]);
            }
            fclose(f);
        }
    }

    g_hover_threshold = GetPrivateProfileIntA("Settings", "HoverThreshold", 1, ini) * 100;
    GetPrivateProfileStringA("Settings", "AlphaIncrement", "0.05", buf, sizeof(buf), ini);
    g_alpha_step = (float)atof(buf);
    GetPrivateProfileStringA("Settings", "PassiveAlpha", "0.1", buf, sizeof(buf), ini);
    g_passive_alpha = (float)atof(buf);
    GetPrivateProfileStringA("Settings", "ShowSeconds", "true", buf, sizeof(buf), ini);
    g_show_seconds = (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0);
#else
    g_hover_threshold = 100;
    g_alpha_step = 0.05f;
    g_passive_alpha = 0.1f;
    g_show_seconds = true;
#endif
    if (g_passive_alpha < 0.0f) g_passive_alpha = 0.0f;
    if (g_passive_alpha > 1.0f) g_passive_alpha = 1.0f;
    g_current_alpha = g_passive_alpha;
}

TTF_Font* load_font(const char* path, int pts) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (g_font_mem) free(g_font_mem);
    g_font_mem = malloc(sz);
    if (fread(g_font_mem, 1, sz, f) == (size_t)sz) {
        fclose(f);
        return TTF_OpenFontRW(SDL_RWFromMem(g_font_mem, (int)sz), 1, pts);
    }
    fclose(f);
    return NULL;
}

void apply_theme(SDL_Window* win, bool dark) {
#ifdef _WIN32
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    SDL_GetWindowWMInfo(win, &wm);
    HWND hwnd = wm.info.win.window;
    BOOL set = dark ? TRUE : FALSE;
    HMODULE lib = LoadLibraryA("uxtheme.dll");
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &set, sizeof(set));
    if (lib) {
        SET_PREFERRED_APP_MODE proc = (SET_PREFERRED_APP_MODE)GetProcAddress(lib, MAKEINTRESOURCEA(135));
        if (proc) proc(dark ? MODE_FORCE_DARK : MODE_FORCE_LIGHT);
        FreeLibrary(lib);
    }
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
#endif
}

void apply_gradient(SDL_Surface* surf, SDL_Color c1, SDL_Color c2) {
    if (!surf) return;
    SDL_LockSurface(surf);
    Uint32* px = (Uint32*)surf->pixels;
    for (int y = 0; y < surf->h; y++) {
        float f = (float)y / (float)surf->h;
        Uint8 r = (Uint8)(c1.r + f * (c2.r - c1.r));
        Uint8 g = (Uint8)(c1.g + f * (c2.g - c1.g));
        Uint8 b = (Uint8)(c1.b + f * (c2.b - c1.b));
        for (int x = 0; x < surf->w; x++) {
            Uint32* p = &px[y * (surf->pitch / 4) + x];
            Uint8 a = (*p >> 24) & 0xFF;
            if (a > 0) *p = (a << 24) | (((r * a) / 255) << 16) | (((g * a) / 255) << 8) | ((b * a) / 255);
        }
    }
    SDL_UnlockSurface(surf);
}

void clear_atlas(void) {
    for (int i = 0; i < 256; i++) {
        if (g_atlas[i].main_tex) SDL_DestroyTexture(g_atlas[i].main_tex);
        if (g_atlas[i].outline_tex) SDL_DestroyTexture(g_atlas[i].outline_tex);
        g_atlas[i].main_tex = g_atlas[i].outline_tex = NULL;
    }
}

void build_atlas(SDL_Renderer* rend, TTF_Font* font) {
    const char* chars = "0123456789 :";
    SDL_Color white = {255, 255, 255, 255};
    clear_atlas();
    if (!font) return;
    g_has_colon = TTF_GlyphIsProvided(font, ':');
    SDL_Color text_c = g_is_inverted ? (SDL_Color){23,23,23,255} : (SDL_Color){255,255,255,255};
    SDL_Color grad_c = g_is_inverted ? (SDL_Color){12,12,12,255} : (SDL_Color){208,208,208,255};
    SDL_Color out_c  = g_is_inverted ? (SDL_Color){240,240,240,255} : (SDL_Color){10,10,10,255};
    for (int i = 0; chars[i]; i++) {
        char c = chars[i];
        SDL_Surface* sm = TTF_RenderGlyph_Blended(font, (Uint16)c, white);
        SDL_Surface* so = TTF_RenderGlyph_Blended(font, (Uint16)c, out_c);
        if (sm && so) {
            apply_gradient(sm, text_c, grad_c);
            g_atlas[(int)c].main_tex = SDL_CreateTextureFromSurface(rend, sm);
            g_atlas[(int)c].outline_tex = SDL_CreateTextureFromSurface(rend, so);
            g_atlas[(int)c].w = sm->w; g_atlas[(int)c].h = sm->h;
            SDL_FreeSurface(sm); SDL_FreeSurface(so);
        }
    }
}

void get_time_str(char* buf, size_t sz) {
    time_t now = time(NULL);
    struct tm lt, tgt;
#ifdef _WIN32
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif
    int d = (6 - lt.tm_wday + 7) % 7;
    if (d == 0 && (lt.tm_hour > 0 || lt.tm_min > 0 || lt.tm_sec > 0)) d = 7;
    tgt = lt; tgt.tm_mday += d; tgt.tm_hour = tgt.tm_min = tgt.tm_sec = 0; tgt.tm_isdst = -1;
    long long diff = (long long)difftime(mktime(&tgt), now);
    if (diff < 0) diff = 0;
    d = (int)(diff / 86400); diff %= 86400;
    int h = (int)(diff / 3600); diff %= 3600;
    int m = (int)(diff / 60), s = (int)(diff % 60);
    if (d > 0) {
        if (g_show_seconds) snprintf(buf, sz, "%d:%02d:%02d:%02d", d, h, m, s);
        else snprintf(buf, sz, "%d:%02d:%02d", d, h, m);
    } else {
        if (g_show_seconds) snprintf(buf, sz, "%02d:%02d:%02d", h, m, s);
        else snprintf(buf, sz, "%02d:%02d", h, m);
    }
    if (!g_has_colon) for(int i=0; buf[i]; i++) if(buf[i] == ':') buf[i] = ' ';
}

int main(int argc, char* argv[]) {
    char path[512], font_p[512], last_s[32] = "", cur_s[32] = "";
    SDL_Window* win; SDL_Renderer* rend; TTF_Font* font; SDL_Event ev;
    bool run = true, drag = false;
    int mx, my, wx, wy, gx, gy;

    load_config();
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    
#ifdef _WIN32
    GetModuleFileNameA(NULL, path, 512);
    char* slash = strrchr(path, '\\');
    if (slash) *slash = '\0';
    snprintf(font_p, 512, "%s\\font.ttf", path);
#else
    snprintf(font_p, 512, "font.ttf");
#endif

    font = load_font(font_p, g_font_size);
    if (!font) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Missing font.ttf", NULL);
        return 1;
    }

    g_is_inverted = is_system_light_mode();
    win = SDL_CreateWindow("Pressure", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_win_w, g_win_h, SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_HIDDEN);
    rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

#ifdef _WIN32
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    SDL_GetWindowWMInfo(win, &wm);
    HWND hwnd = wm.info.win.window;
    apply_theme(win, !g_is_inverted);
    
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TOOLWINDOW);
    
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    strcpy(g_nid.szTip, "Pressure");
    update_tray_icon(win);
    Shell_NotifyIcon(NIM_ADD, &g_nid);
    HMENU menu = CreatePopupMenu();
    AppendMenu(menu, MF_STRING, ID_TRAY_RESIZE, "Resize Mode");
    AppendMenu(menu, MF_STRING, ID_TRAY_INVERT, "Invert Colors");
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(menu, MF_STRING, ID_TRAY_EXIT, "Quit");
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif

    build_atlas(rend, font);
    SDL_ShowWindow(win);

    while (run) {
        bool redraw = false;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) run = false;
#ifdef _WIN32
            if (ev.type == SDL_SYSWMEVENT) {
                UINT msg = ev.syswm.msg->msg.win.msg;
                LPARAM lp = ev.syswm.msg->msg.win.lParam;
                if (msg == WM_TRAYICON && (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_LBUTTONUP)) {
                    POINT p; GetCursorPos(&p); SetForegroundWindow(hwnd);
                    int id = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, hwnd, NULL);
                    if (id == ID_TRAY_EXIT) run = false;
                    if (id == ID_TRAY_RESIZE) g_is_resizing = true;
                    if (id == ID_TRAY_INVERT) { g_is_inverted = !g_is_inverted; apply_theme(win, !g_is_inverted); build_atlas(rend, font); update_tray_icon(win); redraw = true; }
                }
            }
#endif
            if (!g_is_resizing && !g_is_locked) {
                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    if (ev.button.button == SDL_BUTTON_LEFT) { drag = true; SDL_GetMouseState(&mx, &my); }
                    else if (ev.button.button == SDL_BUTTON_RIGHT) {
#ifdef _WIN32
                        POINT p; GetCursorPos(&p); SetForegroundWindow(hwnd);
                        int id = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, hwnd, NULL);
                        if (id == ID_TRAY_EXIT) run = false;
                        if (id == ID_TRAY_RESIZE) g_is_resizing = true;
                        if (id == ID_TRAY_INVERT) { g_is_inverted = !g_is_inverted; apply_theme(win, !g_is_inverted); build_atlas(rend, font); update_tray_icon(win); redraw = true; }
#endif
                    }
                }
                if (ev.type == SDL_MOUSEBUTTONUP) drag = false;
            }
        }

        if (g_is_resizing) {
            bool confirm = false;
#ifdef _WIN32
            confirm = (GetAsyncKeyState(VK_LBUTTON) & 0x8000 || GetAsyncKeyState(VK_RETURN) & 0x8000);
#else
            int btn = SDL_GetMouseState(NULL, NULL);
            confirm = (btn & SDL_BUTTON(SDL_BUTTON_LEFT));
#endif
            if (confirm) {
                g_is_resizing = false;
                g_font_size = (int)(52 * (g_win_w / 320.0f));
                TTF_CloseFont(font); font = load_font(font_p, g_font_size);
                build_atlas(rend, font); redraw = true;
            } else {
                SDL_GetGlobalMouseState(&gx, &gy); SDL_GetWindowPosition(win, &wx, &wy);
                int nh = abs(gy - wy); if (nh < 40) nh = 40; if (nh > 600) nh = 600;
                g_win_w = (int)(nh / G_ASPECT_RATIO); g_win_h = nh;
                SDL_SetWindowSize(win, g_win_w, g_win_h); redraw = true;
            }
        } else if (drag) {
            SDL_GetGlobalMouseState(&gx, &gy);
            SDL_SetWindowPosition(win, gx - mx, gy - my);
            redraw = true;
        }

        SDL_GetWindowPosition(win, &wx, &wy); SDL_GetGlobalMouseState(&gx, &gy);
        bool inside = (gx >= wx && gx <= wx + g_win_w && gy >= wy && gy <= wy + g_win_h);
        if (inside || g_is_resizing) {
            if (g_is_locked && !g_is_resizing && ++g_hover_ticks >= g_hover_threshold) {
#ifdef _WIN32
                g_prev_focus = GetForegroundWindow(); SetForegroundWindow(hwnd);
#endif
                g_is_locked = false; redraw = true;
            }
        } else {
            if (g_hover_ticks != 0 || !g_is_locked) {
#ifdef _WIN32
                if (!g_is_locked && g_prev_focus && g_prev_focus != hwnd) SetForegroundWindow(g_prev_focus);
#endif
                redraw = true; g_hover_ticks = 0; g_is_locked = true; drag = false;
            }
        }

        float target = (!g_is_locked || g_is_resizing) ? 1.0f : g_passive_alpha;
        if (fabsf(g_current_alpha - target) > 0.001f) { g_current_alpha += (target - g_current_alpha) * g_alpha_step; redraw = true; }
        get_time_str(cur_s, 32);
        if (strcmp(cur_s, last_s) != 0 && !g_is_resizing) { strcpy(last_s, cur_s); redraw = true; }

        if (redraw) {
#ifdef _WIN32
            SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), (BYTE)(255 * g_current_alpha), LWA_COLORKEY | LWA_ALPHA);
#else
            SDL_SetWindowOpacity(win, g_current_alpha);
#endif
            SDL_SetRenderDrawColor(rend, 0, 0, 0, 255); SDL_RenderClear(rend);
            if (!g_is_resizing) {
                int tw = 0, mh = 0;
                for (int i = 0; cur_s[i]; i++) { tw += g_atlas[(int)cur_s[i]].w; if(g_atlas[(int)cur_s[i]].h > mh) mh = g_atlas[(int)cur_s[i]].h; }
                if (g_win_w != tw || g_win_h != mh) { g_win_w = tw; g_win_h = mh; SDL_SetWindowSize(win, g_win_w, g_win_h); }
                int cx = 0;
                for (int i = 0; cur_s[i]; i++) {
                    GLYPH_CACHE* g = &g_atlas[(int)cur_s[i]]; SDL_Rect r = { cx, 0, g->w, g->h };
                    int o[8][2] = {{1,1},{-1,-1},{1,-1},{-1,1},{1,0},{-1,0},{0,1},{0,-1}};
                    for (int j=0; j<8; j++) { SDL_Rect sr = { r.x+o[j][0], r.y+o[j][1], r.w, r.h }; SDL_RenderCopy(rend, g->outline_tex, NULL, &sr); }
                    SDL_RenderCopy(rend, g->main_tex, NULL, &r); cx += g->w;
                }
            } else {
                SDL_SetRenderDrawColor(rend, 100, 100, 100, 255);
                SDL_Rect r = { 0, 0, g_win_w, g_win_h };
                SDL_RenderDrawRect(rend, &r);
            }
            SDL_RenderPresent(rend);
        }
        SDL_Delay(10);
    }

#ifdef _WIN32
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
#endif
    clear_atlas(); TTF_CloseFont(font); if (g_font_mem) free(g_font_mem);
    TTF_Quit(); SDL_Quit();
    return 0;
}