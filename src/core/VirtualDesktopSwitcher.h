#pragma once

#include <windows.h>

#include <array>
#include <memory>

#include "util/Utils.h"

constexpr UINT WM_SWITCH_DESKTOP          = WM_USER + 1;
constexpr UINT WM_TOGGLE_PIN_ALL_DESKTOPS = WM_USER + 20;

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
    void PinWindow(HWND hwnd) const;
    void UnpinWindow(HWND hwnd) const;
    bool IsWindowPinned(HWND hwnd) const;
    void SetPinByApp(bool use);
    void RefreshCOM();

    static void SetModMask(uint8_t mask) { s_modMask = static_cast<ModMask>(mask); }
    static void SetPrevDesktopKey(uint8_t vk) { s_prevDesktopKey = vk; }
    static void SetPinAllDesktopsKey(uint8_t vk) { s_pinAllDesktopsKey = vk; }
    static void SetDesktopKey(int index, uint8_t vk) {
        if (index >= 0 && index < static_cast<int>(kMaxDesktops)) { s_desktopKeys.at(index) = vk; }
    }

    [[nodiscard]] int                            GetDesktopCount() const;
    [[nodiscard]] int                            GetCurrentDesktopIndex() const;
    [[nodiscard]] std::array<bool, kMaxDesktops> GetDesktopEmptyMask() const;

private:
    std::unique_ptr<class VirtualDesktopHelper> m_pVDeskHelper;
    HHOOK                                       m_hHook           = nullptr;
    HWND                                        m_hwnd            = nullptr;
    int                                         m_previousDesktop = -1;

    static VirtualDesktopSwitcher           *s_active;
    static ModMask                           s_modMask;
    static uint8_t                           s_prevDesktopKey;
    static uint8_t                           s_pinAllDesktopsKey;
    static std::array<uint8_t, kMaxDesktops> s_desktopKeys;

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
};
