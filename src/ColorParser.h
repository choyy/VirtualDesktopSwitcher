// Adapted from Catime (Apache 2.0) - color_parser.h
// Original: https://github.com/vladelaina/Catime
#ifndef COLOR_PARSER_H
#define COLOR_PARSER_H

#include <windows.h>
#include <string>

COLORREF ParseColorString(const std::wstring& colorStr);
COLORREF ParseFirstColor(const std::wstring& colorStr);
std::wstring ColorToHex(COLORREF color);

#endif
