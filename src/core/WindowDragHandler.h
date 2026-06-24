#pragma once

#include <windows.h>

#include "core/IndicatorConfig.h"

class DesktopIndicator;

constexpr UINT WM_WINDOW_DRAG_DROP = WM_USER + 60;

class WindowDragHandler {
public:
    WindowDragHandler(const WindowDragHandler &)            = delete;
    WindowDragHandler &operator=(const WindowDragHandler &) = delete;
    WindowDragHandler(WindowDragHandler &&)                 = delete;
    WindowDragHandler &operator=(WindowDragHandler &&)      = delete;

    explicit WindowDragHandler(DesktopIndicator *indicator);
    ~WindowDragHandler();

    bool InstallHook(HWND hwndTarget);
    void UninstallHook();
    void SetDragSwitchMode(DragSwitchMode mode) { m_dragMode = mode; }

private:
    DesktopIndicator *m_indicator;
    HWND              m_hwndTarget       = nullptr;
    HHOOK             m_hHook            = nullptr;
    int               m_lastHoverSymbol  = -1;
    bool              m_clickPending     = false;
    bool              m_isDraggingWindow = false;
    HWND              m_dragHwnd         = nullptr;
    bool              m_enableDragSwitch = false;
    DragSwitchMode    m_dragMode         = DragSwitchMode::Always;

    static WindowDragHandler *s_instance;
    static LRESULT CALLBACK   DragHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    void                      ResetDragState();
    bool                      IsDragSwitch() const;
};
