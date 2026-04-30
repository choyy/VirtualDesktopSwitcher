#ifndef UTILS_H
#define UTILS_H

#include <windows.h>

inline int CalcScalePercent(int dpi) {
    return (dpi * 100 + 48) / 96;
}

inline int GetDpiScale() {
    HDC       hdc = GetDC(nullptr);
    const int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);
    return CalcScalePercent(dpi);
}

inline int GetWindowDpi(HWND hwnd) {
    HDC       hdc = GetDC(hwnd);
    const int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hwnd, hdc);
    return dpi;
}

inline int ScaleForDpi(int value, int dpi) {
    return value * CalcScalePercent(dpi) / 100;
}

inline HFONT CreateDefaultFont(int pointSize, int dpi, bool bold = false) {
    return CreateFontW(-MulDiv(pointSize, dpi, 72), 0, 0, 0,
                       bold ? FW_BOLD : FW_NORMAL,
                       FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

inline void ApplyFontToChildren(HWND hwnd, HFONT hFont, const wchar_t *propName) {
    HWND child = nullptr;
    while ((child = FindWindowExW(hwnd, child, nullptr, nullptr)) != nullptr) {
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
    if (propName != nullptr) {
        SetPropW(hwnd, propName, hFont);
    }
}

inline void CleanupWindowFont(HWND hwnd, const wchar_t *propName) {
    auto *hFont = reinterpret_cast<HFONT>(GetPropW(hwnd, propName)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (hFont != nullptr) {
        DeleteObject(hFont);
    }
    RemovePropW(hwnd, propName);
}

inline void CenterWindow(HWND hwnd) {
    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    RECT      r;
    GetWindowRect(hwnd, &r);
    SetWindowPos(hwnd, nullptr,
                 (sw - (r.right - r.left)) / 2,
                 (sh - (r.bottom - r.top)) / 2,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

inline int RunModalLoop(HWND hwnd) {
    MSG msg;
    while ((IsWindow(hwnd) != 0) && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

#endif
