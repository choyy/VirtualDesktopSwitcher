#pragma once

#include <windows.h>

#include <shellapi.h>

#include <array>
#include <functional>
#include <memory>
#include <string>

#include "core/IndicatorConfig.h"
#include "util/Utils.h"

constexpr UINT WM_TRAYICON                = WM_USER + 2;
constexpr UINT CMD_TRAY_LANG_CHINESE      = WM_USER + 50;
constexpr UINT CMD_TRAY_LANG_ENGLISH      = WM_USER + 51;
constexpr UINT CMD_TRAY_ICON_MODE_ICON    = WM_USER + 60;
constexpr UINT CMD_TRAY_ICON_MODE_NUMBER  = WM_USER + 61;
constexpr UINT CMD_COLOR_OPTIONS_BASE     = WM_USER + 100;
constexpr UINT CMD_TRAY_NUMBER_COLOR_BASE = WM_USER + 500;

constexpr int kTrayDefaultIconResource = 101;
class GdiplusGuard;

class TrayIcon {
public:
    TrayIcon(const TrayIcon &)            = delete;
    TrayIcon &operator=(const TrayIcon &) = delete;
    TrayIcon(TrayIcon &&)                 = delete;
    TrayIcon &operator=(TrayIcon &&)      = delete;

    TrayIcon();
    ~TrayIcon();

    bool Initialize(HWND hwnd, HINSTANCE hInstance);
    bool Reinitialize();
    void HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void UpdateTooltip(const std::wstring &tooltip);
    void UpdateTrayIcon(int displayNumber);
    void SetTrayIconMode(TrayIconMode mode);
    void SetNumberColor(const std::wstring &hexColor);

    void SetActivePositionPreset(PositionPreset preset) { m_activePositionPreset = preset; }
    void SetEditModeCallback(std::function<void()> cb) { m_editModeFn = std::move(cb); }
    void SetPositionCallback(std::function<void(PositionPreset)> cb) { m_positionFn = std::move(cb); }
    void SetColorCallback(std::function<void(const std::wstring &)> cb) { m_colorFn = std::move(cb); }
    void SetSettingsCallback(std::function<void()> cb) { m_settingsFn = std::move(cb); }
    void SetAboutCallback(std::function<void()> cb) { m_aboutFn = std::move(cb); }
    void SetShowModeCallback(std::function<void(int)> cb) { m_showModeFn = std::move(cb); }
    void SetAnimModeCallback(std::function<void(bool)> cb) { m_animModeFn = std::move(cb); }
    void SetAutoContrastCallback(std::function<void(bool)> cb) { m_autoContrastFn = std::move(cb); }
    void SetAutoFocusCallback(std::function<void(bool)> cb) { m_autoFocusFn = std::move(cb); }
    void SetDragSwitchModeCallback(std::function<void(int)> cb) { m_dragModeFn = std::move(cb); }

private:
    NOTIFYICONDATAW                     m_nid{};
    std::unique_ptr<GdiplusGuard>       m_gdiplus;
    HINSTANCE                           m_hInstance            = nullptr;
    HMENU                               m_hMenu                = nullptr;
    bool                                m_autoStartEnabled     = false;
    PositionPreset                      m_activePositionPreset = PositionPreset::TopCenter;
    int                                 m_dpi                  = 96;
    int                                 m_nTrayNumber          = -1;
    TrayIconMode                        m_iconMode             = TrayIconMode::Icon;
    COLORREF                            m_numberColor          = RGB(41, 151, 255);
    std::array<HICON, kMaxDesktops + 1> m_hNumberIcons{};

    std::function<void(const std::wstring &)> m_colorFn;
    std::function<void()>                     m_editModeFn;
    std::function<void(PositionPreset)>       m_positionFn;
    std::function<void()>                     m_settingsFn;
    std::function<void()>                     m_aboutFn;
    std::function<void(int)>                  m_showModeFn;
    std::function<void(bool)>                 m_animModeFn;
    std::function<void(bool)>                 m_autoContrastFn;
    std::function<void(bool)>                 m_autoFocusFn;
    std::function<void(int)>                  m_dragModeFn;

    void         BuildMenu();
    void         HandleCommand(WPARAM wParam);
    void         DrawColorSwatch(LPDRAWITEMSTRUCT dis, UINT base) const;
    HICON        GetNumberIcon(int nNumber);
    HICON        GetTrayIconForNumber(int nDisplay);
    static HICON CreateNumberIcon(int nNumber, COLORREF color, GdiplusGuard &gdiplus);

    static void HandleRunAsAdmin();
    static void HandleReset();
    static void HandleExit();
    void        HandleAnimMode();
    void        HandleAutoContrast();
    void        HandleAutoFocus();
    void        HandleDragSwitchModeCommand(int mode);
    void        HandleToggleShow();
    void        HandleToggleAutoStart();
    void        HandleShowModeCommand(int mode);
    void        HandleNumberColorCommand(int colorIndex);
    void        HandlePositionCommand(PositionPreset preset);
    void        HandleColorCommand(int index);
};
