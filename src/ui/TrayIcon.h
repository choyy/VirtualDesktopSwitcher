#pragma once

#include <windows.h>

#include <shellapi.h>

#include <functional>
#include <string>

constexpr UINT WM_TRAYICON                = WM_USER + 2;
constexpr UINT CMD_COLOR_OPTIONS_BASE     = WM_USER + 100;
constexpr UINT CMD_POSITION_BASE          = WM_USER + 200;
constexpr UINT CMD_POSITION_TOP_LEFT      = CMD_POSITION_BASE + 0;
constexpr UINT CMD_POSITION_TOP_CENTER    = CMD_POSITION_BASE + 1;
constexpr UINT CMD_POSITION_TOP_RIGHT     = CMD_POSITION_BASE + 2;
constexpr UINT CMD_POSITION_BOTTOM_LEFT   = CMD_POSITION_BASE + 3;
constexpr UINT CMD_POSITION_BOTTOM_CENTER = CMD_POSITION_BASE + 4;
constexpr UINT CMD_POSITION_BOTTOM_RIGHT  = CMD_POSITION_BASE + 5;
constexpr UINT CMD_POSITION_CUSTOM        = CMD_POSITION_BASE + 6;

class TrayIcon {
public:
    TrayIcon(const TrayIcon &)            = delete;
    TrayIcon &operator=(const TrayIcon &) = delete;
    TrayIcon(TrayIcon &&)                 = delete;
    TrayIcon &operator=(TrayIcon &&)      = delete;

    TrayIcon() = default;
    ~TrayIcon();

    bool               Initialize(HWND hwnd, HINSTANCE hInstance);
    bool               Reinitialize();
    void               HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void               UpdateTooltip(const std::wstring &tooltip);
    [[nodiscard]] bool GetAutoStartStatus() const { return m_autoStartEnabled; }

    void SetActivePositionPreset(int preset) { m_activePositionPreset = preset; }
    void SetEditModeCallback(std::function<void()> cb) { m_editModeFn = std::move(cb); }
    void SetPositionCallback(std::function<void(int)> cb) { m_positionFn = std::move(cb); }
    void SetColorCallback(std::function<void(const std::wstring &)> cb) { m_colorFn = std::move(cb); }
    void SetSettingsCallback(std::function<void()> cb) { m_settingsFn = std::move(cb); }
    void SetAboutCallback(std::function<void()> cb) { m_aboutFn = std::move(cb); }

private:
    NOTIFYICONDATAW m_nid{};
    HMENU           m_hMenu                = nullptr;
    bool            m_autoStartEnabled     = false;
    bool            m_editModeChecked      = false;
    int             m_activePositionPreset = 1;

    std::function<void(const std::wstring &)> m_colorFn;
    std::function<void()>                     m_editModeFn;
    std::function<void(int)>                  m_positionFn;
    std::function<void()>                     m_settingsFn;
    std::function<void()>                     m_aboutFn;

    static bool IsAutoStartEnabled();
    static void SetAutoStart(bool enable);

    void        BuildMenu();
    void        HandleCommand(WPARAM wParam);
    static void DrawColorSwatch(LPDRAWITEMSTRUCT dis);
};
