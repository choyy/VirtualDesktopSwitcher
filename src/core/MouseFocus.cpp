#include "MouseFocus.h"

#include "util/Log.h"

MouseFocus *MouseFocus::s_instance = nullptr;

MouseFocus::MouseFocus() {
    s_instance = this;
}

MouseFocus::~MouseFocus() {
    if (m_hHook != nullptr) {
        UnhookWindowsHookEx(m_hHook);
    }
    if (s_instance == this) { s_instance = nullptr; }
}

void MouseFocus::UpdateHook() {
    const bool multi = GetSystemMetrics(SM_CMONITORS) > 1;

    if (multi && m_hHook == nullptr) {
        m_hHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseFocusProc,
                                    GetModuleHandleW(nullptr), 0);
        if (m_hHook != nullptr) {
            Log(L"[INFO] MouseFocus hook installed");
        } else {
            Log(L"[ERROR] MouseFocus hook install failed");
        }
    } else if (!multi && m_hHook != nullptr) {
        UnhookWindowsHookEx(m_hHook);
        m_hHook       = nullptr;
        m_lastMonitor = nullptr;
        Log(L"[INFO] MouseFocus hook uninstalled (single monitor)");
    }
}

LRESULT CALLBACK MouseFocus::LowLevelMouseFocusProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || s_instance == nullptr) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    if (wParam == WM_MOUSEMOVE) {
        auto *hs = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)

        if ((static_cast<UINT>(GetAsyncKeyState(VK_LBUTTON)) & 0x8000u) != 0 || (static_cast<UINT>(GetAsyncKeyState(VK_RBUTTON)) & 0x8000u) != 0) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        HMONITOR hMon = MonitorFromPoint(hs->pt, MONITOR_DEFAULTTONULL);
        if (hMon == nullptr || hMon == s_instance->m_lastMonitor) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        ULONGLONG now = GetTickCount64();
        if (now - s_instance->m_lastFocusTick < 200) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        s_instance->m_lastMonitor   = hMon;
        s_instance->m_lastFocusTick = now;
        if (s_instance->m_activateFn) {
            s_instance->m_activateFn(hMon);
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
