#include "SettingsDialog.h"

#include <commctrl.h>

#include <array>
#include <string>
#include <vector>

#include "util/Utils.h"

namespace {

constexpr int IDC_CUR_SYM = 101;
constexpr int IDC_OTH_SYM = 102;
constexpr int IDC_FONT    = 103;
constexpr int IDC_SPACING = 104;
constexpr int IDC_SPIN    = 105;
constexpr int IDC_EMP_SYM = 106;

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
        SendMessageW(data->hCmb, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(&lf->lfFaceName[0])); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
    return 1;
}

void PopulateComboWithAllSystemFonts(HWND hCmb, const std::wstring &selectedFont) {
    SendMessageW(hCmb, CB_RESETCONTENT, 0, 0);
    FontEnumData data{.hCmb = hCmb};
    HDC          hdc = GetDC(nullptr);
    LOGFONTW     lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(hdc, &lf, FontEnumProc, reinterpret_cast<LPARAM>(&data), 0); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    ReleaseDC(nullptr, hdc);

    int cnt = static_cast<int>(SendMessageW(hCmb, CB_GETCOUNT, 0, 0));
    int sel = 0;
    for (int i = 0; i < cnt; ++i) {
        std::array<wchar_t, 256> buf{};
        SendMessageW(hCmb, CB_GETLBTEXT, i, reinterpret_cast<LPARAM>(buf.data())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
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
        SendMessageW(hCmb, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"更多字体...")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
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

        FillCombo(GetDlgItem(hwnd, IDC_CUR_SYM), kSymbolList.data(), kSymbolList.size(), data->result.currentSymbol, false, true);
        FillCombo(GetDlgItem(hwnd, IDC_OTH_SYM), kSymbolList.data(), kSymbolList.size(), data->result.otherSymbol, false, true);
        FillCombo(GetDlgItem(hwnd, IDC_EMP_SYM), kSymbolList.data(), kSymbolList.size(), data->result.emptySymbol, false, true);
        FillCombo(GetDlgItem(hwnd, IDC_FONT), kFontList.data(), kFontList.size(), data->result.fontName, true, false);

        SendDlgItemMessageW(hwnd, IDC_SPIN, UDM_SETRANGE, 0, MAKELPARAM(100, 0));
        SendDlgItemMessageW(hwnd, IDC_SPIN, UDM_SETPOS, 0, data->result.charSpacing);
        return TRUE;
    }

    case WM_COMMAND: {
        auto *data = GetWndUserData<DialogData>(hwnd, DWLP_USER);
        if (data == nullptr) {
            return TRUE;
        }
        int id   = LOWORD(wp);
        int code = HIWORD(wp);

        if (code == CBN_SELCHANGE && id >= IDC_CUR_SYM && id <= IDC_EMP_SYM) {
            HWND hCmb = reinterpret_cast<HWND>(lp); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
            int  sel  = static_cast<int>(SendMessageW(hCmb, CB_GETCURSEL, 0, 0));
            if (sel == CB_ERR) {
                return TRUE;
            }

            if (id != IDC_FONT) {
                if (sel >= 0 && static_cast<size_t>(sel) < kSymbolList.size()) {
                    auto &target = (id == IDC_CUR_SYM) ? data->result.currentSymbol : (id == IDC_OTH_SYM) ? data->result.otherSymbol
                                                                                                          : data->result.emptySymbol;
                    target       = kSymbolList.at(sel);
                }
            }
            if (data->preview) {
                data->preview(data->result);
            }
            return TRUE;
        }

        if (id == IDC_FONT && code == CBN_SELCHANGE) {
            HWND hCmb = reinterpret_cast<HWND>(lp); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
            int  sel  = static_cast<int>(SendMessageW(hCmb, CB_GETCURSEL, 0, 0));
            if (sel == CB_ERR) {
                return TRUE;
            }

            if (sel >= 0 && static_cast<size_t>(sel) < kFontList.size()) {
                data->result.fontName = kFontList.at(sel);
            } else {
                std::array<wchar_t, 256> buf{};
                SendMessageW(hCmb, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(buf.data())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                buf.back() = 0;
                if (_wcsicmp(buf.data(), L"更多字体...") == 0) {
                    PopulateComboWithAllSystemFonts(hCmb, data->result.fontName);
                } else {
                    data->result.fontName = buf.data();
                }
            }
            if (data->preview) {
                data->preview(data->result);
            }
            return TRUE;
        }

        if (code == CBN_EDITCHANGE && id >= IDC_CUR_SYM && id <= IDC_EMP_SYM) {
            std::array<wchar_t, 8> buf{};
            GetDlgItemTextW(hwnd, id, buf.data(), static_cast<int>(buf.size()));
            if (buf[0] != 0) {
                (id == IDC_CUR_SYM ? data->result.currentSymbol : id == IDC_OTH_SYM ? data->result.otherSymbol
                                                                                    : data->result.emptySymbol) = buf.data();
                if (data->preview) {
                    data->preview(data->result);
                }
            }
            return TRUE;
        }

        if (id == IDC_SPACING && code == EN_CHANGE && data->preview) {
            std::array<wchar_t, 16> buf{};
            GetDlgItemTextW(hwnd, IDC_SPACING, buf.data(), static_cast<int>(buf.size()));
            data->result.charSpacing = (_wtoi(buf.data()) < 0) ? 0 : _wtoi(buf.data());
            data->preview(data->result);
            return TRUE;
        }

        if (id == IDCANCEL) {
            EndDialog(hwnd, id);
            return TRUE;
        }

        if (id == IDOK) {
            std::array<wchar_t, 16> buf{};
            GetDlgItemTextW(hwnd, IDC_SPACING, buf.data(), static_cast<int>(buf.size()));
            data->result.charSpacing = (_wtoi(buf.data()) < 0) ? 0 : _wtoi(buf.data());
            data->result.accepted    = true;
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

SettingsDialog::Result SettingsDialog::Show(HWND parent, const Result &current,
                                            std::function<void(const Result &)> preview) {
    auto *hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)

    DialogData data;
    data.result  = current;
    data.preview = std::move(preview);

    DialogBoxParamW(hInst, MAKEINTRESOURCEW(1001), parent, SettingsDlgProc, PtrToLParam(&data));
    return data.result;
}
