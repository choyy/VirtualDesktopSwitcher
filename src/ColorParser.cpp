// Adapted from Catime (Apache 2.0) - color_parser.c
// Original: https://github.com/vladelaina/Catime
#include "ColorParser.h"

COLORREF ParseColorString(const std::wstring& colorStr) {
    if (colorStr.empty()) return RGB(255, 255, 255);

    if (colorStr.size() >= 7 && colorStr[0] == L'#') {
        unsigned int r = 255, g = 255, b = 255;
        swscanf_s(colorStr.c_str() + 1, L"%02x%02x%02x", &r, &g, &b);
        return RGB(r, g, b);
    }

    int r = 255, g = 255, b = 255;
    swscanf_s(colorStr.c_str(), L"%d,%d,%d", &r, &g, &b);
    return RGB(r, g, b);
}

COLORREF ParseFirstColor(const std::wstring& colorStr) {
    size_t pos = colorStr.find(L'_');
    if (pos != std::wstring::npos) {
        return ParseColorString(colorStr.substr(0, pos));
    }
    return ParseColorString(colorStr);
}

std::wstring ColorToHex(COLORREF color) {
    wchar_t buf[8];
    swprintf_s(buf, L"#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
    return buf;
}
