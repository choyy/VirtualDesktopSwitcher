// Adapted from Catime (Apache 2.0) - color_state.c
// Original: https://github.com/vladelaina/Catime
#include "ColorState.h"
#include "ColorParser.h"
#include <sstream>

// Exact 29 colors from Catime: 9 solid + 19 two-stop gradients + 1 multi-stop
static const std::vector<PredefinedColor> s_colors = {
    {L"", L"#FFFFFF"},
    {L"", L"#E3E3E5"},
    {L"", L"#000000"},
    {L"", L"#FF5F5F"},
    {L"", L"#F6ABB7"},
    {L"", L"#FB7FA4"},
    {L"", L"#F59E0B"},
    {L"", L"#22C55E"},
    {L"", L"#8771C6"},
    {L"", L"#FF9A9E_#FECFEF"},
    {L"", L"#FEA5B7_#FFDE9B"},
    {L"", L"#A8EDEA_#FED6E3"},
    {L"", L"#D299C2_#FEF9D7"},
    {L"", L"#FF9966_#FF5E62"},
    {L"", L"#ED4264_#FFEDBC"},
    {L"", L"#F6D365_#FDA085"},
    {L"", L"#FFE985_#FA742B"},
    {L"", L"#FF9A56_#56CCBA"},
    {L"", L"#10BD92_#8CE442"},
    {L"", L"#11998E_#38EF7D"},
    {L"", L"#43E97B_#38F9D7"},
    {L"", L"#89F7FE_#66A6FF"},
    {L"", L"#00C9FF_#92FE9D"},
    {L"", L"#648CFF_#64DC78"},
    {L"", L"#1F92A9_#EEE0D5"},
    {L"", L"#8E9EF3_#F774A0"},
    {L"", L"#FF5E96_#56C6FF"},
    {L"", L"#30CFD0_#330867"},
    {L"", L"#FFA745_#FE869F_#EF7AC8_#A083ED_#43AEFF"},
};

const std::vector<PredefinedColor>& GetPredefinedColors() {
    return s_colors;
}

bool IsGradientColor(const std::wstring& hexColor) {
    return hexColor.find(L'_') != std::wstring::npos;
}

void ParseGradientColors(const std::wstring& hexColor, std::vector<COLORREF>& outColors) {
    outColors.clear();
    std::wstringstream ss(hexColor);
    std::wstring token;
    while (std::getline(ss, token, L'_')) {
        outColors.push_back(ParseColorString(token));
    }
}
