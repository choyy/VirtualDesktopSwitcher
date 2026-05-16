#pragma once

#include <windows.h>

#include <shellapi.h>

#include <functional>
#include <string>

constexpr UINT WM_TRAYICON            = WM_USER + 2;
constexpr UINT CMD_COLOR_OPTIONS_BASE = WM_USER + 100;
constexpr UINT WM_TRAY_LANG_CHINESE   = WM_USER + 50;
constexpr UINT WM_TRAY_LANG_ENGLISH   = WM_USER + 51;

class TrayIcon {
public:
    TrayIcon(const TrayIcon &)            = delete;
    TrayIcon &operator=(const TrayIcon &) = delete;
    TrayIcon(TrayIcon &&)                 = delete;
    TrayIcon &operator=(TrayIcon &&)      = delete;

    TrayIcon() = default;
    ~TrayIcon();

    bool Initialize(HWND hwnd, HINSTANCE hInstance);
    bool Reinitialize();
    void HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void UpdateTooltip(const std::wstring &tooltip);

    void SetActivePositionPreset(int preset) { m_activePositionPreset = preset; }
    void SetEditModeCallback(std::function<void()> cb) { m_editModeFn = std::move(cb); }
    void SetPositionCallback(std::function<void(int)> cb) { m_positionFn = std::move(cb); }
    void SetColorCallback(std::function<void(const std::wstring &)> cb) { m_colorFn = std::move(cb); }
    void SetSettingsCallback(std::function<void()> cb) { m_settingsFn = std::move(cb); }
    void SetAboutCallback(std::function<void()> cb) { m_aboutFn = std::move(cb); }
    void SetShowModeCallback(std::function<void(int)> cb) { m_showModeFn = std::move(cb); }
    void SetAnimModeCallback(std::function<void(bool)> cb) { m_animModeFn = std::move(cb); }

private:
    NOTIFYICONDATAW m_nid{};
    HMENU           m_hMenu                = nullptr;
    bool            m_autoStartEnabled     = false;
    int             m_activePositionPreset = 1;
    int             m_menuAveWidth         = 6;
    int             m_dpi                  = 96;

    std::function<void(const std::wstring &)> m_colorFn;
    std::function<void()>                     m_editModeFn;
    std::function<void(int)>                  m_positionFn;
    std::function<void()>                     m_settingsFn;
    std::function<void()>                     m_aboutFn;
    std::function<void(int)>                  m_showModeFn;
    std::function<void(bool)>                 m_animModeFn;

    void BuildMenu();
    void HandleCommand(WPARAM wParam);
    void DrawColorSwatch(LPDRAWITEMSTRUCT dis) const;
};
