// Adapted from Catime (Apache 2.0) - drawing_text_stb.c
// Original: https://github.com/vladelaina/Catime
#define NOMINMAX
#define STB_TRUETYPE_IMPLEMENTATION
#include "../libs/stb_truetype.h"

#include "DrawingTextSTB.h"
#include <vector>
#include <algorithm>
#include <shlwapi.h>

static stbtt_fontinfo g_fontInfo;
static bool g_fontLoaded = false;

struct MappedFont {
    unsigned char* buffer = nullptr;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = nullptr;
};
static MappedFont g_mapped;

static std::wstring FindSystemFontPath(const std::wstring& fontName) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t vn[256], data[MAX_PATH];
        DWORD vns = 256, ds = sizeof(data), type;
        DWORD idx = 0;
        while (RegEnumValueW(hKey, idx++, vn, &vns, nullptr, &type, (BYTE*)data, &ds) == ERROR_SUCCESS) {
            if (type == REG_SZ && wcsstr(vn, fontName.c_str())) {
                RegCloseKey(hKey);
                std::wstring p = data;
                if (p.find(L'\\') == std::wstring::npos) {
                    wchar_t wd[MAX_PATH]; GetWindowsDirectoryW(wd, MAX_PATH);
                    p = std::wstring(wd) + L"\\Fonts\\" + p;
                }
                return p;
            }
            vns = 256; ds = sizeof(data);
        }
        RegCloseKey(hKey);
    }
    return L"";
}
static bool LoadFile(const std::wstring& path) {
    g_mapped.hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_mapped.hFile == INVALID_HANDLE_VALUE) return false;
    g_mapped.hMapping = CreateFileMappingW(g_mapped.hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!g_mapped.hMapping) { CloseHandle(g_mapped.hFile); g_mapped.hFile = INVALID_HANDLE_VALUE; return false; }
    g_mapped.buffer = (unsigned char*)MapViewOfFile(g_mapped.hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!g_mapped.buffer) { CloseHandle(g_mapped.hMapping); CloseHandle(g_mapped.hFile); return false; }
    return true;
}
static void UnloadFile() {
    if (g_mapped.buffer) { UnmapViewOfFile(g_mapped.buffer); g_mapped.buffer = nullptr; }
    if (g_mapped.hMapping) { CloseHandle(g_mapped.hMapping); g_mapped.hMapping = nullptr; }
    if (g_mapped.hFile != INVALID_HANDLE_VALUE) { CloseHandle(g_mapped.hFile); g_mapped.hFile = INVALID_HANDLE_VALUE; }
}

bool InitFontSTB(const std::wstring& fontName) {
    CleanupFontSTB();
    std::wstring p = FindSystemFontPath(fontName);
    if (p.empty()) return false;
    if (!LoadFile(p)) return false;
    if (!stbtt_InitFont(&g_fontInfo, g_mapped.buffer, stbtt_GetFontOffsetForIndex(g_mapped.buffer, 0))) {
        UnloadFile(); return false;
    }
    g_fontLoaded = true;
    return true;
}
void CleanupFontSTB() { g_fontLoaded = false; UnloadFile(); }

bool MeasureTextSTB(const wchar_t* text, int fontSize, int* width, int* height) {
    if (!g_fontLoaded || !text || !width || !height) return false;
    float sc = stbtt_ScaleForPixelHeight(&g_fontInfo, (float)fontSize);
    int a, d; stbtt_GetFontVMetrics(&g_fontInfo, &a, &d, nullptr);
    float x = 0;
    for (const wchar_t* p = text; *p; ++p) {
        int adv, lsb; stbtt_GetCodepointHMetrics(&g_fontInfo, *p, &adv, &lsb);
        x += adv * sc;
        if (*(p+1)) {
            int ni = stbtt_FindGlyphIndex(&g_fontInfo, *(p+1));
            int ci = stbtt_FindGlyphIndex(&g_fontInfo, *p);
            if (ni && ci) x += stbtt_GetGlyphKernAdvance(&g_fontInfo, ci, ni) * sc;
        }
    }
    *width = (int)(x + 1); *height = (int)((a - d) * sc + 1);
    return true;
}

void RenderTextSTB(void* bits, int bufWidth, int bufHeight, int textOffsetX, int textOffsetY,
                   int textAreaW, int textAreaH, const wchar_t* text,
                   const std::vector<COLORREF>& colors, int fontSize) {
    if (!g_fontLoaded || !bits || !text || colors.empty()) return;

    float sc = stbtt_ScaleForPixelHeight(&g_fontInfo, (float)fontSize);
    int a, d; stbtt_GetFontVMetrics(&g_fontInfo, &a, &d, nullptr);

    int tw = 0, th = 0;
    MeasureTextSTB(text, fontSize, &tw, &th);
    if (tw <= 0) return;

    int sx = textOffsetX + (textAreaW - tw) / 2;
    int bl = textOffsetY + (textAreaH - th) / 2 + (int)(a * sc);

    bool grad = colors.size() > 1;
    float x = 0;

    for (const wchar_t* p = text; *p; ++p) {
        int adv, lsb; stbtt_GetCodepointHMetrics(&g_fontInfo, *p, &adv, &lsb);
        int gi = stbtt_FindGlyphIndex(&g_fontInfo, *p);
        if (!gi) { x += adv * sc; continue; }

        int kern = 0;
        if (*(p+1)) {
            int ni = stbtt_FindGlyphIndex(&g_fontInfo, *(p+1));
            if (ni) kern = (int)(stbtt_GetGlyphKernAdvance(&g_fontInfo, gi, ni) * sc);
        }

        int cw, ch, xo, yo;
        unsigned char* bmp = stbtt_GetGlyphBitmap(&g_fontInfo, sc, sc, gi, &cw, &ch, &xo, &yo);
        if (!bmp) { x += adv * sc + kern; continue; }

        int px = sx + (int)x + xo;
        int py = bl + yo;

        for (int r = 0; r < ch; ++r) {
            int sy = py + r; if (sy < 0 || sy >= bufHeight) continue;
            for (int c = 0; c < cw; ++c) {
                int dx = px + c; if (dx < 0 || dx >= bufWidth) continue;
                unsigned char al = bmp[r * cw + c];
                if (al == 0) continue;

                int rr, gg, bb;
                if (grad) {
                    float t = (float)dx / bufWidth; if (t < 0) t = 0; if (t > 1) t = 1;
                    int sg = (int)colors.size() - 1; float st = t * sg; int idx = (int)st;
                    if (idx >= sg) { idx = sg - 1; st = 1; } else st -= idx;
                    rr = (int)(GetRValue(colors[idx])*(1-st) + GetRValue(colors[idx+1])*st);
                    gg = (int)(GetGValue(colors[idx])*(1-st) + GetGValue(colors[idx+1])*st);
                    bb = (int)(GetBValue(colors[idx])*(1-st) + GetBValue(colors[idx+1])*st);
                } else {
                    COLORREF c = colors[0]; rr = GetRValue(c); gg = GetGValue(c); bb = GetBValue(c);
                }

                DWORD* px = &((DWORD*)bits)[sy * bufWidth + dx];
                DWORD ca = (*px >> 24) & 0xFF;
                if (al > ca) {
                    *px = ((DWORD)al << 24) | ((rr * al / 255) << 16) | ((gg * al / 255) << 8) | (bb * al / 255);
                }
            }
        }
        stbtt_FreeBitmap(bmp, nullptr);
        x += adv * sc + kern;
    }
}
