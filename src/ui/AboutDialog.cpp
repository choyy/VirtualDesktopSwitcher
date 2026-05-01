#include "AboutDialog.h"

#include <commctrl.h>

#include <array>
#include <bit>
#include <cstdlib>
#include <string>

namespace {

constexpr int IDC_AUTO_CHECK    = 201;
constexpr int IDC_CHECK_UPDATES = 202;

struct AboutData {
    AboutDialog::Result result;
    HWND                hHomepage    = nullptr;
    RECT                homepageRect = {};
};

std::wstring Utf8ToWide(const std::string &str) {
    if (str.empty()) { return {}; }
    const int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) { return {}; }
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), len);
    result.resize(len - 1);
    return result;
}

bool IsNewerVersion(const std::string &remote, const std::string &local) {
    size_t r_pos = 0, l_pos = 0;
    while (r_pos < remote.size() && l_pos < local.size()) {
        char   *re = nullptr, *le = nullptr;
        int32_t rn = strtol(remote.c_str() + r_pos, &re, 10);
        int32_t ln = strtol(local.c_str() + l_pos, &le, 10);

        if (rn > ln) { return true; }
        if (rn < ln) { return false; }

        r_pos = re - remote.c_str();
        l_pos = le - local.c_str();

        if (r_pos < remote.size() && remote[r_pos] == '.') { r_pos++; }
        if (l_pos < local.size() && local[l_pos] == '.') { l_pos++; }
    }
    return r_pos < remote.size();
}

INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto *data = std::bit_cast<AboutData *>(GetWindowLongPtrW(hwnd, DWLP_USER));

    switch (msg) {
    case WM_INITDIALOG: {
        SetWindowLongPtrW(hwnd, DWLP_USER, lp);
        data = std::bit_cast<AboutData *>(lp);

        auto *hInst = std::bit_cast<HINSTANCE>(GetWindowLongPtrW(GetParent(hwnd), GWLP_HINSTANCE));

        HICON hIcon = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(101), IMAGE_ICON, 200, 200, LR_DEFAULTCOLOR));
        if (hIcon != nullptr) {
            SendDlgItemMessageW(hwnd, 104, STM_SETICON, std::bit_cast<WPARAM>(hIcon), 0);
            SetPropW(hwnd, L"IC", static_cast<HANDLE>(hIcon));
        }

        HFONT hBoldFont = CreateFontW(-MulDiv(14, GetDpiForWindow(hwnd), 72), 0, 0, 0, FW_BOLD,
                                      FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        if (hBoldFont != nullptr) {
            SendDlgItemMessageW(hwnd, 105, WM_SETFONT, std::bit_cast<WPARAM>(hBoldFont), TRUE);
            SetPropW(hwnd, L"AB", hBoldFont);
        }

        std::wstring verStr = L"版本：" + Utf8ToWide(APP_VERSION);
        SetDlgItemTextW(hwnd, 103, verStr.c_str());

        if (data->result.autoCheckUpdates) {
            SendDlgItemMessageW(hwnd, IDC_AUTO_CHECK, BM_SETCHECK, BST_CHECKED, 0);
        }

        data->hHomepage = GetDlgItem(hwnd, 102);
        GetWindowRect(data->hHomepage, &data->homepageRect);
        MapWindowPoints(HWND_DESKTOP, hwnd, std::bit_cast<LPPOINT>(&data->homepageRect), 2);

        return TRUE;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDC_CHECK_UPDATES) {
            AboutDialog::CheckAndDownload(hwnd);
            return TRUE;
        }
        if (id == 102 && HIWORD(wp) == 0) {
            ShellExecuteW(hwnd, L"open",
                          L"https://github.com/choyy/VirtualDesktopSwitcher",
                          nullptr, nullptr, SW_SHOW);
            return TRUE;
        }
        if (id == IDOK || id == IDCANCEL) {
            if (data != nullptr) {
                data->result.autoCheckUpdates = SendDlgItemMessageW(hwnd, IDC_AUTO_CHECK, BM_GETCHECK, 0, 0) == BST_CHECKED;
                data->result.accepted         = TRUE;
            }
            if (HFONT hFont = static_cast<HFONT>(RemovePropW(hwnd, L"AB"))) {
                DeleteObject(hFont);
            }
            if (HICON hIC = static_cast<HICON>(RemovePropW(hwnd, L"IC"))) {
                DestroyIcon(hIC);
            }
            EndDialog(hwnd, id);
            return TRUE;
        }
        return TRUE;
    }

    case WM_CTLCOLORDLG:
        return std::bit_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW));

    case WM_CTLCOLORSTATIC: {
        SetBkMode(std::bit_cast<HDC>(wp), TRANSPARENT);
        if (data != nullptr && data->hHomepage != nullptr && std::bit_cast<HWND>(lp) == data->hHomepage) {
            SetTextColor(std::bit_cast<HDC>(wp), RGB(0, 102, 204));
        }
        return std::bit_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
    }

    case WM_SETCURSOR: {
        if (data != nullptr && data->hHomepage != nullptr) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            if (PtInRect(&data->homepageRect, pt) != 0) {
                SetCursor(LoadCursor(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        return FALSE;
    }
    default: break;
    }
    return FALSE;
}

} // namespace

AboutDialog::VersionCheckResult AboutDialog::CheckForNewerVersion() {
    VersionCheckResult            result;
    std::array<wchar_t, MAX_PATH> tempPath{};
    if (GetTempPathW(static_cast<DWORD>(tempPath.size()), tempPath.data()) == 0) {
        return result;
    }

    std::wstring dest = std::wstring(tempPath.data()) + L"vds_version.json";

    using URLDownloadToFileW_t = HRESULT(WINAPI *)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPUNKNOWN);
    HMODULE hUrlmon            = LoadLibraryW(L"urlmon.dll");
    if (hUrlmon == nullptr) { return result; }

    auto pDownload = std::bit_cast<URLDownloadToFileW_t>(GetProcAddress(hUrlmon, "URLDownloadToFileW"));
    if (pDownload == nullptr) {
        FreeLibrary(hUrlmon);
        return result;
    }

    HRESULT hr = pDownload(nullptr, L"https://api.github.com/repos/choyy/VirtualDesktopSwitcher/releases/latest", dest.c_str(), 0, nullptr);
    FreeLibrary(hUrlmon);
    if (FAILED(hr)) { return result; }

    HANDLE hFile = CreateFileW(dest.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        DeleteFileW(dest.c_str());
        return result;
    }

    DWORD       fileSize = GetFileSize(hFile, nullptr);
    std::string content;
    if (fileSize > 0) {
        content.resize(fileSize);
        DWORD bytesRead = 0;
        ReadFile(hFile, content.data(), fileSize, &bytesRead, nullptr);
        content.resize(bytesRead);
    }
    CloseHandle(hFile);
    DeleteFileW(dest.c_str());

    auto extractJsonString = [&content](const std::string &key) -> std::string {
        auto pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) { return {}; }
        pos = content.find('"', pos + key.size() + 3);
        if (pos == std::string::npos) { return {}; }
        auto start = pos + 1;
        auto end   = content.find('"', start);
        if (end == std::string::npos) { return {}; }
        return content.substr(start, end - start);
    };

    std::string latestTag = extractJsonString("tag_name");
    if (latestTag.empty()) { return result; }
    if (latestTag[0] == 'v') { latestTag = latestTag.substr(1); }

    result.downloadUrl = extractJsonString("browser_download_url");
    result.hasUpdate   = IsNewerVersion(latestTag, APP_VERSION);
    return result;
}

void AboutDialog::DownloadUpdate(HWND parent, const std::string &url) {
    std::wstring                  wideUrl = Utf8ToWide(url);
    std::array<wchar_t, MAX_PATH> userProfile{};
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile.data(), static_cast<DWORD>(userProfile.size())) == 0) {
        if (MessageBoxW(parent, L"下载失败，是否前往发布页手动下载？", L"错误", MB_YESNO | MB_ICONERROR) == IDYES) {
            ShellExecuteW(parent, L"open", L"https://github.com/choyy/VirtualDesktopSwitcher/releases", nullptr, nullptr, SW_SHOW);
        }
        return;
    }

    std::wstring dest = std::wstring(userProfile.data()) + L"\\Desktop\\VirtualDesktopSwitcher.exe";

    using URLDownloadToFileW_t = HRESULT(WINAPI *)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPUNKNOWN);
    HMODULE hUrlmon            = LoadLibraryW(L"urlmon.dll");
    if (hUrlmon == nullptr) {
        if (MessageBoxW(parent, L"下载失败，是否前往发布页手动下载？", L"错误", MB_YESNO | MB_ICONERROR) == IDYES) {
            ShellExecuteW(parent, L"open", L"https://github.com/choyy/VirtualDesktopSwitcher/releases", nullptr, nullptr, SW_SHOW);
        }
        return;
    }

    auto pDownload = std::bit_cast<URLDownloadToFileW_t>(GetProcAddress(hUrlmon, "URLDownloadToFileW"));
    if (pDownload == nullptr) {
        FreeLibrary(hUrlmon);
        return;
    }

    if (SUCCEEDED(pDownload(nullptr, wideUrl.c_str(), dest.c_str(), 0, nullptr))) {
        MessageBoxW(parent, (L"已下载到：\n" + dest).c_str(), L"下载完成", MB_OK | MB_ICONINFORMATION);
    } else {
        FreeLibrary(hUrlmon);
        if (MessageBoxW(parent, L"下载失败，是否前往发布页手动下载？", L"错误", MB_YESNO | MB_ICONERROR) == IDYES) {
            ShellExecuteW(parent, L"open", L"https://github.com/choyy/VirtualDesktopSwitcher/releases", nullptr, nullptr, SW_SHOW);
        }
        return;
    }
    FreeLibrary(hUrlmon);
}

void AboutDialog::CheckAndDownload(HWND parent, bool silentIfUpToDate) {
    auto result = CheckForNewerVersion();
    if (result.hasUpdate) {
        if (MessageBoxW(parent, L"发现新版本，是否立即下载？", L"发现更新", MB_YESNO | MB_ICONQUESTION) == IDYES && !result.downloadUrl.empty()) {
            DownloadUpdate(parent, result.downloadUrl);
        }
    } else if (!silentIfUpToDate) {
        MessageBoxW(parent, L"您已使用最新版本。", L"无需更新", MB_OK | MB_ICONINFORMATION);
    }
}

AboutDialog::Result AboutDialog::Show(HWND parent, bool currentAutoCheck) {
    auto     *hInst = std::bit_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    AboutData data;
    data.result.autoCheckUpdates = currentAutoCheck;
    DialogBoxParamW(hInst, MAKEINTRESOURCEW(1000), parent, AboutDlgProc, std::bit_cast<LPARAM>(&data));
    return data.result;
}
