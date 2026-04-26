#include "AboutDialog.h"

#include <commctrl.h>
#include <windowsx.h>

#include <array>
#include <bit>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "util/Utils.h"

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

namespace {

constexpr int IDC_AUTO_CHECK    = 201;
constexpr int IDC_CHECK_UPDATES = 202;

struct AboutData {
    AboutDialog::Result result;
    HWND                hHomepage    = nullptr;
    RECT                homepageRect = {};
};

std::wstring Utf8ToWide(const std::string &str) {
    if (str.empty()) {
        return {};
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) {
        return {};
    }
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), len);
    result.resize(len - 1); // Remove null terminator
    return result;
}

std::vector<int> ParseVersion(const std::string &ver) {
    std::vector<int>  parts;
    std::stringstream ss(ver);
    std::string       token;
    while (std::getline(ss, token, '.')) {
        parts.push_back(std::stoi(token));
    }
    return parts;
}

LRESULT CALLBACK AboutWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto *data = std::bit_cast<AboutData *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto *cs = std::bit_cast<CREATESTRUCTW *>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, std::bit_cast<LONG_PTR>(cs->lpCreateParams));
        data = static_cast<AboutData *>(cs->lpCreateParams);

        const int dpi = GetWindowDpi(hwnd);
        auto      S   = [dpi](int v) { return ScaleForDpi(v, dpi); };

        int y = S(20);

        // Icon + Title
        HICON hIcon    = LoadIconW(cs->hInstance, MAKEINTRESOURCEW(101));
        HWND  hIconWnd = CreateWindowW(L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_ICON,
                                       S(20), y - S(2), S(32), S(32), hwnd, nullptr,
                                       cs->hInstance, nullptr);
        if (hIcon != nullptr && hIconWnd != nullptr) {
            SendMessageW(hIconWnd, STM_SETICON, std::bit_cast<WPARAM>(hIcon), 0);
        }

        HFONT hTitleFont = CreateDefaultFont(14, dpi, true);
        HWND  hTitle     = CreateWindowW(L"STATIC", L"虚拟桌面切换器",
                                         WS_CHILD | WS_VISIBLE,
                                         S(58), y + S(4), S(280), S(24), hwnd, nullptr,
                                         cs->hInstance, nullptr);
        if (hTitleFont != nullptr && hTitle != nullptr) {
            SendMessageW(hTitle, WM_SETFONT, std::bit_cast<WPARAM>(hTitleFont), TRUE);
        }
        y += S(30);

        // Homepage link
        CreateWindowW(L"STATIC", L"主页：", WS_CHILD | WS_VISIBLE,
                      S(20), y, S(70), S(18), hwnd, nullptr, cs->hInstance, nullptr);
        data->hHomepage = CreateWindowW(L"STATIC", L"github.com/choyy/VirtualDesktopSwitcher",
                                        WS_CHILD | WS_VISIBLE,
                                        S(90), y, S(250), S(18), hwnd, nullptr,
                                        cs->hInstance, nullptr);
        GetWindowRect(data->hHomepage, &data->homepageRect);
        MapWindowPoints(HWND_DESKTOP, hwnd, std::bit_cast<LPPOINT>(&data->homepageRect), 2);
        y += S(26);

        // Version + Auto check
        std::array<wchar_t, 64> verBuf{};
        std::wstring            verStr = L"版本：" + Utf8ToWide(APP_VERSION);
        CreateWindowW(L"STATIC", verStr.c_str(), WS_CHILD | WS_VISIBLE,
                      S(20), y, S(115), S(18), hwnd, nullptr, cs->hInstance, nullptr);
        CreateWindowW(L"BUTTON", L"自动检查更新",
                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                      S(130), y, S(200), S(20), hwnd,
                      std::bit_cast<HMENU>(static_cast<INT_PTR>(IDC_AUTO_CHECK)),
                      cs->hInstance, nullptr);
        if (data->result.autoCheckUpdates) {
            SendMessageW(GetDlgItem(hwnd, IDC_AUTO_CHECK), BM_SETCHECK, BST_CHECKED, 0);
        }
        y += S(30);

        // Buttons
        y += S(8);
        int cx = (S(320) - S(210)) / 2;
        CreateWindowW(L"BUTTON", L"检查更新",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      cx, y, S(130), S(26), hwnd,
                      std::bit_cast<HMENU>(static_cast<INT_PTR>(IDC_CHECK_UPDATES)),
                      cs->hInstance, nullptr);
        CreateWindowW(L"BUTTON", L"关闭",
                      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      cx + S(140), y, S(70), S(26), hwnd,
                      std::bit_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)),
                      cs->hInstance, nullptr);

        HFONT hFont = CreateDefaultFont(9, dpi);
        if (hFont != nullptr) {
            ApplyFontToChildren(hwnd, hFont, L"AboutFont");
        }
        return 0;
    }

    case WM_NCDESTROY:
        CleanupWindowFont(hwnd, L"AboutFont");
        return 0;

    case WM_COMMAND: {
        if (data == nullptr) {
            break;
        }
        int id = LOWORD(wp);

        if (id == IDC_CHECK_UPDATES) {
            auto checkResult = AboutDialog::CheckForNewerVersion();
            if (checkResult.hasUpdate) {
                int ret = MessageBoxW(hwnd, L"发现新版本，是否立即下载？",
                                      L"发现更新", MB_YESNO | MB_ICONQUESTION);
                if (ret == IDYES && !checkResult.downloadUrl.empty()) {
                    AboutDialog::DownloadUpdate(hwnd, checkResult.downloadUrl);
                }
            } else {
                MessageBoxW(hwnd, L"您已使用最新版本。",
                            L"无需更新", MB_OK | MB_ICONINFORMATION);
            }
            return 0;
        }

        if (id == IDCANCEL) {
            data->result.autoCheckUpdates = SendMessageW(GetDlgItem(hwnd, IDC_AUTO_CHECK), BM_GETCHECK, 0, 0) == BST_CHECKED;
            data->result.accepted         = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_CTLCOLORSTATIC: {
        if ((data != nullptr) && (data->hHomepage != nullptr) && std::bit_cast<HWND>(lp) == data->hHomepage) {
            SetTextColor(std::bit_cast<HDC>(wp), RGB(0, 102, 204));
            SetBkMode(std::bit_cast<HDC>(wp), TRANSPARENT);
            return std::bit_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }
        return std::bit_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }

    case WM_SETCURSOR: {
        if ((data != nullptr) && (data->hHomepage != nullptr)) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            if (PtInRect(&data->homepageRect, pt) != 0) {
                SetCursor(LoadCursor(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    case WM_LBUTTONDOWN: {
        if ((data != nullptr) && (data->hHomepage != nullptr)) {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (PtInRect(&data->homepageRect, pt) != 0) {
                ShellExecuteW(hwnd, L"open",
                              L"https://github.com/choyy/VirtualDesktopSwitcher",
                              nullptr, nullptr, SW_SHOW);
                return 0;
            }
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

AboutDialog::VersionCheckResult AboutDialog::CheckForNewerVersion() {
    VersionCheckResult result;

    std::array<wchar_t, MAX_PATH> tempPath{};
    if (GetTempPathW(static_cast<DWORD>(tempPath.size()), tempPath.data()) == 0) {
        return result;
    }
    std::wstring dest = std::wstring(tempPath.data()) + L"vds_version.json";

    // Download latest release info from GitHub API
    using URLDownloadToFileW_t = HRESULT(WINAPI *)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPUNKNOWN);
    HMODULE hUrlmon            = LoadLibraryW(L"urlmon.dll");
    if (hUrlmon == nullptr) {
        return result;
    }

    auto pDownload = reinterpret_cast<URLDownloadToFileW_t>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        GetProcAddress(hUrlmon, "URLDownloadToFileW"));
    if (pDownload == nullptr) {
        FreeLibrary(hUrlmon);
        return result;
    }

    HRESULT hr = pDownload(nullptr,
                           L"https://api.github.com/repos/choyy/VirtualDesktopSwitcher/releases/latest",
                           dest.c_str(), 0, nullptr);
    FreeLibrary(hUrlmon);
    if (FAILED(hr)) {
        return result;
    }

    // Read and parse JSON
    std::ifstream f(dest);
    if (!f.is_open()) {
        DeleteFileW(dest.c_str());
        return result;
    }
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    f.close();
    DeleteFileW(dest.c_str());

    // Parse tag_name
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

    // Strip leading 'v'
    if (latestTag[0] == 'v') { latestTag = latestTag.substr(1); }

    result.downloadUrl = extractJsonString("browser_download_url");

    // Compare versions
    auto remote = ParseVersion(latestTag);
    auto local  = ParseVersion(APP_VERSION);

    for (size_t i = 0; i < remote.size() && i < local.size(); ++i) {
        if (remote[i] > local[i]) {
            result.hasUpdate = true;
            return result;
        }
        if (remote[i] < local[i]) { return result; }
    }
    if (remote.size() > local.size()) { result.hasUpdate = true; }
    return result;
}

void AboutDialog::DownloadUpdate(HWND parent, const std::string &url) {
    std::array<wchar_t, MAX_PATH> userProfile{};
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile.data(), static_cast<DWORD>(userProfile.size())) == 0) {
        return;
    }

    std::wstring dest    = std::wstring(userProfile.data()) + L"\\Desktop\\VirtualDesktopSwitcher.exe";
    std::wstring wideUrl = Utf8ToWide(url);

    using URLDownloadToFileW_t = HRESULT(WINAPI *)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPUNKNOWN);
    HMODULE hUrlmon            = LoadLibraryW(L"urlmon.dll");
    if (hUrlmon == nullptr) {
        MessageBoxW(parent, L"下载失败。", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    auto pDownload = reinterpret_cast<URLDownloadToFileW_t>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        GetProcAddress(hUrlmon, "URLDownloadToFileW"));
    if (pDownload == nullptr) {
        FreeLibrary(hUrlmon);
        MessageBoxW(parent, L"下载失败。", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    if (SUCCEEDED(pDownload(nullptr, wideUrl.c_str(), dest.c_str(), 0, nullptr))) {
        std::wstring msg = L"已下载到：\n" + dest;
        MessageBoxW(parent, msg.c_str(), L"下载完成", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(parent, L"下载失败。", L"错误", MB_OK | MB_ICONERROR);
    }

    FreeLibrary(hUrlmon);
}

AboutDialog::Result AboutDialog::Show(HWND parent, bool currentAutoCheck) {
    auto *hInst = std::bit_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));

    WNDCLASSW wc     = {};
    wc.lpfnWndProc   = AboutWndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = L"AboutDialogClass";
    RegisterClassW(&wc);

    int  dpi = GetDpiScale();
    auto S   = [dpi](int v) { return ScaleForDpi(v, dpi); };

    AboutData data;
    data.result.autoCheckUpdates = currentAutoCheck;

    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"AboutDialogClass",
                                L"关于 虚拟桌面切换器",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                0, 0, S(360), S(195), parent, nullptr, hInst, &data);
    if (hDlg == nullptr) { return {}; }

    CenterWindow(hDlg);
    ShowWindow(hDlg, SW_SHOW);
    RunModalLoop(hDlg);
    DestroyWindow(hDlg);
    return data.result;
}
