#include "VirtualDesktopSwitcher.h"

#include <dwmapi.h>

#include <string>

#include "core/VirtualDesktopHelper.h"
#include "util/Log.h"
#include "util/Utils.h"

namespace {

struct FindWindowCtx {
    HMONITOR hMonitor; // nullptr = 跳过
    HWND     foundWindow;
};

bool IsValidTopWindow(HWND hwnd) {
    if (IsWindowVisible(hwnd) == 0) { return false; }
    if ((GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0) { return false; }

    std::array<wchar_t, 256> title{};
    if (GetWindowTextW(hwnd, title.data(), static_cast<int>(title.size())) == 0) { return false; }

    BOOL       cloaked   = FALSE;
    const bool isCloaked = SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) != FALSE
                           && cloaked != FALSE;
    return !isCloaked;
}

BOOL CALLBACK FindTopWndProc(HWND hwnd, LPARAM lParam) {
    auto *ctx = LParamToPtr<FindWindowCtx>(lParam);
    if (!IsValidTopWindow(hwnd)) { return TRUE; }
    if (IsIconic(hwnd) != 0) { return TRUE; }
    if (ctx->hMonitor != nullptr) {
        RECT wr;
        GetWindowRect(hwnd, &wr);
        if (MonitorFromRect(&wr, MONITOR_DEFAULTTONULL) != ctx->hMonitor) { return TRUE; }
    }
    if (ctx->foundWindow == nullptr) {
        ctx->foundWindow = hwnd;
    }
    return TRUE;
}

HWND FindTopWindowOnMonitor(HMONITOR hMon) {
    if (hMon == nullptr) { return nullptr; }
    FindWindowCtx ctx = {.hMonitor = hMon, .foundWindow = nullptr};
    EnumWindows(FindTopWndProc, PtrToLParam(&ctx));
    return ctx.foundWindow;
}

} // namespace

VirtualDesktopSwitcher           *VirtualDesktopSwitcher::s_active            = nullptr;
ModMask                           VirtualDesktopSwitcher::s_modMask           = ModMask::Alt;
uint8_t                           VirtualDesktopSwitcher::s_prevDesktopKey    = VK_OEM_3;
uint8_t                           VirtualDesktopSwitcher::s_pinAllDesktopsKey = 'D';
std::array<uint8_t, kMaxDesktops> VirtualDesktopSwitcher::s_desktopKeys       = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

VirtualDesktopSwitcher::VirtualDesktopSwitcher()
    : m_pVDeskHelper(std::make_unique<VirtualDesktopHelper>()) {
    s_active = this;
}

VirtualDesktopSwitcher::~VirtualDesktopSwitcher() {
    s_active = nullptr;
    UninstallHook();
}

LRESULT CALLBACK VirtualDesktopSwitcher::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode != HC_ACTION || s_active == nullptr) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    if (wParam != WM_KEYDOWN && wParam != WM_SYSKEYDOWN) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    const auto *pKeyboard = LParamToPtr<const KBDLLHOOKSTRUCT>(lParam);

    const bool alt   = (pKeyboard->flags & LLKHF_ALTDOWN) != 0 || (static_cast<UINT>(GetAsyncKeyState(VK_MENU)) & 0x8000u) != 0;
    const bool ctrl  = (static_cast<UINT>(GetAsyncKeyState(VK_CONTROL)) & 0x8000u) != 0;
    const bool shift = (static_cast<UINT>(GetAsyncKeyState(VK_SHIFT)) & 0x8000u) != 0;
    const bool win   = (static_cast<UINT>(GetAsyncKeyState(VK_LWIN)) & 0x8000u) != 0 || (static_cast<UINT>(GetAsyncKeyState(VK_RWIN)) & 0x8000u) != 0;

    uint8_t held  = (alt ? ModMask::Alt : 0u) | (ctrl ? ModMask::Ctrl : 0u) | (shift ? ModMask::Shift : 0u) | (win ? ModMask::Win : 0u);
    bool    match = held != 0 && held == s_modMask;

    if (match) {
        for (int i = 0; i < static_cast<int>(kMaxDesktops); ++i) {
            if (pKeyboard->vkCode == s_desktopKeys.at(i)) {
                PostMessage(s_active->m_hwnd, WM_SWITCH_DESKTOP, i, 0);
                return 1;
            }
        }
    }

    if (match && pKeyboard->vkCode == s_prevDesktopKey && s_active->m_previousDesktop >= 0) {
        PostMessage(s_active->m_hwnd, WM_SWITCH_DESKTOP, s_active->m_previousDesktop, 0);
        return 1;
    }

    if (match && pKeyboard->vkCode == s_pinAllDesktopsKey) {
        PostMessage(s_active->m_hwnd, WM_TOGGLE_PIN_ALL_DESKTOPS, 0, 0);
        return 1;
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

bool VirtualDesktopSwitcher::InstallHook(HWND hwnd) {
    m_hwnd  = hwnd;
    m_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
    return m_hHook != nullptr;
}

void VirtualDesktopSwitcher::UninstallHook() {
    if (m_hHook != nullptr) {
        UnhookWindowsHookEx(m_hHook);
        m_hHook = nullptr;
    }
}

bool VirtualDesktopSwitcher::ReinstallHook() {
    UninstallHook();
    return InstallHook(m_hwnd);
}

int VirtualDesktopSwitcher::GetDesktopCount() const {
    return m_pVDeskHelper ? m_pVDeskHelper->GetDesktopCount() : 0;
}

int VirtualDesktopSwitcher::GetCurrentDesktopIndex() const {
    return m_pVDeskHelper ? m_pVDeskHelper->GetCurrentDesktopIndex() : -1;
}

std::array<bool, kMaxDesktops> VirtualDesktopSwitcher::GetDesktopEmptyMask() const {
    return m_pVDeskHelper ? m_pVDeskHelper->GetDesktopEmptyMask() : std::array<bool, kMaxDesktops>{};
}

void VirtualDesktopSwitcher::RefreshCOM() {
    if (m_pVDeskHelper) { m_pVDeskHelper->Refresh(); }
}

void VirtualDesktopSwitcher::SwitchToDesktop(int index) {
    if (index < 0 || m_pVDeskHelper == nullptr) { return; }

    const int currentIndex = GetCurrentDesktopIndex();
    if (index == currentIndex) { return; }
    m_previousDesktop = currentIndex;

    m_pVDeskHelper->SwitchToDesktop(index);
    for (int retry = 0; retry < 20; ++retry) {
        if (GetCurrentDesktopIndex() == index) { break; }
        Sleep(10);
    }
}

void VirtualDesktopSwitcher::MoveWindowToDesktop(HWND hwnd, int targetIndex) {
    if (hwnd == nullptr || m_pVDeskHelper == nullptr) { return; }
    m_pVDeskHelper->MoveWindowToDesktop(hwnd, targetIndex);
}

void VirtualDesktopSwitcher::PinWindow(HWND hwnd) const {
    if (hwnd == nullptr || m_pVDeskHelper == nullptr) { return; }
    m_pVDeskHelper->PinWindow(hwnd);
}

void VirtualDesktopSwitcher::UnpinWindow(HWND hwnd) const {
    if (hwnd == nullptr || m_pVDeskHelper == nullptr) { return; }
    m_pVDeskHelper->UnpinWindow(hwnd);
}

bool VirtualDesktopSwitcher::IsWindowPinned(HWND hwnd) const {
    if (hwnd == nullptr || m_pVDeskHelper == nullptr) { return false; }
    return m_pVDeskHelper->IsWindowPinned(hwnd);
}

void VirtualDesktopSwitcher::SetPinByApp(bool use) {
    if (m_pVDeskHelper) { m_pVDeskHelper->SetPinByApp(use); }
}

bool VirtualDesktopSwitcher::ActivateTopWindowOnMonitor(HMONITOR hMon) {
    HWND hwnd = FindTopWindowOnMonitor(hMon);
    if (hwnd == nullptr || hwnd == GetShellWindow()) {
        Log(L"[DEBUG] ActivateTopWindowOnMonitor: no window");
        return false;
    }
    for (int retry = 0; retry < 3; ++retry) {
        ActivateWindow(hwnd);
        if (GetForegroundWindow() == hwnd) {
            Log(L"[DEBUG] ActivateTopWindowOnMonitor: " + GetWindowTitle(hwnd));
            return true;
        }
        Sleep(30);
    }
    Log(L"[DEBUG] ActivateTopWindowOnMonitor: failed for " + GetWindowTitle(hwnd));
    return false;
}
