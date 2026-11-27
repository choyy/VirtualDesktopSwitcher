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
    HWINEVENTHOOK     m_hEventHook       = nullptr;
    int               m_lastHoverSymbol  = -1;
    bool              m_clickPending     = false;
    bool              m_isDraggingWindow = false;
    bool              m_isNativeDrag     = false;
    HWND              m_dragHwnd         = nullptr;
    bool              m_enableDragSwitch = false;
    DragSwitchMode    m_dragMode         = DragSwitchMode::Always;

    // 第三方拖拽工具（如 AltSnap）不进入原生 GUI_INMOVESIZE 循环，
    // 通过 EVENT_SYSTEM_MOVESIZESTART / EVENT_SYSTEM_MOVESIZEEND 跟踪
    bool m_eventMoveSize    = false;
    HWND m_eventHwnd        = nullptr;
    RECT m_eventLastRect    = {};
    bool m_hasEventLastRect = false;

    static WindowDragHandler *s_instance;
    static LRESULT CALLBACK   DragHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static void CALLBACK      MoveSizeEventProc(HWINEVENTHOOK hWinEventHook, DWORD event,
                                                HWND hwnd, LONG idObject, LONG idChild,
                                                DWORD dwEventThread, DWORD dwmsEventTime);
    void                      OnMouseMove(POINT pt);
    void                      TryBeginEventDrag();
    void                      BeginDrag(HWND hwnd, bool isNative);
    void                      UpdateDragHover(POINT pt);
    void                      OnMoveSizeStart(HWND hwnd);
    void                      OnMoveSizeEnd();
    void                      ResetDragState();
    [[nodiscard]] bool        IsDragStillActive(bool inNativeMoveSize) const;
    [[nodiscard]] bool        IsDragSwitch() const;
};
