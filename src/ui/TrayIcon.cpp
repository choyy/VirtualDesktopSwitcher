#include "TrayIcon.h"

#include <shlwapi.h>

#include <array>

#include "util/ConfigIni.h"
#include "util/Utils.h"

namespace {

constexpr auto kRegRunPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

constexpr UINT WM_TRAY_EXIT             = WM_USER + 3;
constexpr UINT WM_TRAY_TOGGLE_AUTOSTART = WM_USER + 4;
constexpr UINT WM_TRAY_EDIT_MODE        = WM_USER + 5;
constexpr UINT WM_TRAY_SETTINGS         = WM_USER + 6;
constexpr UINT WM_TRAY_ABOUT            = WM_USER + 7;
constexpr UINT WM_TRAY_RUNAS_ADMIN      = WM_USER + 8;

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
    for (int i = 0; i < kPositionPresetCount; ++i) {
        AppendMenuW(hPosMenu, MF_STRING | (m_activePositionPreset == i ? MF_CHECKED : 0),
                    CMD_POSITION_TOP_LEFT + i, kPositionLabels.at(i));
    }
    AppendMenuW(hPosMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hPosMenu, MF_STRING | (m_activePositionPreset == -1 ? MF_CHECKED : 0), CMD_POSITION_CUSTOM, L"自定义");
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hPosMenu), L"位置大小"); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

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

    bool runAsAdmin = ReadIniInt(L"General", L"RunAsAdmin", 0) != 0;
    AppendMenuW(m_hMenu, MF_STRING | (runAsAdmin ? MF_CHECKED : MF_UNCHECKED),
                WM_TRAY_RUNAS_ADMIN, L"以管理员启动");

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
        auto *mis = LParamToPtr<MEASUREITEMSTRUCT>(lParam);
        if (mis->CtlType == ODT_MENU) {
            NONCLIENTMETRICSW ncm = {.cbSize = sizeof(ncm)};
            SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
            HDC         hdc   = GetDC(hwnd);
            HFONT       hFont = CreateFontIndirectW(&ncm.lfMenuFont);
            HGDIOBJ     old   = SelectObject(hdc, hFont);
            TEXTMETRICW tm{};
            GetTextMetricsW(hdc, &tm);
            m_menuAveWidth = tm.tmAveCharWidth;
            SelectObject(hdc, old);
            DeleteObject(hFont);
            ReleaseDC(hwnd, hdc);
            m_dpi           = static_cast<int>(GetDpiForWindow(hwnd));
            mis->itemHeight = tm.tmHeight + ScaleForDpi(2, m_dpi);
            mis->itemWidth  = ScaleForDpi(100, m_dpi);
        }
    } else if (msg == WM_DRAWITEM) {
        auto *dis = LParamToPtr<DRAWITEMSTRUCT>(lParam);
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
        if (m_positionFn) { m_positionFn(m_activePositionPreset); }
    } else if (cmd == WM_TRAY_RUNAS_ADMIN) {
        bool wasOn = ReadIniInt(L"General", L"RunAsAdmin", 0) != 0;
        WriteIniInt(L"General", L"RunAsAdmin", wasOn ? 0 : 1);
        if (!wasOn && !IsAdminProcess()) {
            std::array<wchar_t, MAX_PATH> exePath{};
            GetModuleFileNameW(nullptr, exePath.data(), MAX_PATH);
            ShellExecuteW(nullptr, L"runas", exePath.data(), nullptr, nullptr, SW_SHOW);
            PostQuitMessage(0);
        }
    } else if (cmd == CMD_POSITION_CUSTOM) {
        m_activePositionPreset = -1;
        if (m_editModeFn) { m_editModeFn(); }
    } else if (cmd == WM_TRAY_SETTINGS) {
        if (m_settingsFn) { m_settingsFn(); }
    } else if (cmd == WM_TRAY_ABOUT) {
        if (m_aboutFn) { m_aboutFn(); }
    } else if (cmd >= CMD_COLOR_OPTIONS_BASE && cmd < CMD_COLOR_OPTIONS_BASE + static_cast<UINT>(kPredefinedColors.size())) {
        const int index = static_cast<int>(cmd - CMD_COLOR_OPTIONS_BASE);
        if (m_colorFn) { m_colorFn(kPredefinedColors.at(index)); }
    }
}

void TrayIcon::DrawColorSwatch(LPDRAWITEMSTRUCT dis) const {
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
        cr.right = cr.left + ScaleForDpi(GetSystemMetrics(SM_CXMENUCHECK), m_dpi);
        SetTextColor(dis->hDC, (isSel != 0) ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_MENUTEXT));
        SetBkMode(dis->hDC, TRANSPARENT);
        DrawTextW(dis->hDC, L"\u2713", -1, &cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Color swatch
    const int labelW = m_menuAveWidth * 3;
    const int pad    = ScaleForDpi(3, m_dpi);

    RECT cr = dis->rcItem;
    cr.left += labelW + pad;
    cr.top += ScaleForDpi(1, m_dpi);
    cr.bottom -= ScaleForDpi(1, m_dpi);
    cr.right = dis->rcItem.right - pad;

    DrawSwatchRect(dis->hDC, cr, kPredefinedColors.at(colorIndex));

    // Number label
    const std::wstring numStr = std::to_wstring(colorIndex + 1);
    RECT               nr     = dis->rcItem;
    nr.left += pad;
    nr.right = dis->rcItem.left + labelW;
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
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, kRegRunPath, 0, KEY_READ, &hKey);
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
    const LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, kRegRunPath, 0, KEY_WRITE, &hKey);
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
