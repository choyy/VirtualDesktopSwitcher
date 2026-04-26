// Adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#ifndef COLOR_STATE_H
#define COLOR_STATE_H

#include <windows.h>

#include <string>
#include <vector>

COLORREF ParseColorString(const std::wstring &colorStr);

const std::vector<std::wstring> &GetPredefinedColors();
bool                             IsGradientColor(const std::wstring &hexColor);
void                             ParseGradientColors(const std::wstring &hexColor, std::vector<COLORREF> &outColors);

#endif
