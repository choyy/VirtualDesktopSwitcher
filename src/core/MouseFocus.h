#pragma once

#include <windows.h>

#include <functional>

class MouseFocus {
public:
    MouseFocus(const MouseFocus &)            = delete;
    MouseFocus &operator=(const MouseFocus &) = delete;
    MouseFocus(MouseFocus &&)                 = delete;
    MouseFocus &operator=(MouseFocus &&)      = delete;

    MouseFocus();
    ~MouseFocus();

    void UpdateHook();
    void SetActivateFn(std::function<void(HMONITOR)> fn) { m_activateFn = std::move(fn); }

private:
    HHOOK                         m_hHook         = nullptr;
    HMONITOR                      m_lastMonitor   = nullptr;
    ULONGLONG                     m_lastFocusTick = 0;
    std::function<void(HMONITOR)> m_activateFn;

    static MouseFocus      *s_instance;
    static LRESULT CALLBACK LowLevelMouseFocusProc(int nCode, WPARAM wParam, LPARAM lParam);
};
