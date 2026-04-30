#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <windows.h>

#include <string>

class SettingsDialog {
public:
    struct Result {
        std::wstring currentSymbol;
        std::wstring otherSymbol;
        std::wstring emptySymbol;
        std::wstring fontName;
        int          charSpacing = 0;
        bool         accepted    = false;
    };

    static Result Show(HWND parent, const Result &current,
                       void (*preview)(const Result &, void *) = nullptr,
                       void *previewCtx                        = nullptr);
};

#endif
