#include "AboutDialog.h"

#include <string>

#include "util/Lang.h"
#include "util/UpdateChecker.h"
#include "util/Utils.h"

namespace {

constexpr int IDC_AUTO_CHECK      = 201;
constexpr int IDC_CHECK_UPDATES   = 202;
constexpr int IDC_HOMEPAGE        = 102;
constexpr int IDC_VERSION         = 103;
constexpr int IDC_APP_ICON        = 104;
constexpr int IDC_APP_NAME        = 105;
constexpr int IDC_STATIC_HOMEPAGE = 203;

struct AboutData {
    bool  autoCheckUpdates = false;
    HWND  hHomepage        = nullptr;
    RECT  homepageRect     = {};
    HICON hIcon            = nullptr;
    HFONT hFont            = nullptr;
};

INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        auto *data = LParamToPtr<AboutData>(lp);
        SetWindowLongPtrW(hwnd, DWLP_USER, lp);

        auto *hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(GetParent(hwnd), GWLP_HINSTANCE)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)

        auto ChildRect = [&](int id) {
            RECT r{};
            GetWindowRect(GetDlgItem(hwnd, id), &r);
            MapWindowPoints(HWND_DESKTOP, hwnd, reinterpret_cast<LPPOINT>(&r), 2); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            return r;
        };
        const RECT rcTitle   = ChildRect(IDC_APP_NAME);
        const RECT rcVersion = ChildRect(IDC_VERSION);
        const int  iconSize  = rcVersion.bottom - rcTitle.top;
        const RECT rcIcon    = ChildRect(IDC_APP_ICON);
        SetWindowPos(GetDlgItem(hwnd, IDC_APP_ICON), nullptr, rcIcon.left, rcTitle.top, iconSize, iconSize, SWP_NOZORDER);

        SetWindowTextW(hwnd, Lang::Get(L"About.Caption"));
        SetDlgItemTextW(hwnd, IDC_APP_NAME, Lang::Get(L"About.Title"));
        SetDlgItemTextW(hwnd, IDC_STATIC_HOMEPAGE, Lang::Get(L"About.LabelHomepage"));
        SetDlgItemTextW(hwnd, IDC_AUTO_CHECK, Lang::Get(L"About.AutoCheck"));
        SetDlgItemTextW(hwnd, IDC_CHECK_UPDATES, Lang::Get(L"About.BtnCheckUpdates"));
        SetDlgItemTextW(hwnd, IDCANCEL, Lang::Get(L"About.BtnClose"));

        data->hIcon = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(101), IMAGE_ICON, iconSize, iconSize, LR_DEFAULTCOLOR));
        if (data->hIcon != nullptr) {
            SendDlgItemMessageW(hwnd, IDC_APP_ICON, STM_SETIMAGE, IMAGE_ICON, PtrToLParam(data->hIcon));
        }

        data->hFont = CreateFontW(-MulDiv(14, static_cast<int>(GetDpiForWindow(hwnd)), 72), 0, 0, 0, FW_NORMAL,
                                  FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        if (data->hFont != nullptr) {
            SendDlgItemMessageW(hwnd, IDC_APP_NAME, WM_SETFONT, HandleToWParam(data->hFont), TRUE);
        }

        std::wstring verStr = std::wstring(Lang::Get(L"About.VersionPrefix")) + Utf8ToWide(APP_VERSION);
        SetDlgItemTextW(hwnd, IDC_VERSION, verStr.c_str());

        if (data->autoCheckUpdates) {
            SendDlgItemMessageW(hwnd, IDC_AUTO_CHECK, BM_SETCHECK, BST_CHECKED, 0);
        }

        data->hHomepage = GetDlgItem(hwnd, IDC_HOMEPAGE);
        GetWindowRect(data->hHomepage, &data->homepageRect);
        MapWindowPoints(HWND_DESKTOP, hwnd, reinterpret_cast<LPPOINT>(&data->homepageRect), 2); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

        return TRUE;
    }

    case WM_COMMAND: {
        auto *data = GetWndUserData<AboutData>(hwnd, DWLP_USER);
        int   id   = LOWORD(wp);
        if (id == IDC_CHECK_UPDATES) {
            UpdateChecker::CheckAndDownload(hwnd);
            return TRUE;
        }
        if (id == IDC_HOMEPAGE && HIWORD(wp) == 0) {
            ShellExecuteW(hwnd, L"open",
                          L"https://github.com/choyy/VirtualDesktopSwitcher",
                          nullptr, nullptr, SW_SHOW);
            return TRUE;
        }
        if (id == IDOK || id == IDCANCEL) {
            if (data != nullptr) {
                data->autoCheckUpdates = SendDlgItemMessageW(hwnd, IDC_AUTO_CHECK, BM_GETCHECK, 0, 0) == BST_CHECKED;
            }
            EndDialog(hwnd, id);
            return TRUE;
        }
        return TRUE;
    }

    case WM_CTLCOLORDLG:
        return PtrToIntPtr(GetSysColorBrush(COLOR_WINDOW));

    case WM_CTLCOLORSTATIC: {
        auto *data = GetWndUserData<AboutData>(hwnd, DWLP_USER);
        if (data != nullptr && data->hHomepage != nullptr && reinterpret_cast<HWND>(lp) == data->hHomepage) { // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
            SetTextColor(reinterpret_cast<HDC>(wp), RGB(0, 102, 204));                                        // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
            SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);                                                // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
        }
        return PtrToIntPtr(GetSysColorBrush(COLOR_WINDOW));
    }

    case WM_SETCURSOR: {
        auto *data = GetWndUserData<AboutData>(hwnd, DWLP_USER);
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

bool AboutDialog::Show(HWND parent, bool currentAutoCheck) {
    auto     *hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
    AboutData data;
    data.autoCheckUpdates = currentAutoCheck;
    DialogBoxParamW(hInst, MAKEINTRESOURCEW(1000), parent, AboutDlgProc, PtrToLParam(&data));
    if (data.hIcon != nullptr) { DestroyIcon(data.hIcon); }
    if (data.hFont != nullptr) { DeleteObject(data.hFont); }
    return data.autoCheckUpdates;
}
