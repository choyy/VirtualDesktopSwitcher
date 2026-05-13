#pragma once

#include <windows.h>

#include <shellapi.h>

#include <array>
#include <functional>
#include <string>

constexpr UINT WM_TRAYICON            = WM_USER + 2;
constexpr UINT CMD_COLOR_OPTIONS_BASE = WM_USER + 100;

constexpr std::array kPositionLabels      = {L"左上", L"中上", L"右上", L"左下", L"中下", L"右下"};
constexpr int        kPositionPresetCount = static_cast<int>(kPositionLabels.size());

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
    void SetShowModeCallback(std::function<void(int)> cb) { m_showModeFn = std::move(cb); }

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

    void BuildMenu();
    void HandleCommand(WPARAM wParam);
    void DrawColorSwatch(LPDRAWITEMSTRUCT dis) const;
};
