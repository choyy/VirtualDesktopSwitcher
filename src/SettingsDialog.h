#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <windows.h>
#include <string>
#include <functional>

class SettingsDialog {
public:
struct Result {
    std::wstring currentSymbol;
    std::wstring otherSymbol;
    std::wstring emptySymbol;
    std::wstring fontName;
    int          charSpacing = 0;
    bool         accepted = false;
};

    using PreviewCallback = std::function<void(const Result&)>;

    static Result Show(HWND parent, const Result& current, PreviewCallback preview = nullptr);
};

#endif
