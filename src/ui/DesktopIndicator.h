// Contains code adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#pragma once

#include <windows.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/IndicatorConfig.h"
#include "util/Utils.h"

class FontRenderer;

struct MonitorLayer {
    HWND                 hwnd{};
    RECT                 monitor{}; // rcMonitor
    RECT                 work{};    // rcWork (excluding taskbar)
    int                  dpi{};     // LOGPIXELSY
    bool                 isPrimary     = false;
    bool                 bgSampleValid = false;
    double               bgLch_L       = -1.0; // CIE L* (-1 = uninitialized)
    double               bgLch_C       = 0.0;  // CIE C* (chroma)
    std::array<float, 5> smoothV{};            // per-color smoothed V
    std::array<float, 5> smoothS{};            // per-color smoothed S
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

    bool Initialize(HINSTANCE hInstance);
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
    void SetAnimMode(bool on);
    void SetAutoContrast(bool on);
    void SetScrollSwitchCallback(std::function<void(int)> cb) { m_scrollSwitchFn = std::move(cb); }
    void SetMoveWindowCallback(std::function<void(int, HWND)> cb) { m_moveWindowFn = std::move(cb); }
    HWND CreateMonitorWindow(HINSTANCE hInst);

    [[nodiscard]] bool IsEditMode() const { return m_editMode; }

private:
    std::vector<MonitorLayer>      m_layers;
    std::unique_ptr<FontRenderer>  m_renderer;
    IndicatorConfig               *m_pCfg = nullptr;
    std::wstring                   m_text;
    std::wstring                   m_previewColor;
    bool                           m_hasPreview     = false;
    int                            m_desktopCount   = 0;
    int                            m_currentDesktop = 0;
    std::array<bool, kMaxDesktops> m_emptyDesktops{};
    bool                           m_editMode   = false;
    bool                           m_dragging   = false;
    POINT                          m_dragOffset = {.x = 0, .y = 0};
    std::function<void(int)>       m_scrollSwitchFn;
    std::function<void(int, HWND)> m_moveWindowFn;
    HWND                           m_dragHwnd        = nullptr;
    int                            m_lastHoverSymbol = -1;
    int                            m_lastTextW       = 0;
    int                            m_lastPadding     = 8;

    void         ApplyShowMode(ShowMode mode);
    void         SampleBackground();
    void         StartAnimTimer();
    void         StartBgSampleTimer();
    void         StopAnimTimer();
    void         StopBgSampleTimer();
    void         RebuildText();
    std::wstring BuildLayerColors(MonitorLayer &layer, float hueOff, const std::array<COLORREF, 5> &baseColors, size_t colorCount) const;
    void         Render();
    void         ApplyPresetPosition();
    void         MoveByDelta(int dx, int dy);
    void         EnumerateMonitors(HINSTANCE hInstance);
    void         RegisterMouseWheelInput();
    static void  InstallDragHook();
    static void  UninstallDragHook();
    bool         GetSymbolIndexAt(POINT screenPt, int &outIndex) const;

    static HHOOK             s_dragHook;
    static DesktopIndicator *s_instance;
    static LRESULT CALLBACK  DragHookProc(int nCode, WPARAM wParam, LPARAM lParam);

    static void CALLBACK    AutoHideTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
    static void CALLBACK    AnimTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
    static void CALLBACK    BgSampleTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT                 HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};
