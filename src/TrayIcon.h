#ifndef TRAY_ICON_H
#define TRAY_ICON_H

#include <windows.h>

#include <shellapi.h>
#include <string>
#include <functional>

#define WM_TRAYICON (WM_USER + 2)
#define WM_TRAY_EXIT (WM_USER + 3)
#define WM_TRAY_TOGGLE_AUTOSTART (WM_USER + 4)
#define WM_TRAY_EDIT_MODE (WM_USER + 5)
#define WM_TRAY_SETTINGS (WM_USER + 6)
#define CMD_COLOR_OPTIONS_BASE (WM_USER + 100)

class TrayIcon {
private:
    NOTIFYICONDATAW m_nid{};
    HMENU           m_hMenu            = nullptr;
    bool            m_autoStartEnabled = false;
    bool            m_editModeChecked  = false;

    std::function<void()>                    m_editModeCb;
    std::function<void(const std::wstring&)> m_colorCb;
    std::function<void()>                    m_settingsCb;

    static bool IsAutoStartEnabled();
    static void SetAutoStart(bool enable);

    void BuildMenu();

public:
    TrayIcon(const TrayIcon &)            = delete;
    TrayIcon &operator=(const TrayIcon &) = delete;
    TrayIcon(TrayIcon &&)                 = delete;
    TrayIcon &operator=(TrayIcon &&)      = delete;

    TrayIcon() = default;
    ~TrayIcon();

    bool Initialize(HWND hwnd, HINSTANCE hInstance);
    void HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void UpdateTooltip(const std::wstring &tooltip);
    bool GetAutoStartStatus() const { return m_autoStartEnabled; }

    void SetEditModeCallback(std::function<void()> cb) { m_editModeCb = cb; }
    void SetColorCallback(std::function<void(const std::wstring&)> cb) { m_colorCb = cb; }
    void SetSettingsCallback(std::function<void()> cb) { m_settingsCb = cb; }
};

#endif
