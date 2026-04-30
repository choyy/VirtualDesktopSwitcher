#include "TrayIcon.h"

#include <shlwapi.h>

#include <array>

#include "util/ColorState.h"

namespace {

void DrawSwatchRect(HDC hdc, RECT rect, const std::wstring &hex) {
    const size_t usPos = hex.find(L'_');

    if (usPos != std::wstring::npos) {
        const COLORREF startCol = ParseColorString(hex.substr(0, usPos));
        const COLORREF endCol   = ParseColorString(hex.substr(usPos + 1));

        const int sw = rect.right - rect.left;
        const int rs = GetRValue(startCol);
        const int gs = GetGValue(startCol);
        const int bs = GetBValue(startCol);
        const int re = GetRValue(endCol);
        const int ge = GetGValue(endCol);
        const int be = GetBValue(endCol);

        for (int sx = 0; sx < sw; ++sx) {
            const float t  = static_cast<float>(sx) / static_cast<float>(sw);
            const int   rr = static_cast<int>(static_cast<float>(rs) + static_cast<float>(re - rs) * t);
            const int   gg = static_cast<int>(static_cast<float>(gs) + static_cast<float>(ge - gs) * t);
            const int   bb = static_cast<int>(static_cast<float>(bs) + static_cast<float>(be - bs) * t);

            HPEN hp   = CreatePen(PS_SOLID, 1, RGB(rr, gg, bb));
            HPEN oldP = static_cast<HPEN>(SelectObject(hdc, hp));
            MoveToEx(hdc, rect.left + sx, rect.top, nullptr);
            LineTo(hdc, rect.left + sx, rect.bottom);
            SelectObject(hdc, oldP);
            DeleteObject(hp);
        }
    } else {
        HBRUSH hb = CreateSolidBrush(ParseColorString(hex));
        FillRect(hdc, &rect, hb);
        DeleteObject(hb);
    }

    // Border
    HPEN    hp   = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
    HPEN    oldP = static_cast<HPEN>(SelectObject(hdc, hp));
    HGDIOBJ oldB = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(hp);
}

} // namespace

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

    HMENU hPosMenu = CreatePopupMenu();
    AppendMenuW(hPosMenu, MF_STRING | (m_activePositionPreset == 0 ? MF_CHECKED : 0), CMD_POSITION_TOP_LEFT, L"左上");
    AppendMenuW(hPosMenu, MF_STRING | (m_activePositionPreset == 1 ? MF_CHECKED : 0), CMD_POSITION_TOP_CENTER, L"中上");
    AppendMenuW(hPosMenu, MF_STRING | (m_activePositionPreset == 2 ? MF_CHECKED : 0), CMD_POSITION_TOP_RIGHT, L"右上");
    AppendMenuW(hPosMenu, MF_STRING | (m_activePositionPreset == 3 ? MF_CHECKED : 0), CMD_POSITION_BOTTOM_LEFT, L"左下");
    AppendMenuW(hPosMenu, MF_STRING | (m_activePositionPreset == 4 ? MF_CHECKED : 0), CMD_POSITION_BOTTOM_CENTER, L"中下");
    AppendMenuW(hPosMenu, MF_STRING | (m_activePositionPreset == 5 ? MF_CHECKED : 0), CMD_POSITION_BOTTOM_RIGHT, L"右下");
    AppendMenuW(hPosMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hPosMenu, MF_STRING | (m_editModeChecked ? MF_CHECKED : 0), CMD_POSITION_CUSTOM, L"自定义");
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hPosMenu), L"显示位置"); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hColorMenu = CreatePopupMenu();
    for (UINT i = 0; i < static_cast<UINT>(kPredefinedColors.size()); ++i) {
        MENUITEMINFOW mii = {};
        mii.cbSize        = sizeof(mii);
        mii.fMask         = MIIM_ID | MIIM_FTYPE;
        mii.fType         = MFT_OWNERDRAW;
        mii.wID           = CMD_COLOR_OPTIONS_BASE + i;
        InsertMenuItemW(hColorMenu, i, TRUE, &mii);
    }
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hColorMenu), L"颜色"); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    AppendMenuW(m_hMenu, MF_STRING, WM_TRAY_SETTINGS, L"设置...");
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(m_hMenu, MF_STRING | (m_autoStartEnabled ? MF_CHECKED : MF_UNCHECKED),
                WM_TRAY_TOGGLE_AUTOSTART, L"开机自启");
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_hMenu, MF_STRING, WM_TRAY_ABOUT, L"关于...");
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_hMenu, MF_STRING, WM_TRAY_EXIT, L"退出");
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

    UpdateTooltip(L"虚拟桌面切换器");

    if (Shell_NotifyIconW(NIM_ADD, &m_nid) == 0) {
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
        HandleCommand(wParam);
    } else if (msg == WM_MEASUREITEM) {
        auto *mis = reinterpret_cast<LPMEASUREITEMSTRUCT>(lParam); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
        if (mis->CtlType == ODT_MENU) {
            mis->itemHeight = 25;
            mis->itemWidth  = 160;
        }
    } else if (msg == WM_DRAWITEM) {
        auto *dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
        if (dis->CtlType == ODT_MENU) {
            DrawColorSwatch(dis);
        }
    }
}

void TrayIcon::HandleCommand(WPARAM wParam) {
    const UINT cmd = LOWORD(wParam);

    if (cmd == WM_TRAY_EXIT) {
        PostQuitMessage(0);
    } else if (cmd == WM_TRAY_TOGGLE_AUTOSTART) {
        SetAutoStart(!IsAutoStartEnabled());
        m_autoStartEnabled = IsAutoStartEnabled();
    } else if (cmd >= CMD_POSITION_BASE && cmd < CMD_POSITION_CUSTOM) {
        m_activePositionPreset = static_cast<int>(cmd - CMD_POSITION_BASE);
        m_editModeChecked      = false;
        if (m_positionFn != nullptr) {
            m_positionFn(m_activePositionPreset, m_positionCtx);
        }
    } else if (cmd == CMD_POSITION_CUSTOM) {
        m_editModeChecked      = !m_editModeChecked;
        m_activePositionPreset = -1;
        if (m_editModeFn != nullptr) {
            m_editModeFn(m_editModeCtx);
        }
    } else if (cmd == WM_TRAY_SETTINGS) {
        if (m_settingsFn != nullptr) {
            m_settingsFn(m_settingsCtx);
        }
    } else if (cmd == WM_TRAY_ABOUT) {
        if (m_aboutFn != nullptr) {
            m_aboutFn(m_aboutCtx);
        }
    } else if (cmd >= CMD_COLOR_OPTIONS_BASE && cmd < CMD_COLOR_OPTIONS_BASE + static_cast<UINT>(kPredefinedColors.size())) {
        const int index = static_cast<int>(cmd - CMD_COLOR_OPTIONS_BASE);
        if (m_colorFn != nullptr) {
            m_colorFn(kPredefinedColors.at(index), m_colorCtx);
        }
    }
}

void TrayIcon::DrawColorSwatch(LPDRAWITEMSTRUCT dis) {
    const int colorIndex = static_cast<int>(dis->itemID - CMD_COLOR_OPTIONS_BASE);
    if (colorIndex < 0 || static_cast<size_t>(colorIndex) >= kPredefinedColors.size()) {
        return;
    }

    const BOOL isSel = ((dis->itemState & ODS_SELECTED) != 0) ? TRUE : FALSE;
    const BOOL isChk = ((dis->itemState & ODS_CHECKED) != 0) ? TRUE : FALSE;

    // Background
    HBRUSH hBg = CreateSolidBrush((isSel != 0) ? GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_MENU));
    FillRect(dis->hDC, &dis->rcItem, hBg);
    DeleteObject(hBg);

    // Check mark
    if (isChk != 0) {
        RECT cr  = dis->rcItem;
        cr.right = cr.left + GetSystemMetrics(SM_CXMENUCHECK);
        SetTextColor(dis->hDC, (isSel != 0) ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_MENUTEXT));
        SetBkMode(dis->hDC, TRANSPARENT);
        DrawTextW(dis->hDC, L"\u2713", -1, &cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Color swatch
    constexpr int labelW  = 24;
    const int     totalW  = dis->rcItem.right - dis->rcItem.left;
    int           swatchW = (totalW - labelW) * 9 / 10;
    swatchW               = (std::max)(swatchW, 40);

    RECT cr = dis->rcItem;
    cr.left += labelW + 4;
    cr.top += 2;
    cr.bottom -= 2;
    cr.right = cr.left + swatchW;

    DrawSwatchRect(dis->hDC, cr, kPredefinedColors.at(colorIndex));

    // Number label
    const std::wstring numStr = std::to_wstring(colorIndex + 1);
    RECT               nr     = dis->rcItem;
    nr.right                  = nr.left + labelW;
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, (isSel != 0) ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_MENUTEXT));
    DrawTextW(dis->hDC, numStr.data(), -1, &nr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

bool TrayIcon::Reinitialize() {
    if (m_nid.hWnd != nullptr) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
    }
    return Shell_NotifyIconW(NIM_ADD, &m_nid) != 0;
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
                                                               reinterpret_cast<LPBYTE>(value.data()), &valueSize); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    RegCloseKey(hKey);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    std::wstring       registryPath(value.data());
    const std::wstring currentPath(exePath.data());

    if (registryPath.length() >= 2 && registryPath.front() == L'"' && registryPath.back() == L'"') {
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
        const std::wstring quotedPath = L"\"" + std::wstring(exePath.data()) + L"\"";
        RegSetValueExW(hKey, L"VirtualDesktopSwitcher", 0, REG_SZ,
                       reinterpret_cast<const BYTE *>(quotedPath.c_str()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                       static_cast<DWORD>((quotedPath.length() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, L"VirtualDesktopSwitcher");
    }

    RegCloseKey(hKey);
}
