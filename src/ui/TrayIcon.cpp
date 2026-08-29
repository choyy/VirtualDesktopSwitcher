#include "TrayIcon.h"

#include <algorithm>
#include <array>

#include <gdiplus.h>

#include "core/IndicatorConfig.h"
#include "util/AutoStart.h"
#include "util/ConfigIni.h"
#include "util/Lang.h"
#include "util/Utils.h"

namespace {

constexpr UINT CMD_TRAY_EXIT             = WM_USER + 3;
constexpr UINT CMD_TRAY_TOGGLE_AUTOSTART = WM_USER + 4;
constexpr UINT CMD_TRAY_SETTINGS         = WM_USER + 6;
constexpr UINT CMD_TRAY_ABOUT            = WM_USER + 7;
constexpr UINT CMD_TRAY_RUNAS_ADMIN      = WM_USER + 8;
constexpr UINT CMD_TRAY_ANIM_MODE        = WM_USER + 9;
constexpr UINT CMD_TRAY_TOGGLE_SHOW      = WM_USER + 10;
constexpr UINT CMD_TRAY_RESET            = WM_USER + 11;
constexpr UINT CMD_TRAY_AUTO_CONTRAST    = WM_USER + 12;
constexpr UINT CMD_TRAY_AUTO_FOCUS       = WM_USER + 13;
constexpr UINT CMD_SHOW_MODE_BASE        = WM_USER + 300;
constexpr UINT CMD_SHOW_MODE_CUSTOM      = CMD_SHOW_MODE_BASE + static_cast<UINT>(ShowMode::Count);
constexpr UINT CMD_POSITION_BASE         = WM_USER + 200;
constexpr UINT CMD_POSITION_CUSTOM       = CMD_POSITION_BASE + static_cast<int>(PositionPreset::Count) + 1;
constexpr UINT CMD_DRAG_MODE_BASE        = WM_USER + 400;
constexpr UINT CMD_DRAG_MODE_CUSTOM      = CMD_DRAG_MODE_BASE + static_cast<UINT>(DragSwitchMode::Count);

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

} // namespace

// GdiplusGuard RAII：GDI+ 生命周期由 TrayIcon 持有（构造启动、析构关闭）
class GdiplusGuard {
public:
    GdiplusGuard() {
        Gdiplus::GdiplusStartupInput input;
        if (Gdiplus::GdiplusStartup(&m_token, &input, nullptr) != Gdiplus::Ok) {
            m_token = 0;
        }
    }
    ~GdiplusGuard() {
        if (m_token != 0) {
            Gdiplus::GdiplusShutdown(m_token);
        }
    }
    GdiplusGuard(const GdiplusGuard &)            = delete;
    GdiplusGuard &operator=(const GdiplusGuard &) = delete;
    GdiplusGuard(GdiplusGuard &&)                 = delete;
    GdiplusGuard &operator=(GdiplusGuard &&)      = delete;

    [[nodiscard]] bool Ok() const { return m_token != 0; }

private:
    ULONG_PTR m_token = 0;
};

namespace {

void DrawSwatchRect(HDC hdc, RECT rect, const std::wstring &hex, GdiplusGuard &gdiplus) {
    auto colors = ParseMultiColorString(hex);

    if (colors.count >= 2 && gdiplus.Ok()) {
        std::array<Gdiplus::Color, 5> gdColors{};
        for (size_t i = 0; i < colors.count; ++i) {
            gdColors[i] = Gdiplus::Color(255, GetRValue(colors.colors[i]),
                                         GetGValue(colors.colors[i]), GetBValue(colors.colors[i]));
        }
        Gdiplus::LinearGradientBrush brush(
            Gdiplus::RectF(static_cast<Gdiplus::REAL>(rect.left), static_cast<Gdiplus::REAL>(rect.top),
                           static_cast<Gdiplus::REAL>(rect.right - rect.left),
                           static_cast<Gdiplus::REAL>(rect.bottom - rect.top)),
            gdColors[0], gdColors[colors.count - 1], Gdiplus::LinearGradientModeHorizontal);
        if (colors.count > 2) {
            std::array<Gdiplus::REAL, 5> positions{};
            for (size_t i = 0; i < colors.count; ++i) {
                positions[i] = static_cast<Gdiplus::REAL>(i) / static_cast<Gdiplus::REAL>(colors.count - 1);
            }
            brush.SetInterpolationColors(gdColors.data(), positions.data(), static_cast<INT>(colors.count));
        }
        {
            Gdiplus::Graphics g(hdc);
            g.FillRectangle(&brush,
                            static_cast<Gdiplus::REAL>(rect.left), static_cast<Gdiplus::REAL>(rect.top),
                            static_cast<Gdiplus::REAL>(rect.right - rect.left),
                            static_cast<Gdiplus::REAL>(rect.bottom - rect.top));
        }
    } else if (colors.count >= 1) {
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

TrayIcon::TrayIcon() : m_gdiplus(std::make_unique<GdiplusGuard>()) {}

TrayIcon::~TrayIcon() {
    if (m_nid.hWnd != nullptr) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
    }
    if (m_hMenu != nullptr) {
        DestroyMenu(m_hMenu);
    }
    for (int i = 0; i <= kMaxDesktops; ++i) {
        if (m_hNumberIcons[i] != nullptr) {
            DestroyIcon(m_hNumberIcons[i]);
            m_hNumberIcons[i] = nullptr;
        }
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
    AppendMenuW(hColorMenu, MF_STRING | (animOn ? MF_CHECKED : 0), CMD_TRAY_ANIM_MODE, Lang::Get(L"Menu.BreathingRing"));
    bool autoContrastOn = ReadIniInt(L"Display", L"AutoContrast", 1) != 0;
    AppendMenuW(hColorMenu, MF_STRING | (autoContrastOn ? MF_CHECKED : 0), CMD_TRAY_AUTO_CONTRAST, Lang::Get(L"Menu.AutoContrast"));
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hColorMenu), Lang::Get(L"Menu.Color")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    HMENU hTrayIconMenu = CreatePopupMenu();
    int   curIconMode   = ReadIniInt(L"Display", L"TrayIconMode", static_cast<int>(TrayIconMode::Icon));
    AppendMenuW(hTrayIconMenu, MF_STRING | (curIconMode == static_cast<int>(TrayIconMode::Icon) ? MF_CHECKED : 0),
                CMD_TRAY_ICON_MODE_ICON, Lang::Get(L"Menu.TrayIconIcon"));
    AppendMenuW(hTrayIconMenu, MF_STRING | (curIconMode == static_cast<int>(TrayIconMode::Number) ? MF_CHECKED : 0),
                CMD_TRAY_ICON_MODE_NUMBER, Lang::Get(L"Menu.TrayIconNumber"));
    AppendMenuW(hTrayIconMenu, MF_SEPARATOR, 0, nullptr);

    HMENU        hNumColorMenu = CreatePopupMenu();
    std::wstring curNumColor   = ReadIniString(L"Display", L"TrayNumberColor", L"#2997FF");
    UINT         numColorIdx   = 0;
    for (UINT i = 0; i < static_cast<UINT>(kPredefinedColors.size()); ++i) {
        if (std::wstring(kPredefinedColors.at(i)).find(L'_') != std::wstring::npos) { continue; }
        MENUITEMINFOW mii = {};
        mii.cbSize        = sizeof(mii);
        mii.fMask         = MIIM_ID | MIIM_FTYPE | MIIM_STATE;
        mii.fType         = MFT_OWNERDRAW;
        mii.wID           = CMD_TRAY_NUMBER_COLOR_BASE + i;
        if (wcscmp(kPredefinedColors.at(i), curNumColor.c_str()) == 0) {
            mii.fState = MFS_CHECKED;
        }
        InsertMenuItemW(hNumColorMenu, numColorIdx++, TRUE, &mii);
    }
    AppendMenuW(hTrayIconMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hNumColorMenu), Lang::Get(L"Menu.TrayNumberColor")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hTrayIconMenu), Lang::Get(L"Menu.TrayIcon"));              // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    AppendMenuW(m_hMenu, MF_STRING, CMD_TRAY_SETTINGS, Lang::Get(L"Menu.Settings"));
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);

    HMENU hLangMenu = CreatePopupMenu();
    AppendMenuW(hLangMenu, MF_STRING | (Lang::Current() == LangType::Chinese ? MF_CHECKED : 0), CMD_TRAY_LANG_CHINESE, Lang::Get(L"Menu.LangChinese"));
    AppendMenuW(hLangMenu, MF_STRING | (Lang::Current() == LangType::English ? MF_CHECKED : 0), CMD_TRAY_LANG_ENGLISH, Lang::Get(L"Menu.LangEnglish"));
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hLangMenu), Lang::Get(L"Menu.Language")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    bool autoFocusOn = ReadIniInt(L"Display", L"AutoFocus", 1) != 0;
    AppendMenuW(m_hMenu, MF_STRING | (autoFocusOn ? MF_CHECKED : 0),
                CMD_TRAY_AUTO_FOCUS, Lang::Get(L"Menu.AutoFocus"));

    bool runAsAdmin = ReadIniInt(L"General", L"RunAsAdmin", 0) != 0;
    AppendMenuW(m_hMenu, MF_STRING | (runAsAdmin ? MF_CHECKED : MF_UNCHECKED),
                CMD_TRAY_RUNAS_ADMIN, Lang::Get(L"Menu.RunAsAdmin"));

    AppendMenuW(m_hMenu, MF_STRING | (m_autoStartEnabled ? MF_CHECKED : MF_UNCHECKED),
                CMD_TRAY_TOGGLE_AUTOSTART, Lang::Get(L"Menu.AutoStart"));

    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_hMenu, MF_STRING, CMD_TRAY_RESET, Lang::Get(L"Menu.Reset"));
    AppendMenuW(m_hMenu, MF_STRING, CMD_TRAY_ABOUT, Lang::Get(L"Menu.About"));
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_hMenu, MF_STRING, CMD_TRAY_EXIT, Lang::Get(L"Menu.Exit"));
}

bool TrayIcon::Initialize(HWND hwnd, HINSTANCE hInstance) {
    m_hInstance = hInstance;

    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(kTrayDefaultIconResource));
    if (hIcon == nullptr) {
        hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    }

    m_nid.cbSize           = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd             = hwnd;
    m_nid.uID              = 1;
    m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon            = hIcon;

    int modeVal = ReadIniInt(L"Display", L"TrayIconMode", static_cast<int>(TrayIconMode::Icon));
    m_iconMode  = (modeVal >= 0 && modeVal < static_cast<int>(TrayIconMode::Count))
                      ? static_cast<TrayIconMode>(modeVal)
                      : TrayIconMode::Icon;

    std::wstring numColor = ReadIniString(L"Display", L"TrayNumberColor", L"#2997FF");
    auto         colorArr = ParseMultiColorString(numColor);
    m_numberColor         = (colorArr.count > 0) ? colorArr.colors[0] : RGB(41, 151, 255);

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
            PostMessage(hwnd, WM_COMMAND, CMD_TRAY_TOGGLE_SHOW, 0);
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
            if (dis->itemID >= CMD_TRAY_NUMBER_COLOR_BASE
                && dis->itemID < CMD_TRAY_NUMBER_COLOR_BASE + static_cast<UINT>(kPredefinedColors.size())) {
                DrawColorSwatch(dis, CMD_TRAY_NUMBER_COLOR_BASE);
            } else {
                DrawColorSwatch(dis, CMD_COLOR_OPTIONS_BASE);
            }
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

void TrayIcon::SetTrayIconMode(TrayIconMode mode) {
    if (m_iconMode == mode) { return; }
    m_iconMode = mode;
    WriteIniInt(L"Display", L"TrayIconMode", static_cast<int>(mode));
    m_nTrayNumber = -1;
    if (mode == TrayIconMode::Icon) {
        UpdateTrayIcon(1);
    }
}

void TrayIcon::SetNumberColor(const std::wstring &hexColor) {
    auto colorArr = ParseMultiColorString(hexColor);
    m_numberColor = (colorArr.count > 0) ? colorArr.colors[0] : RGB(41, 151, 255);
    WriteIniString(L"Display", L"TrayNumberColor", hexColor);
    for (auto &hIcon : m_hNumberIcons) {
        if (hIcon != nullptr) {
            DestroyIcon(hIcon);
            hIcon = nullptr;
        }
    }
    if (m_iconMode == TrayIconMode::Number && m_nTrayNumber >= 1) {
        int num       = m_nTrayNumber;
        m_nTrayNumber = -1;
        UpdateTrayIcon(num);
    }
}

void TrayIcon::HandleNumberColorCommand(int colorIndex) {
    SetNumberColor(kPredefinedColors.at(colorIndex));
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
    if (cmd >= CMD_TRAY_NUMBER_COLOR_BASE && cmd < CMD_TRAY_NUMBER_COLOR_BASE + static_cast<UINT>(kPredefinedColors.size())) {
        HandleNumberColorCommand(static_cast<int>(cmd - CMD_TRAY_NUMBER_COLOR_BASE));
        return;
    }

    switch (cmd) {
    case CMD_TRAY_EXIT: HandleExit(); break;
    case CMD_TRAY_TOGGLE_AUTOSTART: HandleToggleAutoStart(); break;
    case CMD_TRAY_RUNAS_ADMIN: HandleRunAsAdmin(); break;
    case CMD_TRAY_ICON_MODE_ICON: SetTrayIconMode(TrayIconMode::Icon); break;
    case CMD_TRAY_ICON_MODE_NUMBER: SetTrayIconMode(TrayIconMode::Number); break;
    case CMD_POSITION_CUSTOM:
        m_activePositionPreset = PositionPreset::Custom;
        if (m_editModeFn) { m_editModeFn(); }
        break;
    case CMD_TRAY_SETTINGS:
        if (m_settingsFn) { m_settingsFn(); }
        break;
    case CMD_TRAY_LANG_CHINESE:
        Lang::Set(LangType::Chinese);
        WriteIniInt(L"General", L"Language", 0);
        UpdateTooltip(Lang::Get(L"Tray.DefaultTip"));
        break;
    case CMD_TRAY_LANG_ENGLISH:
        Lang::Set(LangType::English);
        WriteIniInt(L"General", L"Language", 1);
        UpdateTooltip(Lang::Get(L"Tray.DefaultTip"));
        break;
    case CMD_TRAY_RESET: HandleReset(); break;
    case CMD_TRAY_ABOUT:
        if (m_aboutFn) { m_aboutFn(); }
        break;
    case CMD_TRAY_ANIM_MODE: HandleAnimMode(); break;
    case CMD_TRAY_AUTO_CONTRAST: HandleAutoContrast(); break;
    case CMD_TRAY_AUTO_FOCUS: HandleAutoFocus(); break;
    case CMD_TRAY_TOGGLE_SHOW: HandleToggleShow(); break;
    default: break;
    }
}

void TrayIcon::DrawColorSwatch(LPDRAWITEMSTRUCT dis, UINT base) const {
    const int colorIndex = static_cast<int>(dis->itemID - base);
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
    const int pad = MulDiv(3, m_dpi, 96);

    RECT cr = dis->rcItem;
    cr.left += pad;
    cr.top += MulDiv(1, m_dpi, 96);
    cr.bottom -= MulDiv(1, m_dpi, 96);
    cr.right = dis->rcItem.right - pad;

    DrawSwatchRect(dis->hDC, cr, kPredefinedColors.at(colorIndex), *m_gdiplus);
}

bool TrayIcon::Reinitialize() {
    if (m_nid.hWnd != nullptr) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
    }
    const bool ok = Shell_NotifyIconW(NIM_ADD, &m_nid) != 0;
    m_nTrayNumber = -1; // 强制下一次同步重新应用桌面编号图标
    return ok;
}

void TrayIcon::UpdateTooltip(const std::wstring &tooltip) {
    wcsncpy_s(m_nid.szTip, tooltip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayIcon::UpdateTrayIcon(int displayNumber) {
    if (m_iconMode == TrayIconMode::Icon) {
        if (m_nTrayNumber == 0) { return; }
        HICON           hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(kTrayDefaultIconResource));
        NOTIFYICONDATAW nid   = {};
        nid.cbSize            = sizeof(nid);
        nid.hWnd              = m_nid.hWnd;
        nid.uID               = m_nid.uID;
        nid.uFlags            = NIF_ICON;
        nid.hIcon             = hIcon;
        Shell_NotifyIconW(NIM_MODIFY, &nid);
        m_nid.hIcon   = hIcon;
        m_nTrayNumber = 0;
        return;
    }

    if (displayNumber == m_nTrayNumber) {
        return;
    }

    NOTIFYICONDATAW nid = {};
    nid.cbSize          = sizeof(nid);
    nid.hWnd            = m_nid.hWnd;
    nid.uID             = m_nid.uID;
    nid.uFlags          = NIF_ICON;
    nid.hIcon           = GetTrayIconForNumber(displayNumber);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
    m_nid.hIcon   = nid.hIcon;
    m_nTrayNumber = displayNumber;
}

HICON TrayIcon::GetTrayIconForNumber(int nDisplay) {
    if (nDisplay >= 1 && nDisplay <= kMaxDesktops) {
        HICON hIcon = GetNumberIcon(nDisplay);
        if (hIcon != nullptr) {
            return hIcon;
        }
    }
    return LoadIcon(m_hInstance, MAKEINTRESOURCE(kTrayDefaultIconResource));
}

HICON TrayIcon::GetNumberIcon(int nNumber) {
    if (nNumber < 1 || nNumber > kMaxDesktops) {
        return nullptr;
    }
    if (m_hNumberIcons[nNumber] == nullptr) {
        m_hNumberIcons[nNumber] = CreateNumberIcon(nNumber, m_numberColor, *m_gdiplus);
    }
    return m_hNumberIcons[nNumber];
}

HICON TrayIcon::CreateNumberIcon(int nNumber, COLORREF color, GdiplusGuard &gdiplus) {
    if (!gdiplus.Ok()) {
        return nullptr;
    }

    int nSize = std::clamp(GetSystemMetrics(SM_CXSMICON), 16, 64);

    Gdiplus::Bitmap   bmp(nSize, nSize, PixelFormat32bppARGB);
    Gdiplus::Graphics graphics(&bmp);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

    std::wstring szNum = std::to_wstring(nNumber);

    Gdiplus::FontFamily   fontFamily(L"Segoe UI");
    Gdiplus::Font         font(&fontFamily, static_cast<Gdiplus::REAL>(nSize) * 0.85f,
                               Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush   brushText(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::RectF rectText(0.0f, 0.0f, static_cast<Gdiplus::REAL>(nSize), static_cast<Gdiplus::REAL>(nSize));
    graphics.DrawString(szNum.c_str(), -1, &font, rectText, &format, &brushText);

    HICON hIcon = nullptr;
    if (bmp.GetHICON(&hIcon) != Gdiplus::Ok) {
        hIcon = nullptr;
    }
    return hIcon;
}
