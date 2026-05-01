#pragma once

#include <windows.h>

#include <functional>
#include <string>

namespace SettingsDialog {

struct Result {
    std::wstring currentSymbol;
    std::wstring otherSymbol;
    std::wstring emptySymbol;
    std::wstring fontName;
    int          charSpacing = 0;
    bool         accepted    = false;
};

Result Show(HWND parent, const Result &current,
            std::function<void(const Result &)> preview = nullptr);

} // namespace SettingsDialog
