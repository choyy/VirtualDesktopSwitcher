#ifndef VIRTUAL_DESKTOP_SWITCHER_H
#define VIRTUAL_DESKTOP_SWITCHER_H
#include "util/VirtualDesktopHelper.h"

#include <array>
#include <memory>

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

    void                                         SetWindowHandle(HWND hwnd) { m_hwnd = hwnd; }
    [[nodiscard]] HWND                           GetWindowHandle() const { return m_hwnd; }
    [[nodiscard]] int                            GetDesktopCount() const { return m_pVDeskHelper ? m_pVDeskHelper->GetDesktopCount() : 0; }
    [[nodiscard]] int                            GetCurrentDesktopIndex() const { return m_pVDeskHelper ? m_pVDeskHelper->GetCurrentDesktopIndex() : -1; }
    [[nodiscard]] std::array<bool, kMaxDesktops> GetDesktopEmptyMask() const { return m_pVDeskHelper ? m_pVDeskHelper->GetDesktopEmptyMask() : std::array<bool, kMaxDesktops>{}; }

private:
    std::unique_ptr<VirtualDesktopHelper> m_pVDeskHelper;
    HHOOK                                 m_hHook = nullptr;
    HWND                                  m_hwnd  = nullptr;
    std::array<HWND, kMaxDesktops>        m_desktopLastForeground{};

    static VirtualDesktopSwitcher *s_active;

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    void               RecordForeground(int desktopIndex);
    [[nodiscard]] bool TryActivatePreviousWindow(int desktopIndex);
    void               ActivateFallbackWindow(int desktopIndex);
};

#endif
