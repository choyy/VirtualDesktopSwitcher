// Adapted from Catime (Apache 2.0) - drawing_text_stb.h
// Original: https://github.com/vladelaina/Catime
#ifndef DRAWING_TEXT_STB_H
#define DRAWING_TEXT_STB_H

#include <windows.h>
#include <string>
#include <vector>

bool InitFontSTB(const std::wstring& fontName);
void CleanupFontSTB();
bool MeasureTextSTB(const wchar_t* text, int fontSize, int* width, int* height);
void RenderTextSTB(void* bits, int bufWidth, int bufHeight, int textOffsetX, int textOffsetY,
                   int textAreaW, int textAreaH, const wchar_t* text,
                   const std::vector<COLORREF>& colors, int fontSize);

#endif
