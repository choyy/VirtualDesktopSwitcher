#pragma once

#include <windows.h>

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
            void (*preview)(const Result &, void *) = nullptr,
            void *previewCtx                        = nullptr);

} // namespace SettingsDialog
