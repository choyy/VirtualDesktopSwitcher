#include "TrayIcon.h"

#include <shlwapi.h>

#include <array>

#include "util/IndicatorConfig.h"

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
constexpr UINT WM_TRAY_ANIM_MODE        = WM_USER + 9;
constexpr UINT CMD_SHOW_MODE_BASE       = WM_USER + 300;
constexpr UINT CMD_SHOW_MODE_CUSTOM     = CMD_SHOW_MODE_BASE + static_cast<UINT>(ShowMode::Count);
constexpr UINT CMD_POSITION_BASE        = WM_USER + 200;
constexpr UINT CMD_POSITION_CUSTOM      = CMD_POSITION_BASE + static_cast<int>(PositionPreset::Count) + 1;

constexpr std::array kPositionLabels = {L"左上", L"中上", L"右上", L"中央", L"左下", L"中下", L"右下"};
constexpr std::array kShowModeLabels = {L"总是显示", L"总是隐藏", L"切换后显示1s", L"切换后显示3s"};

void DrawSwatchRect(HDC hdc, RECT rect, const std::wstring &hex) {
    std::array<COLORREF, 5> colors{};
    size_t                  colorCount = ParseMultiColorString(hex, colors.data(), 5);

    if (colorCount >= 2) {
        const int sw = rect.right - rect.left;
        const int sh = rect.bottom - rect.top;
        for (int sx = 0; sx < sw; ++sx) {
            COLORREF col = InterpolateGradientColor(colors.data(), colorCount, static_cast<float>(sx) / static_cast<float>(sw));
            for (int sy = 0; sy < sh; ++sy) {
                SetPixel(hdc, rect.left + sx, rect.top + sy, col);
            }
        }
    } else if (colorCount == 1) {
        HBRUSH hb = CreateSolidBrush(colors[0]);
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

bool RunSchtasks(const wchar_t *args) {
    std::array<wchar_t, MAX_PATH> sys32{};
    GetSystemDirectoryW(sys32.data(), static_cast<UINT>(sys32.size()));
    std::wstring        cmd = std::wstring(sys32.data()) + L"\\schtasks.exe " + args;
    PROCESS_INFORMATION pi{};
    STARTUPINFOW        si = {.cb = sizeof(si)};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)
        == 0) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}

void DeleteRunReg() {
    HKEY hKey{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegRunPath, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"VirtualDesktopSwitcher");
        RegCloseKey(hKey);
    }
}

bool IsAutoStartEnabled() {
    if (IsAdminProcess()) {
        return RunSchtasks(L"/query /tn \"VirtualDesktopSwitcher\"");
    }
    if (RunSchtasks(L"/query /tn \"VirtualDesktopSwitcher\"")) {
        return true;
    }
    HKEY hKey   = nullptr;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, kRegRunPath, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) { return false; }
    std::wstring                  currentPath = GetCurrentExePath();
    std::array<wchar_t, MAX_PATH> value{};
    auto                          valueSize = static_cast<DWORD>(sizeof(value));
    result                                  = RegQueryValueExW(hKey, L"VirtualDesktopSwitcher", nullptr, nullptr,
                                                               reinterpret_cast<LPBYTE>(value.data()), &valueSize); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    RegCloseKey(hKey);
    if (result != ERROR_SUCCESS) { return false; }
    std::wstring registryPath(value.data());
    if (registryPath.length() >= 2 && registryPath.front() == L'"' && registryPath.back() == L'"') {
        registryPath = registryPath.substr(1, registryPath.length() - 2);
    }
    return _wcsicmp(registryPath.c_str(), currentPath.c_str()) == 0;
}

void SetAutoStart(bool enable) {
    if (IsAdminProcess()) {
        if (enable) {
            std::wstring exePath = GetCurrentExePath();
            std::wstring args    = L"/create /tn \"VirtualDesktopSwitcher\" /tr \\\"";
            args += exePath;
            args += L"\\\" /sc onlogon /delay 0000:10 /rl highest /f";
            RunSchtasks(args.c_str());
            DeleteRunReg();
        } else {
            RunSchtasks(L"/delete /tn \"VirtualDesktopSwitcher\" /f");
            DeleteRunReg();
        }
        return;
    }
    HKEY       hKey   = nullptr;
    const LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, kRegRunPath, 0, KEY_WRITE, &hKey);
    if (result != ERROR_SUCCESS) { return; }
    if (enable) {
        std::wstring       exePath    = GetCurrentExePath();
        const std::wstring quotedPath = L"\"" + exePath + L"\"";
        RegSetValueExW(hKey, L"VirtualDesktopSwitcher", 0, REG_SZ,
                       reinterpret_cast<const BYTE *>(quotedPath.c_str()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                       static_cast<DWORD>((quotedPath.length() + 1) * sizeof(wchar_t)));
    } else {
        DeleteRunReg();
    }
    RegCloseKey(hKey);
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

    HMENU hShowMenu = CreatePopupMenu();
    int   curShow   = ReadIniInt(L"Display", L"ShowMode", 0);
    for (int i = 0; i < static_cast<int>(kShowModeLabels.size()); ++i) {
        AppendMenuW(hShowMenu, MF_STRING | (curShow == i ? MF_CHECKED : 0),
                    CMD_SHOW_MODE_BASE + i, kShowModeLabels.at(i));
    }
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hShowMenu), L"显示模式"); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    HMENU hPosMenu = CreatePopupMenu();
    for (int i = 0; i < static_cast<int>(PositionPreset::Count); ++i) {
        AppendMenuW(hPosMenu, MF_STRING | (m_activePositionPreset == i ? MF_CHECKED : 0),
                    CMD_POSITION_BASE + i, kPositionLabels.at(i));
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
    AppendMenuW(hColorMenu, MF_SEPARATOR, 0, nullptr);
    bool animOn = ReadIniInt(L"Display", L"AnimMode", 1) != 0;
    AppendMenuW(hColorMenu, MF_STRING | (animOn ? MF_CHECKED : 0), WM_TRAY_ANIM_MODE, L"色环呼吸");
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
    switch (msg) {
    case WM_TRAYICON:
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
        break;

    case WM_COMMAND:
        HandleCommand(wParam);
        break;

    case WM_MEASUREITEM: {
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
        break;
    }

    case WM_DRAWITEM: {
        auto *dis = LParamToPtr<DRAWITEMSTRUCT>(lParam);
        if (dis->CtlType == ODT_MENU) {
            DrawColorSwatch(dis);
        }
        break;
    }
    default:
        break;
    }
}

void TrayIcon::HandleCommand(WPARAM wParam) {
    const UINT cmd = LOWORD(wParam);

    switch (cmd) {
    case WM_TRAY_EXIT:
        DeleteRunReg();
        RunSchtasks(L"/delete /tn \"VirtualDesktopSwitcher\" /f");
        PostQuitMessage(0);
        break;

    case WM_TRAY_TOGGLE_AUTOSTART: {
        bool newState = !IsAutoStartEnabled();
        SetAutoStart(newState);
        m_autoStartEnabled = newState;
        break;
    }

    case WM_TRAY_RUNAS_ADMIN: {
        bool wasOn = ReadIniInt(L"General", L"RunAsAdmin", 0) != 0;
        bool nowOn = !wasOn;
        WriteIniInt(L"General", L"RunAsAdmin", nowOn ? 1 : 0);

        if (!nowOn && IsAdminProcess()) {
            RunSchtasks(L"/delete /tn \"VirtualDesktopSwitcher\" /f");
        }

        if (!wasOn && !IsAdminProcess()) {
            std::wstring exePath = GetCurrentExePath();
            ShellExecuteW(nullptr, L"runas", exePath.c_str(), nullptr, nullptr, SW_SHOW);
            PostQuitMessage(0);
        }
        break;
    }

    case CMD_POSITION_CUSTOM:
        m_activePositionPreset = -1;
        if (m_editModeFn) { m_editModeFn(); }
        break;

    case WM_TRAY_SETTINGS:
        if (m_settingsFn) { m_settingsFn(); }
        break;

    case WM_TRAY_ABOUT:
        if (m_aboutFn) { m_aboutFn(); }
        break;

    case WM_TRAY_ANIM_MODE: {
        bool nowOn = ReadIniInt(L"Display", L"AnimMode", 1) == 0;
        WriteIniInt(L"Display", L"AnimMode", nowOn ? 1 : 0);
        if (m_animModeFn) { m_animModeFn(nowOn); }
        break;
    }

    default:
        if (cmd >= CMD_SHOW_MODE_BASE && cmd < CMD_SHOW_MODE_CUSTOM) {
            int mode = static_cast<int>(cmd - CMD_SHOW_MODE_BASE);
            WriteIniInt(L"Display", L"ShowMode", mode);
            if (m_showModeFn) { m_showModeFn(mode); }
        } else if (cmd >= CMD_POSITION_BASE && cmd < CMD_POSITION_CUSTOM) {
            m_activePositionPreset = static_cast<int>(cmd - CMD_POSITION_BASE);
            if (m_positionFn) { m_positionFn(m_activePositionPreset); }
        } else if (cmd >= CMD_COLOR_OPTIONS_BASE && cmd < CMD_COLOR_OPTIONS_BASE + static_cast<UINT>(kPredefinedColors.size())) {
            const int index = static_cast<int>(cmd - CMD_COLOR_OPTIONS_BASE);
            if (m_colorFn) { m_colorFn(kPredefinedColors.at(index)); }
        }
        break;
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
