#include "AboutDialog.h"
#include <commctrl.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <fstream>

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

#define IDC_AUTO_CHECK 201
#define IDC_CHECK_UPDATES 202

struct AboutData {
    AboutDialog::Result result;
    HWND hHomepage = nullptr;
    RECT homepageRect = {};
};

static LRESULT CALLBACK AboutWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* data = (AboutData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        auto* cs = (CREATESTRUCTW*)lp;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        data = (AboutData*)cs->lpCreateParams;

        HDC hdc = GetDC(hwnd);
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(hwnd, hdc);
        int s = (dpi * 100 + 48) / 96;
        auto S = [s](int v) { return v * s / 100; };

        int y = S(20);

        // Icon + Title
        HICON hIcon = LoadIconW(cs->hInstance, MAKEINTRESOURCEW(101));
        HWND hIconWnd = CreateWindowW(L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_ICON,
                                       S(20), y - S(2), S(32), S(32), hwnd, nullptr, cs->hInstance, nullptr);
        if (hIcon && hIconWnd) SendMessageW(hIconWnd, STM_SETICON, (WPARAM)hIcon, 0);
        HFONT hTitleFont = CreateFontW(-MulDiv(14, dpi, 72), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HWND hTitle = CreateWindowW(L"STATIC", L"Virtual Desktop Switcher", WS_CHILD | WS_VISIBLE,
                                    S(58), y + S(4), S(280), S(24), hwnd, nullptr, cs->hInstance, nullptr);
        if (hTitleFont && hTitle) SendMessageW(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);
        y += S(30);

        // Homepage
        CreateWindowW(L"STATIC", L"Homepage:", WS_CHILD | WS_VISIBLE,
                      S(20), y, S(70), S(18), hwnd, nullptr, cs->hInstance, nullptr);
        data->hHomepage = CreateWindowW(L"STATIC", L"github.com/choyy/VirtualDesktopSwitcher",
                                        WS_CHILD | WS_VISIBLE,
                                        S(90), y, S(250), S(18), hwnd, nullptr, cs->hInstance, nullptr);
        GetWindowRect(data->hHomepage, &data->homepageRect);
        MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&data->homepageRect, 2);
        y += S(26);

        // Version + Auto check (same line)
        wchar_t verBuf[64];
        swprintf_s(verBuf, L"Version: %hs", APP_VERSION);
        CreateWindowW(L"STATIC", verBuf, WS_CHILD | WS_VISIBLE,
                      S(20), y, S(115), S(18), hwnd, nullptr, cs->hInstance, nullptr);
        CreateWindowW(L"BUTTON", L"Auto check for updates",
                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                      S(130), y, S(200), S(20), hwnd, (HMENU)IDC_AUTO_CHECK, cs->hInstance, nullptr);
        if (data->result.autoCheckUpdates)
            SendMessageW(GetDlgItem(hwnd, IDC_AUTO_CHECK), BM_SETCHECK, BST_CHECKED, 0);
        y += S(30);

        // Buttons
        y += S(8);
        int cx = (S(320) - S(210)) / 2;
        CreateWindowW(L"BUTTON", L"Check for Updates", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      cx, y, S(130), S(26), hwnd, (HMENU)IDC_CHECK_UPDATES, cs->hInstance, nullptr);
        CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      cx + S(140), y, S(70), S(26), hwnd, (HMENU)IDCANCEL, cs->hInstance, nullptr);

        // Apply font to all controls
        HFONT hFont = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        if (hFont) {
            SetPropW(hwnd, L"AboutFont", hFont);
            HWND child = nullptr;
            while ((child = FindWindowExW(hwnd, child, nullptr, nullptr)) != nullptr)
                SendMessageW(child, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
        return 0;
    }

    case WM_NCDESTROY: {
        HFONT hFont = (HFONT)GetPropW(hwnd, L"AboutFont");
        if (hFont) DeleteObject(hFont);
        RemovePropW(hwnd, L"AboutFont");
        return 0;
    }

    case WM_COMMAND: {
        if (!data) break;
        int id = LOWORD(wp);

        if (id == IDC_CHECK_UPDATES) {
            auto checkResult = AboutDialog::CheckForNewerVersion();
            if (checkResult.hasUpdate) {
                int ret = MessageBoxW(hwnd, L"A new version is available. Download now?",
                                      L"Update Available", MB_YESNO | MB_ICONQUESTION);
                if (ret == IDYES && !checkResult.downloadUrl.empty()) {
                    wchar_t userProfile[MAX_PATH];
                    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
                        std::wstring dest = std::wstring(userProfile) + L"\\Desktop\\VirtualDesktopSwitcher_update.exe";

                        typedef HRESULT (WINAPI *URLDownloadToFileW_t)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPUNKNOWN);
                        HMODULE hUrlmon = LoadLibraryW(L"urlmon.dll");
                        if (hUrlmon) {
                            auto pDownload = (URLDownloadToFileW_t)GetProcAddress(hUrlmon, "URLDownloadToFileW");
                            if (pDownload) {
                                std::wstring url;
                                int len = MultiByteToWideChar(CP_UTF8, 0, checkResult.downloadUrl.c_str(), -1, nullptr, 0);
                                if (len > 0) {
                                    url.resize(len);
                                    MultiByteToWideChar(CP_UTF8, 0, checkResult.downloadUrl.c_str(), -1, url.data(), len);
                                }
                                if (SUCCEEDED(pDownload(nullptr, url.c_str(), dest.c_str(), 0, nullptr))) {
                                    std::wstring msg = L"Downloaded to:\n" + dest + L"\n\nRun the installer to update.";
                                    MessageBoxW(hwnd, msg.c_str(), L"Download Complete", MB_OK | MB_ICONINFORMATION);
                                } else {
                                    MessageBoxW(hwnd, L"Download failed.", L"Error", MB_OK | MB_ICONERROR);
                                }
                            }
                            FreeLibrary(hUrlmon);
                        }
                    }
                }
            } else {
                MessageBoxW(hwnd, L"You are running the latest version.",
                            L"No Update", MB_OK | MB_ICONINFORMATION);
            }
            return 0;
        }

        if (id == IDCANCEL) {
            data->result.autoCheckUpdates =
                SendMessageW(GetDlgItem(hwnd, IDC_AUTO_CHECK), BM_GETCHECK, 0, 0) == BST_CHECKED;
            data->result.accepted = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_CTLCOLORSTATIC: {
        if (data && data->hHomepage && (HWND)lp == data->hHomepage) {
            SetTextColor((HDC)wp, RGB(0, 102, 204));
            SetBkMode((HDC)wp, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }

    case WM_SETCURSOR: {
        if (data && data->hHomepage) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            if (PtInRect(&data->homepageRect, pt)) {
                SetCursor(LoadCursor(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    case WM_LBUTTONDOWN: {
        if (data && data->hHomepage) {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            if (PtInRect(&data->homepageRect, pt)) {
                ShellExecuteW(hwnd, L"open",
                    L"https://github.com/choyy/VirtualDesktopSwitcher",
                    nullptr, nullptr, SW_SHOW);
                return 0;
            }
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

AboutDialog::VersionCheckResult AboutDialog::CheckForNewerVersion() {
    VersionCheckResult result;

    wchar_t tempPath[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempPath) == 0) return result;
    std::wstring dest = std::wstring(tempPath) + L"vds_version.json";

    // Download latest release info from GitHub API
    typedef HRESULT (WINAPI *URLDownloadToFileW_t)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPUNKNOWN);
    HMODULE hUrlmon = LoadLibraryW(L"urlmon.dll");
    if (!hUrlmon) return result;
    auto pDownload = (URLDownloadToFileW_t)GetProcAddress(hUrlmon, "URLDownloadToFileW");
    if (!pDownload) { FreeLibrary(hUrlmon); return result; }

    HRESULT hr = pDownload(nullptr,
        L"https://api.github.com/repos/choyy/VirtualDesktopSwitcher/releases/latest",
        dest.c_str(), 0, nullptr);
    FreeLibrary(hUrlmon);
    if (FAILED(hr)) return result;

    // Read and parse JSON
    std::ifstream f(dest);
    if (!f.is_open()) { DeleteFileW(dest.c_str()); return result; }
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    DeleteFileW(dest.c_str());

    // Parse tag_name
    auto pos = content.find("\"tag_name\"");
    if (pos == std::string::npos) return result;
    pos = content.find('"', pos + 10);
    if (pos == std::string::npos) return result;
    auto start = pos + 1;
    auto end = content.find('"', start);
    if (end == std::string::npos) return result;
    std::string latestTag = content.substr(start, end - start);

    // Parse browser_download_url (first asset)
    pos = content.find("\"browser_download_url\"");
    if (pos != std::string::npos) {
        pos = content.find('"', pos + 22);
        if (pos != std::string::npos) {
            start = pos + 1;
            end = content.find('"', start);
            if (end != std::string::npos)
                result.downloadUrl = content.substr(start, end - start);
        }
    }

    // Strip leading 'v' if present
    if (!latestTag.empty() && latestTag[0] == 'v')
        latestTag = latestTag.substr(1);

    // Parse local version
    std::string localVerA = APP_VERSION;

    // Compare versions
    auto parseVer = [](const std::string& v) -> std::vector<int> {
        std::vector<int> parts;
        size_t start = 0, end;
        while ((end = v.find('.', start)) != std::string::npos) {
            parts.push_back(std::stoi(v.substr(start, end - start)));
            start = end + 1;
        }
        parts.push_back(std::stoi(v.substr(start)));
        return parts;
    };

    auto remote = parseVer(latestTag);
    auto local = parseVer(localVerA);

    for (size_t i = 0; i < remote.size() && i < local.size(); ++i) {
        if (remote[i] > local[i]) { result.hasUpdate = true; return result; }
        if (remote[i] < local[i]) return result;
    }
    if (remote.size() > local.size()) result.hasUpdate = true;
    return result;
}

AboutDialog::Result AboutDialog::Show(HWND parent, bool currentAutoCheck) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = AboutWndProc;
    wc.hInstance = hInst;
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = L"AboutDialogClass";
    RegisterClassW(&wc);

    HDC hdc = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);
    int s = (dpi * 100 + 48) / 96;
    auto S = [s](int v) { return v * s / 100; };

    AboutData data;
    data.result.autoCheckUpdates = currentAutoCheck;

    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"AboutDialogClass",
                                L"About Virtual Desktop Switcher",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                0, 0, S(360), S(195), parent, nullptr, hInst, &data);
    if (!hDlg) return {};

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    RECT r; GetWindowRect(hDlg, &r);
    SetWindowPos(hDlg, nullptr, (sw - (r.right - r.left)) / 2, (sh - (r.bottom - r.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hDlg, SW_SHOW);

    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyWindow(hDlg);
    return data.result;
}