#pragma once

#include <windows.h>

#include <string>

constexpr size_t kMaxDesktops = 9;

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

// Window activation helper
void         ActivateWindow(HWND hwnd);
std::wstring GetWindowTitle(HWND hwnd);

// String conversion
std::wstring Utf8ToWide(const std::string &str);

// COM RAII wrapper
class ComInitializer {
public:
    ComInitializer() : m_hr(CoInitialize(nullptr)) {}
    ~ComInitializer() { CoUninitialize(); }

    ComInitializer(const ComInitializer &)             = delete;
    ComInitializer &operator=(const ComInitializer &)  = delete;
    ComInitializer(ComInitializer &&)                  = delete;
    ComInitializer       &operator=(ComInitializer &&) = delete;
    [[nodiscard]] bool    Succeeded() const { return SUCCEEDED(m_hr); }
    [[nodiscard]] HRESULT Result() const { return m_hr; }

private:
    HRESULT m_hr;
};

// Network download helpers
bool DownloadFile(const std::wstring &url, const std::wstring &dest);
void ShowDownloadFailedDialog(HWND parent);
