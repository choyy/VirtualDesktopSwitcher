// Adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#ifndef DRAWING_TEXT_STB_H
#define DRAWING_TEXT_STB_H

#include <windows.h>

#include <string>

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
    void Cleanup();

    bool Measure(const wchar_t *text, int fontSize, int *width, int *height) const;
    void Render(void *bits, int bufWidth, int bufHeight, int textOffsetX, int textOffsetY,
                int textAreaW, int textAreaH, const wchar_t *text,
                const std::wstring &colorStr, int fontSize) const;

    [[nodiscard]] bool IsLoaded() const { return m_loaded; }

private:
    bool           m_loaded = false;
    stbtt_fontinfo m_fontInfo{};
    unsigned char *m_fontBuffer  = nullptr;
    HANDLE         m_fontFile    = INVALID_HANDLE_VALUE;
    HANDLE         m_fontMapping = nullptr;

    static std::wstring FindSystemFontPath(const std::wstring &fontName);
    bool                LoadFontFile(const std::wstring &path);
    void                UnloadFontFile();
};

#endif
