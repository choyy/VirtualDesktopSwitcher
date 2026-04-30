#include "ColorState.h"

COLORREF ParseColorString(const std::wstring &colorStr) {
    if (colorStr.empty()) { return RGB(255, 255, 255); }

    if (colorStr[0] == L'#' && colorStr.size() >= 7) {
        wchar_t *end = nullptr;
        uint32_t hex = wcstoul(colorStr.c_str() + 1, &end, 16);
        return RGB((hex >> 16u) & 0xFFu, (hex >> 8u) & 0xFFu, hex & 0xFFu);
    }

    int            r = 255, g = 255, b = 255;
    const wchar_t *s    = colorStr.c_str();
    wchar_t       *next = nullptr;
    r                   = static_cast<int>(wcstoul(s, &next, 10));
    if ((next != nullptr) && *next == L',') {
        s = next + 1;
    } else {
        return RGB(r, g, b);
    }
    g = static_cast<int>(wcstoul(s, &next, 10));
    if ((next != nullptr) && *next == L',') {
        s = next + 1;
    } else {
        return RGB(r, g, b);
    }
    b = static_cast<int>(wcstoul(s, &next, 10));
    return RGB(r, g, b);
}

// kPredefinedColors defined inline in ColorState.h

bool IsGradientColor(const std::wstring &hexColor) {
    return hexColor.find(L'_') != std::wstring::npos;
}
