#pragma once

#include <windows.h>

#include <string>

enum class ShowMode : std::uint8_t { AlwaysShow = 0,
                                     AlwaysHide = 1,
                                     Show1s     = 2,
                                     Show3s     = 3,
                                     Count      = 4 };

enum class PositionPreset : std::uint8_t { TopLeft           = 0,
                                           TopCenter         = 1,
                                           TopRight          = 2,
                                           Center            = 3,
                                           BottomLeft        = 4,
                                           BottomCenter      = 5,
                                           BottomRight       = 6,
                                           EmbedTaskbarRight = 7,
                                           EmbedTaskbarLeft  = 8,
                                           Count             = 9,
                                           Custom            = 0xFF };

enum class DragSwitchMode : std::uint8_t {
    Always = 0,
    Alt    = 1,
    Ctrl   = 2,
    Never  = 3,
    Count  = 4
};

struct IndicatorConfig {
    std::wstring   textColor      = L"#FFA745_#FE869F_#EF7AC8_#A083ED_#43AEFF";
    std::wstring   fontName       = L"Segoe UI Symbol";
    int            fontSize       = 20;
    int            charSpacing    = 0;
    std::wstring   currentSymbol  = L"\u25C9";
    std::wstring   otherSymbol    = L"\u25CB";
    std::wstring   emptySymbol    = L"\u25CC";
    POINT          windowPos      = {};
    bool           posInitialized = false;
    PositionPreset positionPreset = PositionPreset::Custom;
    ShowMode       showMode       = ShowMode::AlwaysShow;
    int            animMode       = 1;
    bool           autoContrast   = true;
    bool           autoFocus      = true;
    DragSwitchMode dragSwitchMode = DragSwitchMode::Always;

    void LoadFromIni();
    void SaveToIni() const;
};
