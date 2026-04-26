#define NOMINMAX
#include "SettingsDialog.h"

#include <commctrl.h>
#include <windowsx.h>

#include <array>
#include <bit>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "util/ConfigIni.h"
#include "util/Utils.h"

namespace {

constexpr int IDC_CUR_SYM = 101;
constexpr int IDC_OTH_SYM = 102;
constexpr int IDC_FONT    = 103;
constexpr int IDC_SPACING = 104;
constexpr int IDC_SPIN    = 105;
constexpr int IDC_EMP_SYM = 106;

const std::vector<std::wstring> &GetSymbolList() {
    static const std::vector<std::wstring> syms = {
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
    return syms;
}

const std::vector<std::wstring> &GetFontList() {
    static const std::vector<std::wstring> fonts = {
        L"Arial Black", L"Lucida Sans Unicode", L"MS Gothic",
        L"Segoe Script", L"Segoe UI Symbol"};
    return fonts;
}

struct FontEnumData {
    HWND                             hCmb;
    std::unordered_set<std::wstring> added;
};

int CALLBACK FontEnumProc(const LOGFONTW *lf, const TEXTMETRICW * /*tm*/,
                          DWORD /*fontType*/, LPARAM lParam) {
    auto *data = std::bit_cast<FontEnumData *>(lParam);
    if (lf->lfFaceName[0] != L'@' && !data->added.contains(&lf->lfFaceName[0])) {
        data->added.insert(&lf->lfFaceName[0]);
        SendMessageW(data->hCmb, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(&lf->lfFaceName[0])); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
    return 1;
}

void PopulateComboWithAllSystemFonts(HWND hCmb, const std::wstring &selectedFont) {
    SendMessageW(hCmb, CB_RESETCONTENT, 0, 0);
    FontEnumData data = {hCmb};
    HDC          hdc  = GetDC(nullptr);
    LOGFONTW     lf   = {};
    lf.lfCharSet      = DEFAULT_CHARSET;
    EnumFontFamiliesExW(hdc, &lf, FontEnumProc, std::bit_cast<LPARAM>(&data), 0);
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

struct DialogData {
    SettingsDialog::Result          result;
    SettingsDialog::PreviewCallback preview;
};

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto *data = std::bit_cast<DialogData *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto *cs = std::bit_cast<CREATESTRUCTW *>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, std::bit_cast<LONG_PTR>(cs->lpCreateParams));
        data = static_cast<DialogData *>(cs->lpCreateParams);

        int  dpi = GetWindowDpi(hwnd);
        auto S   = [dpi](int v) { return ScaleForDpi(v, dpi); };

        int y = S(20);

        // Helper to make a labeled combo box
        auto makeCombo = [&](const wchar_t *label, int ctrlId,
                             const std::vector<std::wstring> &items,
                             const std::wstring              &current,
                             bool extraFont, bool editable = false) {
            CreateWindowW(L"STATIC", label, WS_CHILD | WS_VISIBLE,
                          S(15), y + S(3), S(160), S(18),
                          hwnd, nullptr, cs->hInstance, nullptr);

            DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL;
            style |= static_cast<DWORD>(editable ? CBS_DROPDOWN : CBS_DROPDOWNLIST);

            HWND hCmb = CreateWindowW(L"COMBOBOX", nullptr, style,
                                      S(185), y, S(220), S(200),
                                      hwnd, std::bit_cast<HMENU>(static_cast<INT_PTR>(ctrlId)),
                                      cs->hInstance, nullptr);
            SendMessageW(hCmb, CB_SETMINVISIBLE, 10, 0);

            for (const auto &item : items) {
                SendMessageW(hCmb, CB_ADDSTRING, 0, std::bit_cast<LPARAM>(item.c_str()));
            }

            if (extraFont) {
                SendMessageW(hCmb, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"更多字体...")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            }

            if (editable) {
                SetWindowTextW(hCmb, current.c_str());
            } else {
                int sel = 0;
                for (size_t i = 0; i < items.size(); ++i) {
                    if (items[i] == current) {
                        sel = static_cast<int>(i);
                        break;
                    }
                }
                if (extraFont && (current.empty() || std::ranges::find(items, current) == items.end())) {
                    sel = static_cast<int>(items.size());
                }
                SendMessageW(hCmb, CB_SETCURSEL, sel, 0);
            }
            y += S(30);
        };

        makeCombo(L"当前桌面符号：", IDC_CUR_SYM, GetSymbolList(),
                  data->result.currentSymbol, false, true);
        makeCombo(L"非空桌面符号：", IDC_OTH_SYM, GetSymbolList(),
                  data->result.otherSymbol, false, true);
        makeCombo(L"空桌面符号：", IDC_EMP_SYM, GetSymbolList(),
                  data->result.emptySymbol, false, true);
        makeCombo(L"字体：", IDC_FONT, GetFontList(), data->result.fontName, true);

        // Character Spacing
        CreateWindowW(L"STATIC", L"字符间距：", WS_CHILD | WS_VISIBLE,
                      S(15), y + S(3), S(160), S(18),
                      hwnd, nullptr, cs->hInstance, nullptr);

        std::array<wchar_t, 16> spBuf{};
        std::wstring            spStr = std::to_wstring(data->result.charSpacing);
        CreateWindowW(L"EDIT", spStr.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_NUMBER,
                      S(185), y, S(50), S(22), hwnd,
                      std::bit_cast<HMENU>(static_cast<INT_PTR>(IDC_SPACING)),
                      cs->hInstance, nullptr);

        CreateWindowW(L"msctls_updown32", nullptr,
                      WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_AUTOBUDDY | UDS_ALIGNRIGHT | UDS_ARROWKEYS,
                      0, 0, 0, 0, hwnd,
                      std::bit_cast<HMENU>(static_cast<INT_PTR>(IDC_SPIN)),
                      cs->hInstance, nullptr);

        SendMessageW(GetDlgItem(hwnd, IDC_SPIN), UDM_SETRANGE, 0, MAKELPARAM(100, 0));
        y += S(35);

        // OK / Cancel buttons
        int cx = (S(440) - S(120)) / 2;
        CreateWindowW(L"BUTTON", L"确定",
                      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      cx, y, S(55), S(24), hwnd,
                      std::bit_cast<HMENU>(static_cast<INT_PTR>(IDOK)),
                      cs->hInstance, nullptr);
        CreateWindowW(L"BUTTON", L"取消",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      cx + S(65), y, S(55), S(24), hwnd,
                      std::bit_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)),
                      cs->hInstance, nullptr);

        HFONT hFont = CreateDefaultFont(9, dpi);
        if (hFont != nullptr) {
            ApplyFontToChildren(hwnd, hFont, L"SettingsFont");
        }
        return 0;
    }

    case WM_NCDESTROY:
        CleanupWindowFont(hwnd, L"SettingsFont");
        return 0;

    case WM_COMMAND: {
        if (data == nullptr) {
            break;
        }
        int id   = LOWORD(wp);
        int code = HIWORD(wp);

        if (code == CBN_SELCHANGE && id >= IDC_CUR_SYM && id <= IDC_EMP_SYM) {
            HWND hCmb = std::bit_cast<HWND>(lp);
            int  sel  = static_cast<int>(SendMessageW(hCmb, CB_GETCURSEL, 0, 0));
            if (sel == CB_ERR) {
                return 0;
            }

            if (id != IDC_FONT) {
                const auto &symbols = GetSymbolList();
                if (sel >= 0 && static_cast<size_t>(sel) < symbols.size()) {
                    auto &target = (id == IDC_CUR_SYM) ? data->result.currentSymbol : (id == IDC_OTH_SYM) ? data->result.otherSymbol
                                                                                                          : data->result.emptySymbol;
                    target       = symbols[sel];
                }
            }
            if (data->preview) {
                data->preview(data->result);
            }
            return 0;
        }

        if (id == IDC_FONT && code == CBN_SELCHANGE) {
            HWND hCmb = std::bit_cast<HWND>(lp);
            int  sel  = static_cast<int>(SendMessageW(hCmb, CB_GETCURSEL, 0, 0));
            if (sel == CB_ERR) { return 0; }

            const auto &fonts = GetFontList();
            if (sel >= 0 && static_cast<size_t>(sel) < fonts.size()) {
                data->result.fontName = fonts[sel];
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
            if (data->preview) { data->preview(data->result); }
            return 0;
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
            return 0;
        }

        if (id == IDC_SPACING && code == EN_CHANGE && data->preview) {
            std::array<wchar_t, 16> buf{};
            GetDlgItemTextW(hwnd, IDC_SPACING, buf.data(), static_cast<int>(buf.size()));
            data->result.charSpacing = (std::max)(0, _wtoi(buf.data()));
            data->preview(data->result);
            return 0;
        }

        switch (id) {
        case IDOK: {
            std::array<wchar_t, 16> buf{};
            GetDlgItemTextW(hwnd, IDC_SPACING, buf.data(), static_cast<int>(buf.size()));
            data->result.charSpacing = (std::max)(0, _wtoi(buf.data()));
            data->result.accepted    = true;
            DestroyWindow(hwnd);
            return 0;
        }
        case IDCANCEL:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;
    }

    case WM_CTLCOLORSTATIC:
        return std::bit_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

SettingsDialog::Result SettingsDialog::Show(HWND parent, const Result &current,
                                            PreviewCallback preview) {
    auto *hInst = std::bit_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));

    WNDCLASSW wc     = {};
    wc.lpfnWndProc   = SettingsWndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = L"SettingsDialogClass";
    RegisterClassW(&wc);

    int  dpi = GetDpiScale();
    auto S   = [dpi](int v) { return ScaleForDpi(v, dpi); };

    DialogData data;
    data.result  = current;
    data.preview = std::move(preview);

    // Override with INI values
    std::wstring iniCur = DecodeSymbol(ReadIniString(L"Display", L"CurrentSymbol",
                                                     EncodeSymbol(current.currentSymbol)));
    std::wstring iniOth = DecodeSymbol(ReadIniString(L"Display", L"OtherSymbol",
                                                     EncodeSymbol(current.otherSymbol)));
    std::wstring iniEmp = DecodeSymbol(ReadIniString(L"Display", L"EmptySymbol",
                                                     EncodeSymbol(current.emptySymbol)));
    if (iniCur.size() == 1) { data.result.currentSymbol = iniCur; }
    if (iniOth.size() == 1) { data.result.otherSymbol = iniOth; }
    if (iniEmp.size() == 1) { data.result.emptySymbol = iniEmp; }

    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"SettingsDialogClass",
                                L"桌面指示器设置",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                0, 0, S(460), S(245), parent, nullptr, hInst, &data);
    if (hDlg == nullptr) { return current; }

    CenterWindow(hDlg);
    ShowWindow(hDlg, SW_SHOW);
    RunModalLoop(hDlg);
    DestroyWindow(hDlg);
    return data.result;
}
