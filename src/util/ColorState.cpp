// Adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#include "ColorState.h"

#include <sstream>

COLORREF ParseColorString(const std::wstring &colorStr) {
    if (colorStr.empty()) { return RGB(255, 255, 255); }

    if (colorStr[0] == L'#' && colorStr.size() >= 7) {
        unsigned int r = 255, g = 255, b = 255;
        std::wstring hexStr = colorStr.substr(1, 6);
        r                   = std::stoul(hexStr.substr(0, 2), nullptr, 16);
        g                   = std::stoul(hexStr.substr(2, 2), nullptr, 16);
        b                   = std::stoul(hexStr.substr(4, 2), nullptr, 16);
        return RGB(r, g, b);
    }

    int                r = 255, g = 255, b = 255;
    std::wstringstream ss(colorStr);
    wchar_t            comma1 = 0, comma2 = 0;
    ss >> r >> comma1 >> g >> comma2 >> b;
    return RGB(r, g, b);
}

const std::vector<std::wstring> &GetPredefinedColors() {
    // 29 colors: 9 solid + 19 two-stop gradients + 1 multi-stop
    static const std::vector<std::wstring> colors = {
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
    return colors;
}

bool IsGradientColor(const std::wstring &hexColor) {
    return hexColor.find(L'_') != std::wstring::npos;
}

void ParseGradientColors(const std::wstring &hexColor, std::vector<COLORREF> &outColors) {
    outColors.clear();
    std::wstringstream ss(hexColor);
    std::wstring       token;
    while (std::getline(ss, token, L'_')) {
        outColors.push_back(ParseColorString(token));
    }
}
