// Adapted from Catime (Apache 2.0) - color_state.h
// Original: https://github.com/vladelaina/Catime
#ifndef COLOR_STATE_H
#define COLOR_STATE_H

#include <windows.h>
#include <string>
#include <vector>

struct PredefinedColor {
    std::wstring name;
    std::wstring hexColor;
};

const std::vector<PredefinedColor>& GetPredefinedColors();
bool IsGradientColor(const std::wstring& hexColor);
void ParseGradientColors(const std::wstring& hexColor, std::vector<COLORREF>& outColors);

#endif
