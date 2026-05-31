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

enum class TaskbarSide : std::uint8_t { Right,
                                        Left };

struct SymbolMetrics {
    std::array<int, 9> widths{};
    int                fontSize = 0;
    int                spacing  = 0;
    SIZE               dibSize{};
};

struct MonitorLayer {
    HWND                 hwnd{};
    RECT                 monitor{}; // rcMonitor
    RECT                 work{};    // rcWork (excluding taskbar)
    int                  dpi{};     // LOGPIXELSY
    bool                 isPrimary     = false;
    bool                 bgSampleValid = false;
    double               bgLch_L       = -1.0;             // CIE L* (-1 = uninitialized)
    double               bgLch_C       = 0.0;              // CIE C* (chroma)
    std::array<float, 5> smoothV{};                        // per-color smoothed V
    std::array<float, 5> smoothS{};                        // per-color smoothed S
    std::array<float, 9> symbolScales{};                   // per-symbol dock scale (lerped)
    std::array<float, 9> symbolCenters{};                  // client X center after render
    std::array<float, 9> symbolHalfWidths{};               // half-width after render
    bool                 hasTaskbar  = false;              // embed mode: has taskbar on this monitor
    HWND                 taskbarHwnd = nullptr;            // embed mode: Shell_TrayWnd handle
    TaskbarSide          taskbarSide = TaskbarSide::Right; // embed mode: left or right side
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
    void SetPositionPreset(PositionPreset preset);
    void Rebuild();
    void SetShowMode(ShowMode mode);
    void ShowTemporarily();
    void SetAnimMode(bool on);
    void SetAutoContrast(bool on);
    void SetScrollSwitchCallback(std::function<void(int)> cb) { m_scrollSwitchFn = std::move(cb); }
    void SetMoveWindowCallback(std::function<void(int, HWND)> cb) { m_moveWindowFn = std::move(cb); }
    void UnembedTaskbarIndicator();
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
    HWND                           m_dragHwnd          = nullptr;
    bool                           m_draggingWindow    = false;
    bool                           m_pendingDrag       = false;
    int                            m_lastHoverSymbol   = -1;
    bool                           m_isTaskbarEmbedded = false;
    bool                           m_dragOverlayActive = false;

    void               ShowDragOverlay();
    void               HideDragOverlay();
    void               ApplyShowMode(ShowMode mode);
    void               SampleBackground();
    void               UpdateRenderTimer();
    void               ResetDragState();
    [[nodiscard]] bool NeedsSettleRender() const;
    void               StartBgSampleTimer();
    void               StopBgSampleTimer();
    void               RebuildText();
    std::wstring       BuildLayerColors(MonitorLayer &layer, const std::array<COLORREF, 5> &baseColors, size_t colorCount) const;
    void               RenderLayer(MonitorLayer &layer, HDC hdcScreen, HDC hdcMem);
    void               PresentLayer(MonitorLayer &layer, const SymbolMetrics &metrics,
                                    int centerW, const ColorArray &colors,
                                    HDC hdcScreen, HDC hdcMem);
    void               Render();
    void               ApplyPresetPosition(PositionPreset preset);
    void               RebuildToPreset(PositionPreset preset);
    void               MoveByDelta(int dx, int dy);
    void               EnumerateMonitors(HINSTANCE hInstance);
    void               RegisterMouseWheelInput();
    bool               HandleRawInput(HWND hwnd, LPARAM lp);
    bool               HandleDragStart(HWND hwnd, LPARAM lp);
    static void        InstallDragHook();
    static void        UninstallDragHook();
    bool               GetSymbolIndexAt(POINT screenPt, int &outIndex) const;

    // Taskbar embed helpers
    static void InstallTrayHook();
    static void UninstallTrayHook();

    static HHOOK             s_dragHook;
    static DesktopIndicator *s_instance;
    static HWINEVENTHOOK     s_eventHook;
    static LRESULT CALLBACK  DragHookProc(int nCode, WPARAM wParam, LPARAM lParam);

    static void CALLBACK    AutoHideTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
    static void CALLBACK    RenderTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
    static void CALLBACK    BgSampleTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
    static void CALLBACK    WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event,
                                         HWND hwnd, LONG idObject, LONG idChild,
                                         DWORD dwEventThread, DWORD dwmsEventTime);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT                 HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};
