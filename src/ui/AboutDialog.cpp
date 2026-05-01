#include "AboutDialog.h"

#include <commctrl.h>

#include <string>

#include "core/UpdateChecker.h"
#include "util/Utils.h"

namespace {

constexpr int IDC_AUTO_CHECK    = 201;
constexpr int IDC_CHECK_UPDATES = 202;

struct AboutData {
    AboutDialog::Result result;
    HWND                hHomepage    = nullptr;
    RECT                homepageRect = {};
};

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
    auto *data = GetWndUserData<AboutData>(hwnd, DWLP_USER);

    switch (msg) {
    case WM_INITDIALOG: {
        SetWindowLongPtrW(hwnd, DWLP_USER, lp);
        data = LParamToPtr<AboutData>(lp);

        auto *hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(GetParent(hwnd), GWLP_HINSTANCE)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)

        auto *hIcon = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(101), IMAGE_ICON, 200, 200, LR_DEFAULTCOLOR));
        if (hIcon != nullptr) {
            SendDlgItemMessageW(hwnd, 104, STM_SETICON, HandleToWParam(hIcon), 0);
        }

        HFONT hBoldFont = CreateFontW(-MulDiv(14, static_cast<int>(GetDpiForWindow(hwnd)), 72), 0, 0, 0, FW_NORMAL,
                                      FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        if (hBoldFont != nullptr) {
            SendDlgItemMessageW(hwnd, 105, WM_SETFONT, HandleToWParam(hBoldFont), TRUE);
        }

        std::wstring verStr = L"版本：" + Utf8ToWide(APP_VERSION);
        SetDlgItemTextW(hwnd, 103, verStr.c_str());

        if (data->result.autoCheckUpdates) {
            SendDlgItemMessageW(hwnd, IDC_AUTO_CHECK, BM_SETCHECK, BST_CHECKED, 0);
        }

        data->hHomepage = GetDlgItem(hwnd, 102);
        GetWindowRect(data->hHomepage, &data->homepageRect);
        MapWindowPoints(HWND_DESKTOP, hwnd, reinterpret_cast<LPPOINT>(&data->homepageRect), 2); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

        return TRUE;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDC_CHECK_UPDATES) {
            UpdateChecker::CheckAndDownload(hwnd);
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
            EndDialog(hwnd, id);
            return TRUE;
        }
        return TRUE;
    }

    case WM_CTLCOLORDLG:
        return PtrToIntPtr(GetSysColorBrush(COLOR_WINDOW));

    case WM_CTLCOLORSTATIC: {
        if (data != nullptr && data->hHomepage != nullptr && reinterpret_cast<HWND>(lp) == data->hHomepage) { // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
            SetTextColor(reinterpret_cast<HDC>(wp), RGB(0, 102, 204));                                        // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
            SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);                                                // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
        }
        return PtrToIntPtr(GetSysColorBrush(COLOR_WINDOW));
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

AboutDialog::Result AboutDialog::Show(HWND parent, bool currentAutoCheck) {
    auto     *hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
    AboutData data;
    data.result.autoCheckUpdates = currentAutoCheck;
    DialogBoxParamW(hInst, MAKEINTRESOURCEW(1000), parent, AboutDlgProc, PtrToLParam(&data));
    return data.result;
}
