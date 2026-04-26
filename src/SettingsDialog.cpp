#define NOMINMAX
#include "SettingsDialog.h"
#include "ConfigIni.h"
#include <commctrl.h>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <windowsx.h>

static const std::vector<std::wstring>& GetSymbolList() {
    static const std::vector<std::wstring> syms = {
        L"\u00B7", L"\u2022", L"\u2218", L"\u2588", L"\u258C", L"\u2591",
        L"\u25A0", L"\u25A1", L"\u25AA", L"\u25AB", L"\u25B2", L"\u25B3",
        L"\u25B6", L"\u25B7", L"\u25BC", L"\u25BD", L"\u25C0", L"\u25C1",
        L"\u25C6", L"\u25C7", L"\u25C9", L"\u25CB", L"\u25CC", L"\u25CE",
        L"\u25CF", L"\u25D8", L"\u25E6", L"\u25EF", L"\u25FB", L"\u25FC",
        L"\u2605", L"\u2606", L"\u2609", L"\u2688", L"\u2689", L"\u26AA",
        L"\u26AB", L"\u2716", L"\u2726", L"\u2727", L"\u273F", L"\u2740",
        L"\u2B1B", L"\u2B1C", L"\u2B1F", L"\u2B20", L"\u2B21", L"\u2B22",
        L"\u2B24", L"\u2B25", L"\u2B26", L"\U0001F7C8", L"\U0001F7C9",
    };
    return syms;
}

static std::vector<std::wstring> GetFontList() {
    return { L"Arial Black", L"Lucida Sans Unicode", L"MS Gothic",
             L"Segoe Script", L"Segoe UI Symbol" };
}

struct FontEnumData {
    HWND hCmb;
    std::unordered_set<std::wstring> added;
};

static int CALLBACK FontEnumProc(const LOGFONTW* lf, const TEXTMETRICW* tm, DWORD fontType, LPARAM lParam) {
    auto* data = (FontEnumData*)lParam;
    if (lf->lfFaceName[0] != L'@' && data->added.find(lf->lfFaceName) == data->added.end()) {
        data->added.insert(lf->lfFaceName);
        SendMessageW(data->hCmb, CB_ADDSTRING, 0, (LPARAM)lf->lfFaceName);
    }
    return 1;
}

static void PopulateComboWithAllSystemFonts(HWND hCmb, const std::wstring& selectedFont) {
    SendMessageW(hCmb, CB_RESETCONTENT, 0, 0);
    FontEnumData data = { hCmb };
    HDC hdc = GetDC(nullptr);
    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(hdc, &lf, FontEnumProc, (LPARAM)&data, 0);
    ReleaseDC(nullptr, hdc);
    int cnt = (int)SendMessageW(hCmb, CB_GETCOUNT, 0, 0);
    int sel = 0;
    for (int i = 0; i < cnt; ++i) {
        wchar_t buf[256];
        SendMessageW(hCmb, CB_GETLBTEXT, i, (LPARAM)buf);
        if (_wcsicmp(buf, selectedFont.c_str()) == 0) { sel = i; break; }
    }
    SendMessageW(hCmb, CB_SETCURSEL, sel, 0);
    SendMessageW(hCmb, CB_SHOWDROPDOWN, TRUE, 0);
}

struct DialogData {
    SettingsDialog::Result result;
    SettingsDialog::PreviewCallback preview;
};

#define IDC_CUR_SYM_LABEL 101
#define IDC_OTH_SYM_LABEL 102
#define IDC_FONT_LABEL    103

static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* data = (DialogData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        auto* cs = (CREATESTRUCTW*)lp;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        data = (DialogData*)cs->lpCreateParams;

        HDC hdc = GetDC(hwnd);
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(hwnd, hdc);
        int s = (dpi * 100 + 48) / 96;
        auto S = [s](int v) { return v * s / 100; };

        int y = S(15);

        auto makeCombo = [&](const wchar_t* label, int ctrlId, const std::vector<std::wstring>& items, const std::wstring& current, bool extraFont, bool editable = false) {
            CreateWindowW(L"STATIC", label, WS_CHILD | WS_VISIBLE, S(15), y + S(3), S(160), S(18), hwnd, nullptr, cs->hInstance, nullptr);
            DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL;
            style |= editable ? CBS_DROPDOWN : CBS_DROPDOWNLIST;
            HWND hCmb = CreateWindowW(L"COMBOBOX", nullptr,
                                       style,
                                       S(185), y, S(220), S(200), hwnd, (HMENU)ctrlId, cs->hInstance, nullptr);
            SendMessageW(hCmb, CB_SETMINVISIBLE, 10, 0);
            for (size_t i = 0; i < items.size(); ++i)
                SendMessageW(hCmb, CB_ADDSTRING, 0, (LPARAM)items[i].c_str());
            if (extraFont) {
                SendMessageW(hCmb, CB_ADDSTRING, 0, (LPARAM)L"More Fonts...");
            }
            if (editable) {
                SetWindowTextW(hCmb, current.c_str());
            } else {
                int sel = 0;
                for (size_t i = 0; i < items.size(); ++i)
                    if (items[i] == current) { sel = (int)i; break; }
                if (extraFont && (current.empty() || std::find(items.begin(), items.end(), current) == items.end()))
                    sel = (int)items.size();
                SendMessageW(hCmb, CB_SETCURSEL, sel, 0);
            }
            y += S(30);
        };

        makeCombo(L"Current Desktop Symbol:", 101, GetSymbolList(), data->result.currentSymbol, false, true);
        makeCombo(L"Other Desktop Symbol:", 102, GetSymbolList(), data->result.otherSymbol, false, true);
        makeCombo(L"Font:", 103, GetFontList(), data->result.fontName, true);

        // Spacing
        CreateWindowW(L"STATIC", L"Character Spacing:", WS_CHILD | WS_VISIBLE, S(15), y + S(3), S(160), S(18), hwnd, nullptr, cs->hInstance, nullptr);
        wchar_t spBuf[16];
        swprintf_s(spBuf, L"%d", data->result.charSpacing);
        HWND hEditSp = CreateWindowW(L"EDIT", spBuf, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_NUMBER,
                                     S(185), y, S(50), S(22), hwnd, (HMENU)104, cs->hInstance, nullptr);
        CreateWindowW(L"msctls_updown32", nullptr,
                      WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_AUTOBUDDY | UDS_ALIGNRIGHT | UDS_ARROWKEYS,
                      0, 0, 0, 0, hwnd, (HMENU)105, cs->hInstance, nullptr);
        // Set up-down range
        SendMessageW(GetDlgItem(hwnd, 105), UDM_SETRANGE, 0, MAKELPARAM(100, 0));
        y += S(35);

        int cx = (S(440) - S(120)) / 2;
        CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      cx, y, S(55), S(24), hwnd, (HMENU)IDOK, cs->hInstance, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      cx + S(65), y, S(55), S(24), hwnd, (HMENU)IDCANCEL, cs->hInstance, nullptr);

        // Set DPI-scaled font for all controls
        HFONT hDlgFont = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        if (hDlgFont) {
            SetPropW(hwnd, L"SettingsFont", hDlgFont);
            // Apply font to all child controls
            HWND child = nullptr;
            while ((child = FindWindowExW(hwnd, child, nullptr, nullptr)) != nullptr) {
                SendMessageW(child, WM_SETFONT, (WPARAM)hDlgFont, TRUE);
            }
        }
        return 0;
    }
    case WM_NCDESTROY: {
        HFONT hFont = (HFONT)GetPropW(hwnd, L"SettingsFont");
        if (hFont) DeleteObject(hFont);
        RemovePropW(hwnd, L"SettingsFont");
        return 0;
    }

    case WM_COMMAND: {
        if (!data) break;
        int id = LOWORD(wp);
        int code = HIWORD(wp);

        if (id >= 101 && id <= 103 && code == CBN_SELCHANGE) {
            HWND hCmb = (HWND)lp;
            int sel = (int)SendMessageW(hCmb, CB_GETCURSEL, 0, 0);
            if (sel == CB_ERR) return 0;

            if (id == 101) {
                if (sel >= 0 && sel < (int)GetSymbolList().size())
                    data->result.currentSymbol = GetSymbolList()[sel];
            } else if (id == 102) {
                if (sel >= 0 && sel < (int)GetSymbolList().size())
                    data->result.otherSymbol = GetSymbolList()[sel];
            } else if (id == 103) {
                auto fonts = GetFontList();
                if (sel >= 0 && sel < (int)fonts.size()) {
                    data->result.fontName = fonts[sel];
                } else {
                    wchar_t buf[256];
                    SendMessageW(hCmb, CB_GETLBTEXT, sel, (LPARAM)buf);
                    buf[255] = 0;
                    if (_wcsicmp(buf, L"More Fonts...") == 0) {
                        PopulateComboWithAllSystemFonts(hCmb, data->result.fontName);
                    } else {
                        data->result.fontName = buf;
                    }
                }
            }
            // Fire preview callback
            if (data->preview) data->preview(data->result);
            return 0;
        }

        if ((id == 101 || id == 102) && code == CBN_EDITCHANGE) {
            wchar_t buf[8];
            GetDlgItemTextW(hwnd, id, buf, 8);
            if (buf[0] != 0) {
                (id == 101 ? data->result.currentSymbol : data->result.otherSymbol) = buf;
                if (data->preview) data->preview(data->result);
            }
            return 0;
        }

        if (id == 104 && code == EN_CHANGE) {
            if (data->preview) {
                wchar_t buf[16];
                GetDlgItemTextW(hwnd, 104, buf, 16);
                data->result.charSpacing = (std::max)(0, _wtoi(buf));
                data->preview(data->result);
            }
            return 0;
        }

        switch (id) {
        case IDOK: {
            wchar_t buf[16];
            GetDlgItemTextW(hwnd, 104, buf, 16);
            data->result.charSpacing = (std::max)(0, _wtoi(buf));
            data->result.accepted = true;
            DestroyWindow(hwnd);
            return 0;
        }
        case IDCANCEL:
            DestroyWindow(hwnd);
            return 0;
        case 105:
            break;
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

SettingsDialog::Result SettingsDialog::Show(HWND parent, const Result& current, PreviewCallback preview) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = hInst;
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = L"SettingsDialogClass";
    RegisterClassW(&wc);

    HDC hdc = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);
    int s = (dpi * 100 + 48) / 96;
    auto S = [s](int v) { return v * s / 100; };

    DialogData data;
    data.result = current;
    // Override with INI values if Application didn't pass the right ones
    std::wstring iniCur = DecodeSymbol(ReadIniString(L"Display", L"CurrentSymbol", EncodeSymbol(current.currentSymbol)));
    std::wstring iniOth = DecodeSymbol(ReadIniString(L"Display", L"OtherSymbol", EncodeSymbol(current.otherSymbol)));
    if (!iniCur.empty()) data.result.currentSymbol = iniCur;
    if (!iniOth.empty()) data.result.otherSymbol = iniOth;
    // Validate both symbols are in the symbol list; fall back to defaults if not
    const auto& syms = GetSymbolList();
    if (std::find(syms.begin(), syms.end(), data.result.currentSymbol) == syms.end())
        data.result.currentSymbol = L"\u2609";
    if (std::find(syms.begin(), syms.end(), data.result.otherSymbol) == syms.end())
        data.result.otherSymbol = L"\u25CB";
    data.preview = preview;

    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"SettingsDialogClass", L"Desktop Indicator Settings",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                0, 0, S(460), S(270), parent, nullptr, hInst, &data);
    if (!hDlg) return current;

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
