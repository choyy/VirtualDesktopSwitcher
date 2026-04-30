#ifndef TRAY_ICON_H
#define TRAY_ICON_H

#include <windows.h>

#include <shellapi.h>

#include <string>

constexpr UINT WM_TRAYICON                = WM_USER + 2;
constexpr UINT WM_TRAY_EXIT               = WM_USER + 3;
constexpr UINT WM_TRAY_TOGGLE_AUTOSTART   = WM_USER + 4;
constexpr UINT WM_TRAY_EDIT_MODE          = WM_USER + 5;
constexpr UINT WM_TRAY_SETTINGS           = WM_USER + 6;
constexpr UINT WM_TRAY_ABOUT              = WM_USER + 7;
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
    void SetEditModeCallback(void (*cb)(void *), void *ctx) {
        m_editModeFn  = cb;
        m_editModeCtx = ctx;
    }
    void SetPositionCallback(void (*cb)(int, void *), void *ctx) {
        m_positionFn  = cb;
        m_positionCtx = ctx;
    }
    void SetColorCallback(void (*cb)(const std::wstring &, void *), void *ctx) {
        m_colorFn  = cb;
        m_colorCtx = ctx;
    }
    void SetSettingsCallback(void (*cb)(void *), void *ctx) {
        m_settingsFn  = cb;
        m_settingsCtx = ctx;
    }
    void SetAboutCallback(void (*cb)(void *), void *ctx) {
        m_aboutFn  = cb;
        m_aboutCtx = ctx;
    }

private:
    NOTIFYICONDATAW m_nid{};
    HMENU           m_hMenu                = nullptr;
    bool            m_autoStartEnabled     = false;
    bool            m_editModeChecked      = false;
    int             m_activePositionPreset = 1;

    void (*m_editModeFn)(void *)                    = nullptr;
    void *m_editModeCtx                             = nullptr;
    void (*m_positionFn)(int, void *)               = nullptr;
    void *m_positionCtx                             = nullptr;
    void (*m_colorFn)(const std::wstring &, void *) = nullptr;
    void *m_colorCtx                                = nullptr;
    void (*m_settingsFn)(void *)                    = nullptr;
    void *m_settingsCtx                             = nullptr;
    void (*m_aboutFn)(void *)                       = nullptr;
    void *m_aboutCtx                                = nullptr;

    static bool IsAutoStartEnabled();
    static void SetAutoStart(bool enable);

    void        BuildMenu();
    void        HandleCommand(WPARAM wParam);
    static void DrawColorSwatch(LPDRAWITEMSTRUCT dis);
};

#endif
