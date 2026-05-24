#pragma once

#include <windows.h>

#include <array>
#include <memory>

#include "util/Utils.h"

constexpr UINT WM_SWITCH_DESKTOP = WM_USER + 1;

enum ModMask : uint8_t {
    Alt   = 1,
    Ctrl  = 2,
    Shift = 4,
    Win   = 8,
};

class VirtualDesktopSwitcher {
public:
    VirtualDesktopSwitcher(const VirtualDesktopSwitcher &)            = delete;
    VirtualDesktopSwitcher &operator=(const VirtualDesktopSwitcher &) = delete;
    VirtualDesktopSwitcher(VirtualDesktopSwitcher &&)                 = delete;
    VirtualDesktopSwitcher &operator=(VirtualDesktopSwitcher &&)      = delete;

    VirtualDesktopSwitcher();
    ~VirtualDesktopSwitcher();

    bool InstallHook(HWND hwnd);
    void UninstallHook();
    bool ReinstallHook();
    void SwitchToDesktop(int index);
    void MoveWindowToDesktop(HWND hwnd, int targetIndex);
    void RefreshCOM();

    static void SetModMask(uint8_t mask) { s_modMask = static_cast<ModMask>(mask); }

    [[nodiscard]] int                            GetDesktopCount() const;
    [[nodiscard]] int                            GetCurrentDesktopIndex() const;
    [[nodiscard]] std::array<bool, kMaxDesktops> GetDesktopEmptyMask() const;

private:
    std::unique_ptr<class VirtualDesktopHelper> m_pVDeskHelper;
    HHOOK                                       m_hHook = nullptr;
    HWND                                        m_hwnd  = nullptr;

    static VirtualDesktopSwitcher *s_active;
    static ModMask                 s_modMask;

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
};
