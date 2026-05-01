#pragma once

#include <windows.h>

#include <array>
#include <memory>

#include "util/Utils.h"

constexpr UINT WM_SWITCH_DESKTOP = WM_USER + 1;

class VirtualDesktopSwitcher {
public:
    VirtualDesktopSwitcher(const VirtualDesktopSwitcher &)            = delete;
    VirtualDesktopSwitcher &operator=(const VirtualDesktopSwitcher &) = delete;
    VirtualDesktopSwitcher(VirtualDesktopSwitcher &&)                 = delete;
    VirtualDesktopSwitcher &operator=(VirtualDesktopSwitcher &&)      = delete;

    VirtualDesktopSwitcher();
    ~VirtualDesktopSwitcher();

    bool InstallHook();
    void UninstallHook();
    bool ReinstallHook();
    void SwitchToDesktop(int index);

    void SetWindowHandle(HWND hwnd) { m_hwnd = hwnd; }

    [[nodiscard]] int                            GetDesktopCount() const;
    [[nodiscard]] int                            GetCurrentDesktopIndex() const;
    [[nodiscard]] std::array<bool, kMaxDesktops> GetDesktopEmptyMask() const;

private:
    std::unique_ptr<class VirtualDesktopHelper> m_pVDeskHelper;
    HHOOK                                       m_hHook = nullptr;
    HWND                                        m_hwnd  = nullptr;
    std::array<HWND, kMaxDesktops>              m_desktopLastForeground{};

    static VirtualDesktopSwitcher *s_active;

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    void               RecordForeground(int desktopIndex);
    [[nodiscard]] bool TryActivatePreviousWindow(int desktopIndex);
    void               ActivateFallbackWindow(int desktopIndex);
};
