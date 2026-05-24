// Adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "stb_truetype.h"

class FontRenderer {
public:
    FontRenderer() = default;
    ~FontRenderer() { Cleanup(); }

    FontRenderer(const FontRenderer &)            = delete;
    FontRenderer &operator=(const FontRenderer &) = delete;
    FontRenderer(FontRenderer &&)                 = delete;
    FontRenderer &operator=(FontRenderer &&)      = delete;

    bool Init(const std::wstring &fontName);

    bool Measure(const wchar_t *text, int fontSize, int *width, int *height) const;
    void Render(void *bits, int bufWidth, int bufHeight, int textOffsetX, int textOffsetY,
                int textAreaW, int textAreaH, const wchar_t *text,
                const std::wstring &colorStr, int fontSize) const;

private:
    bool                       m_loaded = false;
    stbtt_fontinfo             m_fontInfo{};
    std::vector<unsigned char> m_fontData;

    void Cleanup();
    bool LoadFontData(const std::wstring &fontName);

    [[nodiscard]] float GetCodepointAdvance(wchar_t codepoint, wchar_t nextCodepoint, float scale) const;
    [[nodiscard]] int   GetGlyphIndex(wchar_t codepoint) const;
};
