#include "util/Utils.h"

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

bool DownloadFile(const std::wstring &url, const std::wstring &dest) {
    using URLDownloadToFileW_t = HRESULT(WINAPI *)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPUNKNOWN);
    HMODULE hUrlmon            = LoadLibraryW(L"urlmon.dll");
    if (hUrlmon == nullptr) { return false; }

    auto pDownload = reinterpret_cast<URLDownloadToFileW_t>(GetProcAddress(hUrlmon, "URLDownloadToFileW")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (pDownload == nullptr) {
        FreeLibrary(hUrlmon);
        return false;
    }

    HRESULT hr = pDownload(nullptr, url.c_str(), dest.c_str(), 0, nullptr);
    FreeLibrary(hUrlmon);
    return SUCCEEDED(hr);
}

void ShowDownloadFailedDialog(HWND parent) {
    if (MessageBoxW(parent, L"下载失败，是否前往发布页手动下载？", L"错误", MB_YESNO | MB_ICONERROR) == IDYES) {
        ShellExecuteW(parent, L"open", L"https://github.com/choyy/VirtualDesktopSwitcher/releases", nullptr, nullptr, SW_SHOW);
    }
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
