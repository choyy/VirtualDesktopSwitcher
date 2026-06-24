#include "TrayIcon.h"

#include <array>

#include "core/IndicatorConfig.h"
#include "util/AutoStart.h"
#include "util/ConfigIni.h"
#include "util/Lang.h"
#include "util/Utils.h"

namespace {

constexpr UINT WM_TRAY_EXIT             = WM_USER + 3;
constexpr UINT WM_TRAY_TOGGLE_AUTOSTART = WM_USER + 4;
constexpr UINT WM_TRAY_SETTINGS         = WM_USER + 6;
constexpr UINT WM_TRAY_ABOUT            = WM_USER + 7;
constexpr UINT WM_TRAY_RUNAS_ADMIN      = WM_USER + 8;
constexpr UINT WM_TRAY_ANIM_MODE        = WM_USER + 9;
constexpr UINT WM_TRAY_TOGGLE_SHOW      = WM_USER + 10;
constexpr UINT WM_TRAY_RESET            = WM_USER + 11;
constexpr UINT WM_TRAY_AUTO_CONTRAST    = WM_USER + 12;
constexpr UINT WM_TRAY_AUTO_FOCUS       = WM_USER + 13;
constexpr UINT CMD_SHOW_MODE_BASE       = WM_USER + 300;
constexpr UINT CMD_SHOW_MODE_CUSTOM     = CMD_SHOW_MODE_BASE + static_cast<UINT>(ShowMode::Count);
constexpr UINT CMD_POSITION_BASE        = WM_USER + 200;
constexpr UINT CMD_POSITION_CUSTOM      = CMD_POSITION_BASE + static_cast<int>(PositionPreset::Count) + 1;
constexpr UINT CMD_DRAG_MODE_BASE       = WM_USER + 400;
constexpr UINT CMD_DRAG_MODE_CUSTOM     = CMD_DRAG_MODE_BASE + static_cast<UINT>(DragSwitchMode::Count);

constexpr std::array kShowModeKeys = {
    L"Menu.AlwaysShow",
    L"Menu.AlwaysHide",
    L"Menu.Show1s",
    L"Menu.Show3s",
};

constexpr std::array kPositionKeys = {
    L"Menu.TopLeft",
    L"Menu.TopCenter",
    L"Menu.TopRight",
    L"Menu.Center",
    L"Menu.BottomLeft",
    L"Menu.BottomCenter",
    L"Menu.BottomRight",
    L"Menu.EmbedTaskbarRight",
    L"Menu.EmbedTaskbarLeft",
};

constexpr std::array kDragModeKeys = {
    L"Menu.DragAlways",
    L"Menu.DragAlt",
    L"Menu.DragCtrl",
    L"Menu.DragNever",
};

void DrawSwatchRect(HDC hdc, RECT rect, const std::wstring &hex) {
    auto colors = ParseMultiColorString(hex);

    if (colors.count >= 2) {
        const int sw = rect.right - rect.left;
        const int sh = rect.bottom - rect.top;
        for (int sx = 0; sx < sw; ++sx) {
            COLORREF col = InterpolateGradientColor(colors.colors.data(), colors.count, static_cast<float>(sx) / static_cast<float>(sw));
            for (int sy = 0; sy < sh; ++sy) {
                SetPixel(hdc, rect.left + sx, rect.top + sy, col);
            }
        }
    } else if (colors.count == 1) {
        HBRUSH hb = CreateSolidBrush(colors.colors[0]);
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

    HMENU hDragMenu = CreatePopupMenu();
    int   curDrag   = ReadIniInt(L"Display", L"DragSwitchMode", 0);
    for (int i = 0; i < std::size(kDragModeKeys); ++i) {
        AppendMenuW(hDragMenu, MF_STRING | (curDrag == i ? MF_CHECKED : 0),
                    CMD_DRAG_MODE_BASE + i, Lang::Get(kDragModeKeys.at(i)));
    }
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hDragMenu), Lang::Get(L"Menu.DragSwitchMode")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    HMENU hShowMenu = CreatePopupMenu();
    int   curShow   = ReadIniInt(L"Display", L"ShowMode", 0);
    for (int i = 0; i < std::size(kShowModeKeys); ++i) {
        AppendMenuW(hShowMenu, MF_STRING | (curShow == i ? MF_CHECKED : 0),
                    CMD_SHOW_MODE_BASE + i, Lang::Get(kShowModeKeys.at(i)));
    }
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hShowMenu), Lang::Get(L"Menu.ShowMode")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    HMENU hPosMenu = CreatePopupMenu();
    for (int i = 0; i < static_cast<int>(PositionPreset::Count); ++i) {
        AppendMenuW(hPosMenu, MF_STRING | (static_cast<int>(m_activePositionPreset) == i ? MF_CHECKED : 0),
                    CMD_POSITION_BASE + i, Lang::Get(kPositionKeys.at(i)));
    }
    AppendMenuW(hPosMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hPosMenu, MF_STRING | (m_activePositionPreset == PositionPreset::Custom ? MF_CHECKED : 0), CMD_POSITION_CUSTOM, Lang::Get(L"Menu.Custom"));
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hPosMenu), Lang::Get(L"Menu.PositionSize")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

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
    AppendMenuW(hColorMenu, MF_STRING | (animOn ? MF_CHECKED : 0), WM_TRAY_ANIM_MODE, Lang::Get(L"Menu.BreathingRing"));
    bool autoContrastOn = ReadIniInt(L"Display", L"AutoContrast", 1) != 0;
    AppendMenuW(hColorMenu, MF_STRING | (autoContrastOn ? MF_CHECKED : 0), WM_TRAY_AUTO_CONTRAST, Lang::Get(L"Menu.AutoContrast"));
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hColorMenu), Lang::Get(L"Menu.Color")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    AppendMenuW(m_hMenu, MF_STRING, WM_TRAY_SETTINGS, Lang::Get(L"Menu.Settings"));
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hLangMenu = CreatePopupMenu();
    AppendMenuW(hLangMenu, MF_STRING | (Lang::Current() == LangType::Chinese ? MF_CHECKED : 0), WM_TRAY_LANG_CHINESE, Lang::Get(L"Menu.LangChinese"));
    AppendMenuW(hLangMenu, MF_STRING | (Lang::Current() == LangType::English ? MF_CHECKED : 0), WM_TRAY_LANG_ENGLISH, Lang::Get(L"Menu.LangEnglish"));
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hLangMenu), Lang::Get(L"Menu.Language")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    bool autoFocusOn = ReadIniInt(L"Display", L"AutoFocus", 1) != 0;
    AppendMenuW(m_hMenu, MF_STRING | (autoFocusOn ? MF_CHECKED : 0),
                WM_TRAY_AUTO_FOCUS, Lang::Get(L"Menu.AutoFocus"));

    bool runAsAdmin = ReadIniInt(L"General", L"RunAsAdmin", 0) != 0;
    AppendMenuW(m_hMenu, MF_STRING | (runAsAdmin ? MF_CHECKED : MF_UNCHECKED),
                WM_TRAY_RUNAS_ADMIN, Lang::Get(L"Menu.RunAsAdmin"));

    AppendMenuW(m_hMenu, MF_STRING | (m_autoStartEnabled ? MF_CHECKED : MF_UNCHECKED),
                WM_TRAY_TOGGLE_AUTOSTART, Lang::Get(L"Menu.AutoStart"));

    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_hMenu, MF_STRING, WM_TRAY_RESET, Lang::Get(L"Menu.Reset"));
    AppendMenuW(m_hMenu, MF_STRING, WM_TRAY_ABOUT, Lang::Get(L"Menu.About"));
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_hMenu, MF_STRING, WM_TRAY_EXIT, Lang::Get(L"Menu.Exit"));
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

    UpdateTooltip(Lang::Get(L"Tray.DefaultTip"));

    if (Shell_NotifyIconW(NIM_ADD, &m_nid) == 0) {
        return false;
    }

    m_autoStartEnabled = AutoStart::IsEnabled();
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

            m_autoStartEnabled = AutoStart::IsEnabled();
            BuildMenu();

            TrackPopupMenu(m_hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
            PostMessage(hwnd, WM_NULL, 0, 0);
        } else if (lParam == WM_LBUTTONDBLCLK) {
            PostMessage(hwnd, WM_COMMAND, WM_TRAY_TOGGLE_SHOW, 0);
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
            mis->itemHeight = tm.tmHeight + MulDiv(2, m_dpi, 96);
            mis->itemWidth  = MulDiv(100, m_dpi, 96);
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

void TrayIcon::HandleExit() {
    AutoStart::RemoveAll();
    PostQuitMessage(0);
}

void TrayIcon::HandleToggleAutoStart() {
    bool newState = !AutoStart::IsEnabled();
    AutoStart::SetEnabled(newState);
    m_autoStartEnabled = newState;
}

void TrayIcon::HandleRunAsAdmin() {
    bool wasOn = ReadIniInt(L"General", L"RunAsAdmin", 0) != 0;
    bool nowOn = !wasOn;
    WriteIniInt(L"General", L"RunAsAdmin", nowOn ? 1 : 0);

    if (!nowOn && IsAdminProcess()) {
        AutoStart::RemoveTask();
    }

    if (!wasOn && !IsAdminProcess()) {
        std::wstring exePath = GetCurrentExePath();
        ShellExecuteW(nullptr, L"runas", exePath.c_str(), nullptr, nullptr, SW_SHOW);
        PostQuitMessage(0);
    }
}

void TrayIcon::HandleReset() {
    DeleteFileW((GetAppDataDir() + L"\\settings.ini").c_str());
    AutoStart::RemoveAll();
    ShellExecuteW(nullptr, L"open", GetCurrentExePath().c_str(), nullptr, nullptr, SW_SHOW);
    PostQuitMessage(0);
}

void TrayIcon::HandleAnimMode() {
    bool nowOn = ReadIniInt(L"Display", L"AnimMode", 1) == 0;
    WriteIniInt(L"Display", L"AnimMode", nowOn ? 1 : 0);
    if (m_animModeFn) { m_animModeFn(nowOn); }
}

void TrayIcon::HandleAutoContrast() {
    bool nowOn = ReadIniInt(L"Display", L"AutoContrast", 0) == 0;
    WriteIniInt(L"Display", L"AutoContrast", nowOn ? 1 : 0);
    if (m_autoContrastFn) { m_autoContrastFn(nowOn); }
}

void TrayIcon::HandleDragSwitchModeCommand(int mode) {
    WriteIniInt(L"Display", L"DragSwitchMode", mode);
    if (m_dragModeFn) { m_dragModeFn(mode); }
}

void TrayIcon::HandleAutoFocus() {
    bool nowOn = ReadIniInt(L"Display", L"AutoFocus", 1) == 0;
    WriteIniInt(L"Display", L"AutoFocus", nowOn ? 1 : 0);
    if (m_autoFocusFn) { m_autoFocusFn(nowOn); }
}

void TrayIcon::HandleToggleShow() {
    int mode    = ReadIniInt(L"Display", L"ShowMode", 0);
    int newMode = (mode == static_cast<int>(ShowMode::AlwaysHide))
                      ? static_cast<int>(ShowMode::AlwaysShow)
                      : static_cast<int>(ShowMode::AlwaysHide);
    WriteIniInt(L"Display", L"ShowMode", newMode);
    if (m_showModeFn) { m_showModeFn(newMode); }
}

void TrayIcon::HandleShowModeCommand(int mode) {
    WriteIniInt(L"Display", L"ShowMode", mode);
    if (m_showModeFn) { m_showModeFn(mode); }
}

void TrayIcon::HandlePositionCommand(PositionPreset preset) {
    m_activePositionPreset = preset;
    if (m_positionFn) { m_positionFn(preset); }
}

void TrayIcon::HandleColorCommand(int index) {
    if (m_colorFn) { m_colorFn(kPredefinedColors.at(index)); }
}

void TrayIcon::HandleCommand(WPARAM wParam) {
    const UINT cmd = LOWORD(wParam);

    if (cmd >= CMD_SHOW_MODE_BASE && cmd < CMD_SHOW_MODE_CUSTOM) {
        HandleShowModeCommand(static_cast<int>(cmd - CMD_SHOW_MODE_BASE));
        return;
    }
    if (cmd >= CMD_POSITION_BASE && cmd < CMD_POSITION_CUSTOM) {
        HandlePositionCommand(static_cast<PositionPreset>(cmd - CMD_POSITION_BASE));
        return;
    }
    if (cmd >= CMD_DRAG_MODE_BASE && cmd < CMD_DRAG_MODE_CUSTOM) {
        HandleDragSwitchModeCommand(static_cast<int>(cmd - CMD_DRAG_MODE_BASE));
        return;
    }
    if (cmd >= CMD_COLOR_OPTIONS_BASE && cmd < CMD_COLOR_OPTIONS_BASE + static_cast<UINT>(kPredefinedColors.size())) {
        HandleColorCommand(static_cast<int>(cmd - CMD_COLOR_OPTIONS_BASE));
        return;
    }

    switch (cmd) {
    case WM_TRAY_EXIT: HandleExit(); break;
    case WM_TRAY_TOGGLE_AUTOSTART: HandleToggleAutoStart(); break;
    case WM_TRAY_RUNAS_ADMIN: HandleRunAsAdmin(); break;
    case CMD_POSITION_CUSTOM:
        m_activePositionPreset = PositionPreset::Custom;
        if (m_editModeFn) { m_editModeFn(); }
        break;
    case WM_TRAY_SETTINGS:
        if (m_settingsFn) { m_settingsFn(); }
        break;
    case WM_TRAY_LANG_CHINESE:
        Lang::Set(LangType::Chinese);
        WriteIniInt(L"General", L"Language", 0);
        UpdateTooltip(Lang::Get(L"Tray.DefaultTip"));
        break;
    case WM_TRAY_LANG_ENGLISH:
        Lang::Set(LangType::English);
        WriteIniInt(L"General", L"Language", 1);
        UpdateTooltip(Lang::Get(L"Tray.DefaultTip"));
        break;
    case WM_TRAY_RESET: HandleReset(); break;
    case WM_TRAY_ABOUT:
        if (m_aboutFn) { m_aboutFn(); }
        break;
    case WM_TRAY_ANIM_MODE: HandleAnimMode(); break;
    case WM_TRAY_AUTO_CONTRAST: HandleAutoContrast(); break;
    case WM_TRAY_AUTO_FOCUS: HandleAutoFocus(); break;
    case WM_TRAY_TOGGLE_SHOW: HandleToggleShow(); break;
    default: break;
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
        cr.right = cr.left + MulDiv(GetSystemMetrics(SM_CXMENUCHECK), m_dpi, 96);
        SetTextColor(dis->hDC, (isSel != 0) ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_MENUTEXT));
        SetBkMode(dis->hDC, TRANSPARENT);
        DrawTextW(dis->hDC, L"\u2713", -1, &cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Color swatch
    const int labelW = m_menuAveWidth * 3;
    const int pad    = MulDiv(3, m_dpi, 96);

    RECT cr = dis->rcItem;
    cr.left += labelW + pad;
    cr.top += MulDiv(1, m_dpi, 96);
    cr.bottom -= MulDiv(1, m_dpi, 96);
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
