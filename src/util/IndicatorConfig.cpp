#include "IndicatorConfig.h"

#include "util/ConfigIni.h"

void IndicatorConfig::LoadFromIni() {
    std::wstring color = ReadIniString(L"Display", L"TextColor", L"#FFA745_#FE869F_#EF7AC8_#A083ED_#43AEFF");
    if (!color.empty()) { textColor = color; }

    int x = ReadIniInt(L"Display", L"WindowPosX", -1);
    int y = ReadIniInt(L"Display", L"WindowPosY", -1);
    if (x >= 0) {
        windowPos.x    = x;
        posInitialized = true;
    }
    if (y >= 0) {
        windowPos.y    = y;
        posInitialized = true;
    }

    int fs = ReadIniInt(L"Display", L"FontSize", 20);
    if (fs > 0) { fontSize = fs; }

    currentSymbol = ReadIniSymbol(L"Display", L"CurrentSymbol", currentSymbol);
    otherSymbol   = ReadIniSymbol(L"Display", L"OtherSymbol", otherSymbol);
    emptySymbol   = ReadIniSymbol(L"Display", L"EmptySymbol", emptySymbol);

    int sp = ReadIniInt(L"Display", L"CharSpacing", 0);
    if (sp >= 0) { charSpacing = sp; }

    std::wstring fn = ReadIniString(L"Display", L"FontName", L"Segoe UI Symbol");
    if (!fn.empty()) { fontName = fn; }

    positionPreset = ReadIniInt(L"Display", L"PositionPreset", -1);
}

void IndicatorConfig::SaveToIni() const {
    WriteIniString(L"Display", L"TextColor", textColor);
    WriteIniInt(L"Display", L"WindowPosX", windowPos.x);
    WriteIniInt(L"Display", L"WindowPosY", windowPos.y);
    WriteIniInt(L"Display", L"FontSize", fontSize);
    WriteIniString(L"Display", L"CurrentSymbol", EncodeSymbol(currentSymbol));
    WriteIniString(L"Display", L"OtherSymbol", EncodeSymbol(otherSymbol));
    WriteIniString(L"Display", L"EmptySymbol", EncodeSymbol(emptySymbol));
    WriteIniInt(L"Display", L"CharSpacing", charSpacing);
    WriteIniString(L"Display", L"FontName", fontName);
    WriteIniInt(L"Display", L"PositionPreset", positionPreset);
}
