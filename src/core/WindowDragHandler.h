#pragma once

#include <windows.h>

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

private:
    DesktopIndicator *m_indicator;
    HWND              m_hwndTarget      = nullptr;
    HHOOK             m_hHook           = nullptr;
    bool              m_pendingDrag     = false;
    bool              m_dragging        = false;
    HWND              m_dragHwnd        = nullptr;
    int               m_lastHoverSymbol = -1;

    static WindowDragHandler *s_instance;
    static LRESULT CALLBACK   DragHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    void                      ResetDragState();
};
