#pragma once

#include <windows.h>

#include <string>

struct IndicatorConfig {
    std::wstring textColor      = L"#FFA745_#FE869F_#EF7AC8_#A083ED_#43AEFF";
    std::wstring fontName       = L"Segoe UI Symbol";
    int          fontSize       = 20;
    int          charSpacing    = 0;
    std::wstring currentSymbol  = L"\u25C9";
    std::wstring otherSymbol    = L"\u25CB";
    std::wstring emptySymbol    = L"\u25CC";
    POINT        windowPos      = {};
    bool         posInitialized = false;
    int          positionPreset = -1;

    void LoadFromIni();
    void SaveToIni() const;
};
