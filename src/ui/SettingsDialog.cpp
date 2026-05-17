#include "SettingsDialog.h"

#include <commctrl.h>

#include <array>
#include <string>
#include <vector>

#include "util/Lang.h"
#include "util/Utils.h"

namespace {

constexpr int IDC_CUR_SYM    = 101;
constexpr int IDC_OTH_SYM    = 102;
constexpr int IDC_FONT       = 103;
constexpr int IDC_SPACING    = 104;
constexpr int IDC_SPIN       = 105;
constexpr int IDC_EMP_SYM    = 106;
constexpr int IDC_ST_CUR_SYM = 107;
constexpr int IDC_ST_OTH_SYM = 108;
constexpr int IDC_ST_EMP_SYM = 109;
constexpr int IDC_ST_FONT    = 110;
constexpr int IDC_ST_SPACING = 111;
constexpr int IDC_ST_MODKEY  = 112;
constexpr int IDC_MODKEY_ALT = 113;

constexpr std::array kModkeyIds = {
    IDC_MODKEY_ALT,
    IDC_MODKEY_ALT + 1,
    IDC_MODKEY_ALT + 2,
    IDC_MODKEY_ALT + 3,
};

constexpr std::array kSymbolList = {
    L"\u00B7",
    L"\u2022",
    L"\u2218",
    L"\u2588",
    L"\u258C",
    L"\u2591",
    L"\u25A0",
    L"\u25A1",
    L"\u25AA",
    L"\u25AB",
    L"\u25B2",
    L"\u25B3",
    L"\u25B6",
    L"\u25B7",
    L"\u25BC",
    L"\u25BD",
    L"\u25C0",
    L"\u25C1",
    L"\u25C6",
    L"\u25C7",
    L"\u25C9",
    L"\u25CB",
    L"\u25CC",
    L"\u25CE",
    L"\u25CF",
    L"\u25D8",
    L"\u25E6",
    L"\u25EF",
    L"\u25FB",
    L"\u25FC",
    L"\u2605",
    L"\u2606",
    L"\u2609",
    L"\u2688",
    L"\u2689",
    L"\u26AA",
    L"\u26AB",
    L"\u2716",
    L"\u2726",
    L"\u2727",
    L"\u273F",
    L"\u2740",
    L"\u2B1B",
    L"\u2B1C",
    L"\u2B1F",
    L"\u2B20",
    L"\u2B21",
    L"\u2B22",
    L"\u2B24",
    L"\u2B25",
    L"\u2B26",
    L"\U0001F7C8",
    L"\U0001F7C9",
};

constexpr std::array kFontList = {
    L"Arial Black", L"Lucida Sans Unicode", L"MS Gothic",
    L"Segoe Script", L"Segoe UI Symbol"};

struct FontEnumData {
    HWND                      hCmb;
    std::vector<std::wstring> added;
};

int CALLBACK FontEnumProc(const LOGFONTW *lf, const TEXTMETRICW * /*unused*/, DWORD /*unused*/, LPARAM lParam) {
    auto *data = LParamToPtr<FontEnumData>(lParam);
    bool  dup  = false;
    for (const auto &a : data->added) {
        if (a == &lf->lfFaceName[0]) {
            dup = true;
            break;
        }
    }
    if (lf->lfFaceName[0] != L'@' && !dup) {
        data->added.emplace_back(&lf->lfFaceName[0]);
        SendMessageW(data->hCmb, CB_ADDSTRING, 0, PtrToLParam(&lf->lfFaceName[0]));
    }
    return 1;
}

void PopulateComboWithAllSystemFonts(HWND hCmb, const std::wstring &selectedFont) {
    SendMessageW(hCmb, CB_RESETCONTENT, 0, 0);
    FontEnumData data{.hCmb = hCmb};
    HDC          hdc = GetDC(nullptr);
    LOGFONTW     lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(hdc, &lf, FontEnumProc, PtrToLParam(&data), 0);
    ReleaseDC(nullptr, hdc);

    int cnt = static_cast<int>(SendMessageW(hCmb, CB_GETCOUNT, 0, 0));
    int sel = 0;
    for (int i = 0; i < cnt; ++i) {
        std::array<wchar_t, 256> buf{};
        SendMessageW(hCmb, CB_GETLBTEXT, i, PtrToLParam(buf.data()));
        if (_wcsicmp(buf.data(), selectedFont.c_str()) == 0) {
            sel = i;
            break;
        }
    }
    SendMessageW(hCmb, CB_SETCURSEL, sel, 0);
    SendMessageW(hCmb, CB_SHOWDROPDOWN, TRUE, 0);
}

void FillCombo(HWND hCmb, const wchar_t *const *items, size_t count, const std::wstring &current, bool extraFont, bool editable) {
    for (size_t i = 0; i < count; ++i) {
        SendMessageW(hCmb, CB_ADDSTRING, 0, PtrToLParam(items[i]));
    }
    if (extraFont) {
        SendMessageW(hCmb, CB_ADDSTRING, 0, PtrToLParam(Lang::Get(L"Settings.MoreFonts")));
    }

    if (editable) {
        SetWindowTextW(hCmb, current.c_str());
    } else {
        int sel = 0;
        for (size_t i = 0; i < count; ++i) {
            if (items[i] == current) {
                sel = static_cast<int>(i);
                break;
            }
        }
        if (extraFont && current.empty()) {
            sel = static_cast<int>(count);
        }
        SendMessageW(hCmb, CB_SETCURSEL, sel, 0);
    }
}

struct DialogData {
    SettingsDialog::Result                              result;
    std::function<void(const SettingsDialog::Result &)> preview;
};

INT_PTR CALLBACK SettingsDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        auto *data = LParamToPtr<DialogData>(lp);
        SetWindowLongPtrW(hwnd, DWLP_USER, lp);

        SetWindowTextW(hwnd, Lang::Get(L"Settings.Caption"));
        SetDlgItemTextW(hwnd, IDC_ST_CUR_SYM, Lang::Get(L"Settings.LabelCurSym"));
        SetDlgItemTextW(hwnd, IDC_ST_OTH_SYM, Lang::Get(L"Settings.LabelOthSym"));
        SetDlgItemTextW(hwnd, IDC_ST_EMP_SYM, Lang::Get(L"Settings.LabelEmpSym"));
        SetDlgItemTextW(hwnd, IDC_ST_FONT, Lang::Get(L"Settings.LabelFont"));
        SetDlgItemTextW(hwnd, IDC_ST_SPACING, Lang::Get(L"Settings.LabelSpacing"));
        SetDlgItemTextW(hwnd, IDC_ST_MODKEY, Lang::Get(L"Settings.ModKey"));
        SetDlgItemTextW(hwnd, IDOK, Lang::Get(L"Settings.BtnOK"));
        SetDlgItemTextW(hwnd, IDCANCEL, Lang::Get(L"Settings.BtnCancel"));

        SendDlgItemMessageW(hwnd, kModkeyIds.at(data->result.modKey), BM_SETCHECK, BST_CHECKED, 0);

        FillCombo(GetDlgItem(hwnd, IDC_CUR_SYM), kSymbolList.data(), kSymbolList.size(), data->result.currentSymbol, false, true);
        FillCombo(GetDlgItem(hwnd, IDC_OTH_SYM), kSymbolList.data(), kSymbolList.size(), data->result.otherSymbol, false, true);
        FillCombo(GetDlgItem(hwnd, IDC_EMP_SYM), kSymbolList.data(), kSymbolList.size(), data->result.emptySymbol, false, true);
        FillCombo(GetDlgItem(hwnd, IDC_FONT), kFontList.data(), kFontList.size(), data->result.fontName, true, false);

        SendDlgItemMessageW(hwnd, IDC_SPIN, UDM_SETRANGE, 0, MAKELPARAM(100, 0));
        SendDlgItemMessageW(hwnd, IDC_SPIN, UDM_SETPOS, 0, data->result.charSpacing);

        int labelLeft = GetChildRect(hwnd, IDC_ST_CUR_SYM).left;
        int maxW      = MeasureLabelWidth(hwnd, IDC_ST_OTH_SYM);
        int vx        = labelLeft + maxW + 4;
        int dx        = vx - GetChildRect(hwnd, IDC_CUR_SYM).left;

        MoveChildrenToX(hwnd, {IDC_CUR_SYM, IDC_OTH_SYM, IDC_EMP_SYM, IDC_FONT, IDC_SPACING, IDC_MODKEY_ALT}, vx);
        if (dx != 0) {
            for (int id : {IDC_SPIN, IDC_MODKEY_ALT + 1, IDC_MODKEY_ALT + 2, IDC_MODKEY_ALT + 3}) {
                HWND h = GetDlgItem(hwnd, id);
                if (h == nullptr) { break; }
                RECT rc = GetChildRect(hwnd, h);
                MoveWindowTo(h, rc.left + dx, rc.top);
            }
        }

        int maxRight = GetChildRect(hwnd, IDC_CUR_SYM).right;

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        int newClientW = maxRight + 50;
        if (newClientW < rcClient.right) {
            int shrink = rcClient.right - newClientW;

            RECT rcOk     = GetChildRect(hwnd, IDOK);
            RECT rcCancel = GetChildRect(hwnd, IDCANCEL);
            MoveWindowTo(GetDlgItem(hwnd, IDOK), rcOk.left - shrink / 2, rcOk.top);
            MoveWindowTo(GetDlgItem(hwnd, IDCANCEL), rcCancel.left - shrink / 2, rcCancel.top);

            RECT rcDlg;
            GetWindowRect(hwnd, &rcDlg);
            int newW = newClientW + (rcDlg.right - rcDlg.left - rcClient.right);
            SetWindowPos(hwnd, nullptr, rcDlg.left + shrink / 2, rcDlg.top, newW, rcDlg.bottom - rcDlg.top, SWP_NOZORDER);
        }

        return TRUE;
    }

    case WM_COMMAND: {
        auto *data = GetWndUserData<DialogData>(hwnd, DWLP_USER);
        if (data == nullptr) {
            return TRUE;
        }
        int id   = LOWORD(wp);
        int code = HIWORD(wp);

        auto SymbolField = [&]() -> std::wstring & {
            if (id == IDC_CUR_SYM) { return data->result.currentSymbol; }
            if (id == IDC_OTH_SYM) { return data->result.otherSymbol; }
            return data->result.emptySymbol;
        };
        auto ApplyPreview = [&] { if (data->preview) { data->preview(data->result); } };
        auto ReadSpacing  = [&] { return static_cast<int>(GetDlgItemInt(hwnd, IDC_SPACING, nullptr, FALSE)); };

        switch (id) {
        case IDC_CUR_SYM:
        case IDC_OTH_SYM:
        case IDC_EMP_SYM:
            if (code == CBN_SELCHANGE) {
                HWND hCmb = reinterpret_cast<HWND>(lp); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
                int  sel  = static_cast<int>(SendMessageW(hCmb, CB_GETCURSEL, 0, 0));
                if (sel >= 0 && static_cast<size_t>(sel) < kSymbolList.size()) {
                    SymbolField() = kSymbolList.at(sel);
                    ApplyPreview();
                }
            } else if (code == CBN_EDITCHANGE) {
                std::array<wchar_t, 8> buf{};
                GetDlgItemTextW(hwnd, id, buf.data(), static_cast<int>(buf.size()));
                if (buf[0] != 0) {
                    SymbolField() = std::wstring(buf.data());
                    ApplyPreview();
                }
            }
            break;

        case IDC_FONT:
            if (code == CBN_SELCHANGE) {
                HWND hCmb = reinterpret_cast<HWND>(lp); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
                int  sel  = static_cast<int>(SendMessageW(hCmb, CB_GETCURSEL, 0, 0));
                if (sel == CB_ERR) { break; }

                if (static_cast<size_t>(sel) < kFontList.size()) {
                    data->result.fontName = kFontList.at(sel);
                } else {
                    std::array<wchar_t, 256> buf{};
                    SendMessageW(hCmb, CB_GETLBTEXT, sel, PtrToLParam(buf.data()));
                    if (wcscmp(buf.data(), Lang::Get(L"Settings.MoreFonts")) == 0) {
                        PopulateComboWithAllSystemFonts(hCmb, data->result.fontName);
                    } else {
                        data->result.fontName = buf.data();
                    }
                }
                ApplyPreview();
            }
            break;

        case IDC_SPACING:
            if (code == EN_CHANGE) {
                data->result.charSpacing = ReadSpacing();
                ApplyPreview();
            }
            break;

        case IDOK: {
            data->result.charSpacing = ReadSpacing();
            data->result.accepted    = true;
            for (size_t i = 0; i < kModkeyIds.size(); ++i) {
                if (SendDlgItemMessageW(hwnd, kModkeyIds.at(i), BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    data->result.modKey = static_cast<int>(i);
                    break;
                }
            }
            EndDialog(hwnd, id);
            break;
        }

        case IDCANCEL:
            EndDialog(hwnd, id);
            break;
        default:
            break;
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

SettingsDialog::Result SettingsDialog::Show(HWND parent, const Result &current,
                                            std::function<void(const Result &)> preview) {
    auto *hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)

    DialogData data;
    data.result  = current;
    data.preview = std::move(preview);

    DialogBoxParamW(hInst, MAKEINTRESOURCEW(1001), parent, SettingsDlgProc, PtrToLParam(&data));
    return data.result;
}
