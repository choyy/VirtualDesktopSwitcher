#pragma once

#include <windows.h>

#include <array>
#include <memory>

#include "util/Utils.h"

constexpr UINT WM_SWITCH_DESKTOP = WM_USER + 1;

enum class ModKey : std::uint8_t { Alt,
                                   Ctrl,
                                   CtrlAlt,
                                   AltShift };

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
    void RefreshCOM();

    void        SetWindowHandle(HWND hwnd) { m_hwnd = hwnd; }
    static void SetModKey(ModKey mod) { s_modKey = mod; }

    [[nodiscard]] int                            GetDesktopCount() const;
    [[nodiscard]] int                            GetCurrentDesktopIndex() const;
    [[nodiscard]] std::array<bool, kMaxDesktops> GetDesktopEmptyMask() const;

private:
    std::unique_ptr<class VirtualDesktopHelper> m_pVDeskHelper;
    HHOOK                                       m_hHook = nullptr;
    HWND                                        m_hwnd  = nullptr;

    static VirtualDesktopSwitcher *s_active;
    static ModKey                  s_modKey;

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
};
