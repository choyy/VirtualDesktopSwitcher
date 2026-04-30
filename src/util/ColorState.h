// Adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#ifndef COLOR_STATE_H
#define COLOR_STATE_H

#include <windows.h>

#include <array>
#include <string>

COLORREF ParseColorString(const std::wstring &colorStr);

inline constexpr std::array<const wchar_t *, 29> kPredefinedColors = {
    L"#FFFFFF",
    L"#E3E3E5",
    L"#000000",
    L"#FF5F5F",
    L"#F6ABB7",
    L"#FB7FA4",
    L"#F59E0B",
    L"#22C55E",
    L"#8771C6",
    L"#FF9A9E_#FECFEF",
    L"#FEA5B7_#FFDE9B",
    L"#A8EDEA_#FED6E3",
    L"#D299C2_#FEF9D7",
    L"#FF9966_#FF5E62",
    L"#ED4264_#FFEDBC",
    L"#F6D365_#FDA085",
    L"#FFE985_#FA742B",
    L"#FF9A56_#56CCBA",
    L"#10BD92_#8CE442",
    L"#11998E_#38EF7D",
    L"#43E97B_#38F9D7",
    L"#89F7FE_#66A6FF",
    L"#00C9FF_#92FE9D",
    L"#648CFF_#64DC78",
    L"#1F92A9_#EEE0D5",
    L"#8E9EF3_#F774A0",
    L"#FF5E96_#56C6FF",
    L"#30CFD0_#330867",
    L"#FFA745_#FE869F_#EF7AC8_#A083ED_#43AEFF",
};

bool IsGradientColor(const std::wstring &hexColor);

#endif
