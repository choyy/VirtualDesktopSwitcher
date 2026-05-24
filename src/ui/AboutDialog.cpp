#include "AboutDialog.h"

#include <string>

#include "config.h"
#include "util/Lang.h"
#include "util/UpdateChecker.h"
#include "util/Utils.h"

namespace {

constexpr int IDC_AUTO_CHECK      = 201;
constexpr int IDC_CHECK_UPDATES   = 202;
constexpr int IDC_HOMEPAGE        = 102;
constexpr int IDC_VERSION         = 103;
constexpr int IDC_ABOUT_ICON      = 104;
constexpr int IDC_ABOUT_TITLE     = 105;
constexpr int IDC_STATIC_VERSION  = 106;
constexpr int IDC_STATIC_HOMEPAGE = 203;

struct AboutData {
    bool  autoCheckUpdates = false;
    HICON hIcon            = nullptr;
    HFONT hFont            = nullptr;
};

INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        auto *data = LParamToPtr<AboutData>(lp);
        SetWindowLongPtrW(hwnd, DWLP_USER, lp);

        auto *hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(GetParent(hwnd), GWLP_HINSTANCE)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)

        const RECT rcTitle   = GetChildRect(hwnd, IDC_ABOUT_TITLE);
        const RECT rcVersion = GetChildRect(hwnd, IDC_VERSION);
        const int  iconSize  = rcVersion.bottom - rcTitle.top;
        const RECT rcIcon    = GetChildRect(hwnd, IDC_ABOUT_ICON);
        SetWindowPos(GetDlgItem(hwnd, IDC_ABOUT_ICON), nullptr, rcIcon.left, rcTitle.top, iconSize, iconSize, SWP_NOZORDER);

        SetWindowTextW(hwnd, Lang::Get(L"About.Caption"));
        SetDlgItemTextW(hwnd, IDC_ABOUT_TITLE, Lang::Get(L"About.Title"));
        SetDlgItemTextW(hwnd, IDC_STATIC_HOMEPAGE, Lang::Get(L"About.LabelHomepage"));
        SetDlgItemTextW(hwnd, IDC_STATIC_VERSION, Lang::Get(L"About.Version"));
        SetDlgItemTextW(hwnd, IDC_AUTO_CHECK, Lang::Get(L"About.AutoCheck"));
        SetDlgItemTextW(hwnd, IDC_CHECK_UPDATES, Lang::Get(L"About.BtnCheckUpdates"));
        SetDlgItemTextW(hwnd, IDCANCEL, Lang::Get(L"About.BtnClose"));

        data->hIcon = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(101), IMAGE_ICON, iconSize, iconSize, LR_DEFAULTCOLOR));
        if (data->hIcon != nullptr) {
            SendDlgItemMessageW(hwnd, IDC_ABOUT_ICON, STM_SETIMAGE, IMAGE_ICON, PtrToLParam(data->hIcon));
        }

        data->hFont = CreateFontW(-MulDiv(14, static_cast<int>(GetDpiForWindow(hwnd)), 72), 0, 0, 0, FW_NORMAL,
                                  FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        if (data->hFont != nullptr) {
            SendDlgItemMessageW(hwnd, IDC_ABOUT_TITLE, WM_SETFONT, HandleToWParam(data->hFont), TRUE);
        }

        SetDlgItemTextW(hwnd, IDC_VERSION, Utf8ToWide(APP_VERSION).c_str());

        int labelLeft = GetChildRect(hwnd, IDC_STATIC_HOMEPAGE).left;
        int maxW      = MeasureMaxLabelWidth(hwnd, {IDC_STATIC_HOMEPAGE, IDC_STATIC_VERSION});
        int vx        = labelLeft + maxW + 4;
        MoveChildrenToX(hwnd, {IDC_HOMEPAGE, IDC_VERSION}, vx);

        if (data->autoCheckUpdates) {
            SendDlgItemMessageW(hwnd, IDC_AUTO_CHECK, BM_SETCHECK, BST_CHECKED, 0);
        }

        return TRUE;
    }

    case WM_COMMAND: {
        auto *data = GetWndUserData<AboutData>(hwnd, DWLP_USER);
        int   id   = LOWORD(wp);
        if (id == IDC_CHECK_UPDATES) {
            UpdateChecker::CheckAndDownload(hwnd);
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
    case WM_CTLCOLORSTATIC:
        return PtrToIntPtr(GetSysColorBrush(COLOR_WINDOW));

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
