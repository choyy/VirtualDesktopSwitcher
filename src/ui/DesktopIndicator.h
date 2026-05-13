// Contains code adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#pragma once

#include <windows.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "util/IndicatorConfig.h"
#include "util/Utils.h"

class FontRenderer;

struct MonitorLayer {
    HWND hwnd{};
    RECT monitor{}; // rcMonitor
    RECT work{};    // rcWork (excluding taskbar)
    int  dpi{};     // LOGPIXELSY
    bool isPrimary = false;
};

class DesktopIndicator {
public:
    DesktopIndicator();
    ~DesktopIndicator();
    DesktopIndicator(const DesktopIndicator &)            = delete;
    DesktopIndicator &operator=(const DesktopIndicator &) = delete;
    DesktopIndicator(DesktopIndicator &&)                 = delete;
    DesktopIndicator &operator=(DesktopIndicator &&)      = delete;

    void SetConfig(IndicatorConfig *cfg) { m_pCfg = cfg; }
    void SetOnConfigChanged(std::function<void()> cb) { m_onConfigChanged = std::move(cb); }

    bool Initialize(HINSTANCE hInstance);
    void Show();
    void SetDesktopState(int count, int currentIndex, const std::array<bool, kMaxDesktops> &emptyDesktops);
    void SetColor(const std::wstring &hexColor);
    void SetColorPreview(const std::wstring &hexColor);
    void CancelPreview();
    void SetCurrentSymbol(const std::wstring &sym);
    void SetOtherSymbol(const std::wstring &sym);
    void SetEmptySymbol(const std::wstring &sym);
    void SetFontName(const std::wstring &name);
    void SetCharSpacing(int spacing);
    void ToggleEditMode();
    void SetEditMode(bool edit);
    void SetPositionPreset(int preset);
    void Rebuild();
    void SetShowMode(ShowMode mode);
    void ShowTemporarily();
    HWND CreateMonitorWindow(HINSTANCE hInst);

    [[nodiscard]] bool IsEditMode() const { return m_editMode; }

private:
    std::vector<MonitorLayer>      m_layers;
    std::unique_ptr<FontRenderer>  m_renderer;
    IndicatorConfig               *m_pCfg = nullptr;
    std::function<void()>          m_onConfigChanged;
    std::wstring                   m_text;
    std::wstring                   m_previewColor;
    bool                           m_hasPreview     = false;
    int                            m_desktopCount   = 0;
    int                            m_currentDesktop = 0;
    std::array<bool, kMaxDesktops> m_emptyDesktops{};
    bool                           m_editMode   = false;
    bool                           m_dragging   = false;
    POINT                          m_dragOffset = {.x = 0, .y = 0};

    void RebuildText();
    void Render();
    void ApplyPresetPosition();
    void MoveByDelta(int dx, int dy);

    static void CALLBACK    AutoHideTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT                 HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};
