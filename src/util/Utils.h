#pragma once

#include <windows.h>

#include <array>
#include <string>

// --- Constants ---

constexpr size_t kMaxDesktops = 9;

constexpr std::array kPredefinedColors = {
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

// --- Color Parsing ---

COLORREF ParseColorString(const std::wstring &colorStr);
size_t   ParseMultiColorString(const std::wstring &colorStr, COLORREF *outColors, size_t maxColors);
COLORREF InterpolateGradientColor(const COLORREF *colors, size_t colorCount, float t);

// --- HSV Color Space ---

void     RGBToHSV(COLORREF rgb, float &h, float &s, float &v);
COLORREF HSVToRGB(float h, float s, float v);

// --- CIE LCh Color Space ---

void RGBToLCh(COLORREF color, double &L, double &C, double &h);

// --- System & Window Helpers ---

inline std::wstring GetCurrentExePath() {
    std::array<wchar_t, MAX_PATH> buf{};
    GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    return buf.data();
}

bool         IsAdminProcess();
void         ActivateWindow(HWND hwnd);
std::wstring GetWindowTitle(HWND hwnd);

// --- Win32 Cast Helpers ---

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

// --- Dialog Layout Helpers ---

inline RECT GetChildRect(HWND parent, HWND hChild) {
    RECT rc{};
    if (hChild != nullptr) {
        GetWindowRect(hChild, &rc);
        MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<LPPOINT>(&rc), 2); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
    return rc;
}

inline RECT GetChildRect(HWND parent, int childId) {
    return GetChildRect(parent, GetDlgItem(parent, childId));
}

inline void MoveWindowTo(HWND hWnd, int x, int y) {
    SetWindowPos(hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

inline void MoveChildrenToX(HWND parent, std::initializer_list<int> ids, int x) {
    for (int id : ids) {
        HWND h = GetDlgItem(parent, id);
        if (h == nullptr) { continue; }
        RECT rc = GetChildRect(parent, h);
        MoveWindowTo(h, x, rc.top);
    }
}

int MeasureLabelWidth(HWND parent, int id);
int MeasureMaxLabelWidth(HWND parent, std::initializer_list<int> ids);
