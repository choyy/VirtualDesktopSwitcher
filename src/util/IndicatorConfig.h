#pragma once

#include <windows.h>

#include <string>

enum class ShowMode : std::uint8_t { AlwaysShow = 0,
                                     AlwaysHide = 1,
                                     Show1s     = 2,
                                     Show3s     = 3,
                                     Count      = 4 };

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
    ShowMode     showMode       = ShowMode::AlwaysShow;

    void LoadFromIni();
    void SaveToIni() const;
};
