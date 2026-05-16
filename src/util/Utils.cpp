#include "util/Utils.h"

#include <algorithm>

void ActivateWindow(HWND hwnd) {
    HWND        foreHwnd     = GetForegroundWindow();
    DWORD       foreThreadId = 0;
    const DWORD curThreadId  = GetCurrentThreadId();
    BOOL        attached     = FALSE;

    if (foreHwnd != nullptr) {
        foreThreadId = GetWindowThreadProcessId(foreHwnd, nullptr);
        if (foreThreadId != 0 && foreThreadId != curThreadId) {
            AttachThreadInput(curThreadId, foreThreadId, TRUE);
            attached = TRUE;
        }
    }

    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);

    if (attached != FALSE) {
        AttachThreadInput(curThreadId, foreThreadId, FALSE);
    }
}

std::wstring GetWindowTitle(HWND hwnd) {
    std::wstring buffer(256, L'\0');
    const int    length = GetWindowTextW(hwnd, buffer.data(), static_cast<int>(buffer.size()));
    if (length <= 0) {
        return {};
    }
    buffer.resize(length);
    return buffer;
}

std::wstring Utf8ToWide(const std::string &str) {
    if (str.empty()) { return {}; }
    const int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) { return {}; }
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), len);
    result.resize(len - 1);
    return result;
}

bool IsAdminProcess() {
    HANDLE hToken{};
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) == 0) { return false; }
    TOKEN_ELEVATION elev{};
    DWORD           size = sizeof(elev);
    bool            ok   = (GetTokenInformation(hToken, TokenElevation, &elev, size, &size) != 0) && elev.TokenIsElevated != 0;
    CloseHandle(hToken);
    return ok;
}

COLORREF ParseColorString(const std::wstring &colorStr) {
    if (colorStr.empty()) { return RGB(255, 255, 255); }

    if (colorStr[0] == L'#' && colorStr.size() >= 7) {
        wchar_t *end = nullptr;
        uint32_t hex = wcstoul(colorStr.c_str() + 1, &end, 16);
        return RGB((hex >> 16u) & 0xFFu, (hex >> 8u) & 0xFFu, hex & 0xFFu);
    }

    int            r = 255, g = 255, b = 255;
    const wchar_t *s    = colorStr.c_str();
    wchar_t       *next = nullptr;
    r                   = static_cast<int>(wcstoul(s, &next, 10));
    if ((next != nullptr) && *next == L',') {
        s = next + 1;
    } else {
        return RGB(r, g, b);
    }
    g = static_cast<int>(wcstoul(s, &next, 10));
    if ((next != nullptr) && *next == L',') {
        s = next + 1;
    } else {
        return RGB(r, g, b);
    }
    b = static_cast<int>(wcstoul(s, &next, 10));
    return RGB(r, g, b);
}

size_t ParseMultiColorString(const std::wstring &colorStr, COLORREF *outColors, size_t maxColors) {
    size_t count = 0;
    size_t start = 0;
    while (start < colorStr.size() && count < maxColors) {
        size_t end = colorStr.find(L'_', start);
        if (end == std::wstring::npos) {
            outColors[count++] = ParseColorString(colorStr.substr(start));
            break;
        }
        outColors[count++] = ParseColorString(colorStr.substr(start, end - start));
        start              = end + 1;
    }
    return count;
}

COLORREF InterpolateGradientColor(const COLORREF *colors, size_t colorCount, float t) {
    if (colorCount <= 1) { return colors[0]; }
    t          = std::clamp(t, 0.0f, 1.0f);
    int   segs = static_cast<int>(colorCount) - 1;
    float segT = t * static_cast<float>(segs);
    int   idx  = static_cast<int>(segT);
    if (idx >= segs) {
        idx  = segs - 1;
        segT = 1.0f;
    } else {
        segT -= static_cast<float>(idx);
    }
    float s = 1.0f - segT;
    return RGB(
        static_cast<int>(GetRValue(colors[idx]) * s + GetRValue(colors[idx + 1]) * segT),
        static_cast<int>(GetGValue(colors[idx]) * s + GetGValue(colors[idx + 1]) * segT),
        static_cast<int>(GetBValue(colors[idx]) * s + GetBValue(colors[idx + 1]) * segT));
}
