// Adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#define NOMINMAX
#define STB_TRUETYPE_IMPLEMENTATION
#include "DrawingTextSTB.h"

#include <shlwapi.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

std::wstring FontRenderer::FindSystemFontPath(const std::wstring &fontName) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
                      0, KEY_READ, &hKey)
        != ERROR_SUCCESS) {
        return {};
    }

    std::array<wchar_t, 256>      vn{};
    std::array<wchar_t, MAX_PATH> data{};
    DWORD                         vns = static_cast<DWORD>(vn.size()), ds = sizeof(data), type = 0;
    DWORD                         idx = 0;
    while (RegEnumValueW(hKey, idx++, vn.data(), &vns, nullptr, &type, reinterpret_cast<BYTE *>(data.data()), &ds) == ERROR_SUCCESS) { // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        if (type == REG_SZ && (wcsstr(vn.data(), fontName.c_str()) != nullptr)) {
            RegCloseKey(hKey);
            std::wstring p = data.data();
            if (p.find(L'\\') == std::wstring::npos) {
                std::array<wchar_t, MAX_PATH> wd{};
                GetWindowsDirectoryW(wd.data(), static_cast<UINT>(wd.size()));
                p = std::wstring(wd.data());
                p += L"\\Fonts\\";
                p += data.data();
            }
            return p;
        }
        vns = static_cast<DWORD>(vn.size());
        ds  = sizeof(data);
    }
    RegCloseKey(hKey);
    return {};
}

bool FontRenderer::LoadFontFile(const std::wstring &path) {
    m_fontFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (m_fontFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    m_fontMapping = CreateFileMappingW(m_fontFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (m_fontMapping == nullptr) {
        CloseHandle(m_fontFile);
        m_fontFile = INVALID_HANDLE_VALUE;
        return false;
    }

    m_fontBuffer = static_cast<unsigned char *>(MapViewOfFile(m_fontMapping, FILE_MAP_READ, 0, 0, 0));
    if (m_fontBuffer == nullptr) {
        CloseHandle(m_fontMapping);
        CloseHandle(m_fontFile);
        m_fontMapping = nullptr;
        m_fontFile    = INVALID_HANDLE_VALUE;
        return false;
    }
    return true;
}

void FontRenderer::UnloadFontFile() {
    if (m_fontBuffer != nullptr) {
        UnmapViewOfFile(m_fontBuffer);
        m_fontBuffer = nullptr;
    }
    if (m_fontMapping != nullptr) {
        CloseHandle(m_fontMapping);
        m_fontMapping = nullptr;
    }
    if (m_fontFile != INVALID_HANDLE_VALUE) {
        CloseHandle(m_fontFile);
        m_fontFile = INVALID_HANDLE_VALUE;
    }
}

bool FontRenderer::Init(const std::wstring &fontName) {
    Cleanup();

    std::wstring path = FindSystemFontPath(fontName);
    if (path.empty()) {
        return false;
    }

    if (!LoadFontFile(path)) {
        return false;
    }

    if (stbtt_InitFont(&m_fontInfo, m_fontBuffer, stbtt_GetFontOffsetForIndex(m_fontBuffer, 0)) == 0) {
        UnloadFontFile();
        return false;
    }

    m_loaded = true;
    return true;
}

void FontRenderer::Cleanup() {
    m_loaded = false;
    UnloadFontFile();
}

bool FontRenderer::Measure(const wchar_t *text, int fontSize, int *width, int *height) const {
    if (!m_loaded || text == nullptr || width == nullptr || height == nullptr) {
        return false;
    }

    float sc = stbtt_ScaleForPixelHeight(&m_fontInfo, static_cast<float>(fontSize));
    int   a = 0, d = 0;
    stbtt_GetFontVMetrics(&m_fontInfo, &a, &d, nullptr);

    float x        = 0;
    auto  textSpan = std::span(text, wcslen(text));
    for (size_t i = 0; i < textSpan.size(); ++i) {
        int adv = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&m_fontInfo, textSpan[i], &adv, &lsb);
        x += static_cast<float>(adv) * sc;
        if (i + 1 < textSpan.size()) {
            int ni = stbtt_FindGlyphIndex(&m_fontInfo, textSpan[i + 1]);
            int ci = stbtt_FindGlyphIndex(&m_fontInfo, textSpan[i]);
            if ((ni != 0) && (ci != 0)) {
                x += static_cast<float>(stbtt_GetGlyphKernAdvance(&m_fontInfo, ci, ni)) * sc;
            }
        }
    }
    *width  = static_cast<int>(x + 1);
    *height = static_cast<int>(static_cast<float>(a - d) * sc + 1);
    return true;
}

void FontRenderer::Render(void *bits, int bufWidth, int bufHeight,
                          int textOffsetX, int textOffsetY,
                          int textAreaW, int textAreaH,
                          const wchar_t               *text,
                          const std::vector<COLORREF> &colors, int fontSize) const {
    if (!m_loaded || bits == nullptr || text == nullptr || colors.empty()) {
        return;
    }

    float sc = stbtt_ScaleForPixelHeight(&m_fontInfo, static_cast<float>(fontSize));
    int   a = 0, d = 0;
    stbtt_GetFontVMetrics(&m_fontInfo, &a, &d, nullptr);

    int tw = 0, th = 0;
    Measure(text, fontSize, &tw, &th);
    if (tw <= 0) {
        return;
    }

    int sx = textOffsetX + (textAreaW - tw) / 2;
    int bl = textOffsetY + (textAreaH - th) / 2 + static_cast<int>(static_cast<float>(a) * sc);

    bool  isGradient = colors.size() > 1;
    float x          = 0;

    auto textSpan = std::span(text, wcslen(text));
    for (size_t i = 0; i < textSpan.size(); ++i) {
        int adv = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&m_fontInfo, textSpan[i], &adv, &lsb);
        int gi = stbtt_FindGlyphIndex(&m_fontInfo, textSpan[i]);
        if (gi == 0) {
            x += static_cast<float>(adv) * sc;
            continue;
        }

        int kern = 0;
        if (i + 1 < textSpan.size()) {
            int ni = stbtt_FindGlyphIndex(&m_fontInfo, textSpan[i + 1]);
            if (ni != 0) {
                kern = static_cast<int>(static_cast<float>(stbtt_GetGlyphKernAdvance(&m_fontInfo, gi, ni)) * sc);
            }
        }

        int            cw = 0, ch = 0, xo = 0, yo = 0;
        unsigned char *bmp = stbtt_GetGlyphBitmap(&m_fontInfo, sc, sc, gi, &cw, &ch, &xo, &yo);
        if (bmp == nullptr) {
            x += static_cast<float>(adv) * sc + static_cast<float>(kern);
            continue;
        }

        std::span<const unsigned char> bmpSpan(bmp, static_cast<size_t>(cw * ch));

        int px = sx + static_cast<int>(x) + xo;
        int py = bl + yo;

        for (int r = 0; r < ch; ++r) {
            int sy = py + r;
            if (sy < 0 || sy >= bufHeight) {
                continue;
            }
            for (int c = 0; c < cw; ++c) {
                int dx = px + c;
                if (dx < 0 || dx >= bufWidth) {
                    continue;
                }
                unsigned char alpha = bmpSpan[r * cw + c];
                if (alpha == 0) {
                    continue;
                }

                int rr = 0, gg = 0, bb = 0;
                if (isGradient) {
                    float t    = static_cast<float>(dx) / static_cast<float>(bufWidth);
                    t          = std::clamp(t, 0.0f, 1.0f);
                    int   segs = static_cast<int>(colors.size()) - 1;
                    float segT = t * static_cast<float>(segs);
                    int   idx  = static_cast<int>(segT);
                    if (idx >= segs) {
                        idx  = segs - 1;
                        segT = 1.0f;
                    } else {
                        segT -= static_cast<float>(idx);
                    }
                    float s = 1.0f - segT;
                    rr      = static_cast<int>(GetRValue(colors[idx]) * s + GetRValue(colors[idx + 1]) * segT);
                    gg      = static_cast<int>(GetGValue(colors[idx]) * s + GetGValue(colors[idx + 1]) * segT);
                    bb      = static_cast<int>(GetBValue(colors[idx]) * s + GetBValue(colors[idx + 1]) * segT);
                } else {
                    COLORREF c = colors[0];
                    rr         = GetRValue(c);
                    gg         = GetGValue(c);
                    bb         = GetBValue(c);
                }

                auto  pixels        = std::span(static_cast<DWORD *>(bits), static_cast<size_t>(bufWidth) * bufHeight);
                auto &pix           = pixels[sy * bufWidth + dx];
                DWORD existingAlpha = (pix >> 24u) & 0xFFu;
                if (alpha > existingAlpha) {
                    pix = (static_cast<DWORD>(alpha) << 24u) | (static_cast<DWORD>(rr * alpha / 255) << 16u) | (static_cast<DWORD>(gg * alpha / 255) << 8u) | static_cast<DWORD>(bb * alpha / 255);
                }
            }
        }
        stbtt_FreeBitmap(bmp, nullptr);
        x += static_cast<float>(adv) * sc + static_cast<float>(kern);
    }
}
