#include "TrayIcon.h"
#include "ColorParser.h"
#include "ColorState.h"

#include <array>

#include <shlwapi.h>

TrayIcon::~TrayIcon() {
    if (m_nid.hWnd != nullptr) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
    }
    if (m_hMenu != nullptr) {
        DestroyMenu(m_hMenu);
    }
}

void TrayIcon::BuildMenu() {
    if (m_hMenu != nullptr) {
        DestroyMenu(m_hMenu);
    }
    m_hMenu = CreatePopupMenu();

    AppendMenuW(m_hMenu, MF_STRING | (m_editModeChecked ? MF_CHECKED : MF_UNCHECKED),
                WM_TRAY_EDIT_MODE, L"Edit Mode");
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hColorMenu = CreatePopupMenu();
    const auto& colors = GetPredefinedColors();
    for (size_t i = 0; i < colors.size(); i++) {
        wchar_t numStr[8];
        swprintf_s(numStr, L"%zu", i + 1);
        MENUITEMINFOW mii = {};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STRING | MIIM_ID | MIIM_FTYPE;
        mii.fType = MFT_STRING | MFT_OWNERDRAW;
        mii.wID = CMD_COLOR_OPTIONS_BASE + (UINT)i;
        mii.dwTypeData = numStr;
        InsertMenuItemW(hColorMenu, (UINT)i, TRUE, &mii);
    }
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hColorMenu, L"Color");

    AppendMenuW(m_hMenu, MF_STRING, WM_TRAY_SETTINGS, L"Settings...");
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(m_hMenu, MF_STRING | (m_autoStartEnabled ? MF_CHECKED : MF_UNCHECKED),
                WM_TRAY_TOGGLE_AUTOSTART, L"Auto Start");
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_hMenu, MF_STRING, WM_TRAY_EXIT, L"Exit");
}

bool TrayIcon::Initialize(HWND hwnd, HINSTANCE hInstance) {
    std::array<wchar_t, MAX_PATH> exePath{};
    GetModuleFileNameW(nullptr, exePath.data(), MAX_PATH);

    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(101));
    if (hIcon == nullptr) {
        hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    }

    m_nid.cbSize           = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd             = hwnd;
    m_nid.uID              = 1;
    m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon            = hIcon;

    UpdateTooltip(L"Virtual Desktop Switcher");

    if (Shell_NotifyIconW(NIM_ADD, &m_nid) == FALSE) {
        return false;
    }

    m_autoStartEnabled = IsAutoStartEnabled();

    BuildMenu();

    return true;
}

void TrayIcon::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);

            m_autoStartEnabled = IsAutoStartEnabled();
            BuildMenu();

            TrackPopupMenu(m_hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
            PostMessage(hwnd, WM_NULL, 0, 0);
        } else if (lParam == WM_LBUTTONDBLCLK) {
            PostMessage(hwnd, WM_COMMAND, WM_TRAY_TOGGLE_AUTOSTART, 0);
        }
    } else if (msg == WM_COMMAND) {
        if (LOWORD(wParam) == WM_TRAY_EXIT) {
            PostQuitMessage(0);
        } else if (LOWORD(wParam) == WM_TRAY_TOGGLE_AUTOSTART) {
            SetAutoStart(!IsAutoStartEnabled());
            m_autoStartEnabled = IsAutoStartEnabled();
        } else if (LOWORD(wParam) == WM_TRAY_EDIT_MODE) {
            m_editModeChecked = !m_editModeChecked;
            if (m_editModeCb) m_editModeCb();
        } else if (LOWORD(wParam) == WM_TRAY_SETTINGS) {
            if (m_settingsCb) m_settingsCb();
        } else if (LOWORD(wParam) >= CMD_COLOR_OPTIONS_BASE &&
                   LOWORD(wParam) < CMD_COLOR_OPTIONS_BASE + (UINT)GetPredefinedColors().size()) {
            int index = LOWORD(wParam) - CMD_COLOR_OPTIONS_BASE;
            if (index >= 0 && index < (int)GetPredefinedColors().size()) {
                if (m_colorCb) m_colorCb(GetPredefinedColors()[index].hexColor);
            }
        }
    } else if (msg == WM_MEASUREITEM) {
        auto *mis = reinterpret_cast<LPMEASUREITEMSTRUCT>(lParam);
        if (mis->CtlType == ODT_MENU) {
            mis->itemHeight = 25;
            mis->itemWidth = 160;
        }
    } else if (msg == WM_DRAWITEM) {
        auto *dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
        if (dis->CtlType != ODT_MENU) return;

        int colorIndex = dis->itemID - CMD_COLOR_OPTIONS_BASE;
        const auto& colors = GetPredefinedColors();
        if (colorIndex < 0 || colorIndex >= (int)colors.size()) return;

        const BOOL isSel = (dis->itemState & ODS_SELECTED) != 0;
        const BOOL isChk = (dis->itemState & ODS_CHECKED) != 0;

        HBRUSH hBg = CreateSolidBrush(isSel ? GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_MENU));
        FillRect(dis->hDC, &dis->rcItem, hBg);
        DeleteObject(hBg);

        if (isChk) {
            RECT cr = dis->rcItem;
            cr.right = cr.left + GetSystemMetrics(SM_CXMENUCHECK);
            SetTextColor(dis->hDC, isSel ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_MENUTEXT));
            SetBkMode(dis->hDC, TRANSPARENT);
            wchar_t chk[] = L"\u2713";
            DrawTextW(dis->hDC, chk, -1, &cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        int labelW = 24;
        RECT cr = dis->rcItem;
        int totalW = cr.right - cr.left;
        int swatchW = (totalW - labelW) * 9 / 10;
        if (swatchW < 40) swatchW = 40;
        cr.left += labelW + 4;
        cr.top += 2;
        cr.bottom -= 2;
        cr.right = cr.left + swatchW;

        {
            std::wstring hex = colors[colorIndex].hexColor;
            size_t usPos = hex.find(L'_');
            if (usPos != std::wstring::npos) {
                COLORREF startCol = ParseColorString(hex.substr(0, usPos));
                COLORREF endCol   = ParseColorString(hex.substr(usPos + 1));
                int sw = cr.right - cr.left;
                int rs = GetRValue(startCol), gs = GetGValue(startCol), bs = GetBValue(startCol);
                int re = GetRValue(endCol), ge = GetGValue(endCol), be = GetBValue(endCol);
                for (int sx = 0; sx < sw; ++sx) {
                    float t = (float)sx / sw;
                    int rr = (int)(rs + (re - rs) * t);
                    int gg = (int)(gs + (ge - gs) * t);
                    int bb = (int)(bs + (be - bs) * t);
                    HPEN hp = CreatePen(PS_SOLID, 1, RGB(rr, gg, bb));
                    HPEN oldP = (HPEN)SelectObject(dis->hDC, hp);
                    MoveToEx(dis->hDC, cr.left + sx, cr.top, nullptr);
                    LineTo(dis->hDC, cr.left + sx, cr.bottom);
                    SelectObject(dis->hDC, oldP);
                    DeleteObject(hp);
                }
            } else {
                COLORREF col = ParseColorString(hex);
                HBRUSH hb = CreateSolidBrush(col);
                RECT fr = cr;
                FillRect(dis->hDC, &fr, hb);
                DeleteObject(hb);
            }
            HPEN hp = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
            HPEN oldP = (HPEN)SelectObject(dis->hDC, hp);
            HGDIOBJ oldB = SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
            Rectangle(dis->hDC, cr.left, cr.top, cr.right, cr.bottom);
            SelectObject(dis->hDC, oldB);
            SelectObject(dis->hDC, oldP);
            DeleteObject(hp);
        }

        wchar_t ns[8];
        swprintf_s(ns, L"%d", colorIndex + 1);
        RECT nr = dis->rcItem;
        nr.right = nr.left + labelW;
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, isSel ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_MENUTEXT));
        DrawTextW(dis->hDC, ns, -1, &nr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
}

void TrayIcon::UpdateTooltip(const std::wstring &tooltip) {
    wcsncpy_s(m_nid.szTip, tooltip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

bool TrayIcon::IsAutoStartEnabled() {
    HKEY hKey   = nullptr;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
                                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    std::array<wchar_t, MAX_PATH> exePath{};
    GetModuleFileNameW(nullptr, exePath.data(), MAX_PATH);

    std::array<wchar_t, MAX_PATH> value{};
    auto                          valueSize = static_cast<DWORD>(sizeof(value));
    result                                  = RegQueryValueExW(hKey, L"VirtualDesktopSwitcher", nullptr, nullptr,
                                                               reinterpret_cast<LPBYTE>(value.data()), &valueSize);

    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        return false;
    }

    std::wstring registryPath(value.data());
    std::wstring currentPath(exePath.data());

    if (registryPath.length() >= 2 &&
        registryPath.front() == L'"' &&
        registryPath.back() == L'"') {
        registryPath = registryPath.substr(1, registryPath.length() - 2);
    }

    return _wcsicmp(registryPath.c_str(), currentPath.c_str()) == 0;
}

void TrayIcon::SetAutoStart(bool enable) {
    HKEY       hKey   = nullptr;
    const LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
                                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                      0, KEY_WRITE, &hKey);
    if (result != ERROR_SUCCESS) {
        return;
    }

    if (enable) {
        std::array<wchar_t, MAX_PATH> exePath{};
        GetModuleFileNameW(nullptr, exePath.data(), MAX_PATH);
        std::wstring quotedPath = L"\"" + std::wstring(exePath.data()) + L"\"";
        RegSetValueExW(hKey, L"VirtualDesktopSwitcher", 0, REG_SZ,
                       reinterpret_cast<const BYTE *>(quotedPath.c_str()),
                       static_cast<DWORD>((quotedPath.length() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, L"VirtualDesktopSwitcher");
    }

    RegCloseKey(hKey);
}
