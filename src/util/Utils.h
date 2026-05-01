#pragma once

#include <windows.h>

#include <array>
#include <string>

constexpr size_t kMaxDesktops = 9;

// Predefined color palette (29 colors, supports gradient notation like "#FFA745_#FE869F")
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

COLORREF ParseColorString(const std::wstring &colorStr);

// Win32 pointer cast helpers — replace std::bit_cast with proper reinterpret_cast

template <typename T>
inline T *GetWndUserData(HWND hwnd, int index = GWLP_USERDATA) {
    return reinterpret_cast<T *>(GetWindowLongPtrW(hwnd, index)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

template <typename T>
inline void SetWndUserData(HWND hwnd, T *ptr, int index = GWLP_USERDATA) {
    SetWindowLongPtrW(hwnd, index, reinterpret_cast<LONG_PTR>(ptr)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

template <typename T>
inline LPARAM PtrToLParam(T *ptr) {
    return reinterpret_cast<LPARAM>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

template <typename T>
inline T *LParamToPtr(LPARAM lparam) {
    return reinterpret_cast<T *>(lparam); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

template <typename T>
inline WPARAM HandleToWParam(T handle) {
    return reinterpret_cast<WPARAM>(handle); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

template <typename T>
inline INT_PTR PtrToIntPtr(T value) {
    return reinterpret_cast<INT_PTR>(value); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

inline int CalcScalePercent(int dpi) {
    return (dpi * 100 + 48) / 96;
}

inline int GetDpiScale() {
    static int scale = [] {
        HDC       hdc = GetDC(nullptr);
        const int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(nullptr, hdc);
        return CalcScalePercent(dpi);
    }();
    return scale;
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

// Window activation helper
void         ActivateWindow(HWND hwnd);
std::wstring GetWindowTitle(HWND hwnd);

// String conversion
std::wstring Utf8ToWide(const std::string &str);

// Version comparison
bool IsNewerVersion(const std::string &remote, const std::string &local);

// Network download helpers
bool DownloadFile(const std::wstring &url, const std::wstring &dest);
void ShowDownloadFailedDialog(HWND parent);

// Retry with timeout — returns true if condition met within maxRetries
template <typename Pred>
inline bool WaitForCondition(Pred pred, int maxRetries, int intervalMs = 10) {
    for (int retry = 0; retry < maxRetries; ++retry) {
        if (pred()) { return true; }
        Sleep(intervalMs);
    }
    return false;
}
