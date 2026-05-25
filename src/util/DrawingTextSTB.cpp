// Adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#define STB_TRUETYPE_IMPLEMENTATION
#include "DrawingTextSTB.h"

#include <cstddef>

#include "util/Utils.h"

bool FontRenderer::LoadFontData(const std::wstring &fontName) {
    HDC hdc = CreateCompatibleDC(nullptr);
    if (hdc == nullptr) { return false; }

    HFONT hFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_TT_PRECIS,
                              CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, fontName.c_str());
    if (hFont == nullptr) {
        DeleteDC(hdc);
        return false;
    }

    auto *oldFont = SelectObject(hdc, hFont);
    DWORD size    = GetFontData(hdc, 0, 0, nullptr, 0);
    if (size == GDI_ERROR || size == 0) {
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
        DeleteDC(hdc);
        return false;
    }

    m_fontData.resize(size);

    DWORD result = GetFontData(hdc, 0, 0, m_fontData.data(), size);
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
    DeleteDC(hdc);

    if (result == GDI_ERROR || result != size) {
        m_fontData.clear();
        return false;
    }

    return true;
}

bool FontRenderer::Init(const std::wstring &fontName) {
    Cleanup();

    if (!LoadFontData(fontName)) {
        return false;
    }

    if (stbtt_InitFont(&m_fontInfo, m_fontData.data(), stbtt_GetFontOffsetForIndex(m_fontData.data(), 0)) == 0) {
        return false;
    }

    m_loaded = true;
    return true;
}

void FontRenderer::Cleanup() {
    m_loaded = false;
    m_fontData.clear();
}

float FontRenderer::GetCodepointAdvance(wchar_t codepoint, wchar_t nextCodepoint, float scale) const {
    int adv = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&m_fontInfo, codepoint, &adv, &lsb);
    float x = static_cast<float>(adv) * scale;
    if (nextCodepoint != 0) {
        int ci = stbtt_FindGlyphIndex(&m_fontInfo, codepoint);
        int ni = stbtt_FindGlyphIndex(&m_fontInfo, nextCodepoint);
        if (ci != 0 && ni != 0) {
            x += static_cast<float>(stbtt_GetGlyphKernAdvance(&m_fontInfo, ci, ni)) * scale;
        }
    }
    return x;
}

int FontRenderer::GetGlyphIndex(wchar_t codepoint) const {
    return stbtt_FindGlyphIndex(&m_fontInfo, codepoint);
}

SIZE FontRenderer::Measure(const wchar_t *text, int fontSize) const {
    if (!m_loaded || text == nullptr) { return {0, 0}; }

    float sc = stbtt_ScaleForPixelHeight(&m_fontInfo, static_cast<float>(fontSize));
    int   a = 0, d = 0;
    stbtt_GetFontVMetrics(&m_fontInfo, &a, &d, nullptr);

    float  x   = 0;
    size_t len = wcslen(text);
    for (size_t i = 0; i < len; ++i) {
        wchar_t next = (i + 1 < len) ? text[i + 1] : 0;
        x += GetCodepointAdvance(text[i], next, sc);
    }
    return {static_cast<int>(x + 1), static_cast<int>(static_cast<float>(a - d) * sc + 1)};
}

void FontRenderer::Render(void *bits, int bufWidth, int bufHeight,
                          int textOffsetX, int textOffsetY,
                          int textAreaW, int textAreaH,
                          const wchar_t      *text,
                          const std::wstring &colorStr, int fontSize) const {
    if (!m_loaded || bits == nullptr || text == nullptr) {
        return;
    }

    auto colors = ParseMultiColorString(colorStr);

    float sc = stbtt_ScaleForPixelHeight(&m_fontInfo, static_cast<float>(fontSize));
    int   a = 0, d = 0;
    stbtt_GetFontVMetrics(&m_fontInfo, &a, &d, nullptr);

    auto m = Measure(text, fontSize);
    if (m.cx <= 0) { return; }

    int sx = textOffsetX + (textAreaW - m.cx) / 2;
    int bl = textOffsetY + (textAreaH - m.cy) / 2 + static_cast<int>(static_cast<float>(a) * sc);

    bool   isGradient = colors.count > 1;
    float  x          = 0;
    size_t len        = wcslen(text);

    for (size_t i = 0; i < len; ++i) {
        wchar_t next = (i + 1 < len) ? text[i + 1] : 0;
        int     gi   = GetGlyphIndex(text[i]);
        if (gi == 0) {
            x += GetCodepointAdvance(text[i], next, sc);
            continue;
        }

        int            cw = 0, ch = 0, xo = 0, yo = 0;
        unsigned char *bmp = stbtt_GetGlyphBitmap(&m_fontInfo, sc, sc, gi, &cw, &ch, &xo, &yo);
        if (bmp == nullptr) {
            x += GetCodepointAdvance(text[i], next, sc);
            continue;
        }

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
                unsigned char alpha = bmp[r * cw + c];
                if (alpha == 0) {
                    continue;
                }

                COLORREF pixelColor = isGradient
                                          ? InterpolateGradientColor(colors.colors.data(), colors.count, static_cast<float>(dx) / static_cast<float>(bufWidth))
                                          : colors.colors.at(0);
                int      rr = GetRValue(pixelColor), gg = GetGValue(pixelColor), bb = GetBValue(pixelColor);

                auto *pixels        = static_cast<DWORD *>(bits);
                auto &pix           = pixels[sy * bufWidth + dx];
                DWORD existingAlpha = (pix >> 24u) & 0xFFu;
                if (alpha > existingAlpha) {
                    pix = (static_cast<DWORD>(alpha) << 24u) | (static_cast<DWORD>(rr * alpha / 255) << 16u) | (static_cast<DWORD>(gg * alpha / 255) << 8u) | static_cast<DWORD>(bb * alpha / 255);
                }
            }
        }
        stbtt_FreeBitmap(bmp, nullptr);
        x += GetCodepointAdvance(text[i], next, sc);
    }
}
