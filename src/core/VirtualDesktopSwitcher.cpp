#include "VirtualDesktopSwitcher.h"

#include <bit>
#include <string>

#include "util/Log.h"
#include "util/VirtualDesktopHelper.h"

namespace {

void ActivateWindow(HWND hwnd) {
    HWND        foreHwnd     = GetForegroundWindow();
    DWORD       foreThreadId = 0;
    const DWORD curThreadId  = GetCurrentThreadId();
    BOOL        attached     = FALSE;

    if (foreHwnd != nullptr) {
        foreThreadId = GetWindowThreadProcessId(foreHwnd, nullptr);
        if (foreThreadId != 0 && foreThreadId != curThreadId) {
            AttachThreadInput(curThreadId, foreThreadId, TRUE);
            attached = TRUE;
        }
    }

    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);

    if (attached != FALSE) {
        AttachThreadInput(curThreadId, foreThreadId, FALSE);
    }
}

std::wstring GetWindowTitle(HWND hwnd) {
    std::wstring buffer(256, L'\0');
    const int    length = GetWindowTextW(hwnd, buffer.data(), static_cast<int>(buffer.size()));
    if (length <= 0) {
        return {};
    }
    buffer.resize(length);
    return buffer;
}

struct EnumContext {
    const VirtualDesktopHelper *pHelper;
    HWND                        skipWindow;
    HWND                        foundWindow;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto *ctx = std::bit_cast<EnumContext *>(lParam);

    if ((IsWindowVisible(hwnd) == 0) || hwnd == ctx->skipWindow) {
        return TRUE;
    }
    if (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) {
        return TRUE;
    }

    std::wstring title(256, L'\0');
    if (GetWindowTextW(hwnd, title.data(), 256) == 0) {
        return TRUE;
    }

    if (ctx->foundWindow == nullptr && ctx->pHelper != nullptr && ctx->pHelper->IsWindowOnCurrentDesktop(hwnd)) {
        ctx->foundWindow = hwnd;
    }
    return TRUE;
}

HWND FindTopWindowOnCurrentDesktop(const VirtualDesktopHelper *pHelper) {
    EnumContext ctx = {.pHelper = pHelper, .skipWindow = GetForegroundWindow(), .foundWindow = nullptr};
    EnumWindows(EnumWindowsProc, std::bit_cast<LPARAM>(&ctx));
    return ctx.foundWindow;
}

} // namespace

VirtualDesktopSwitcher *VirtualDesktopSwitcher::s_active = nullptr;

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

    const auto *pKeyboard  = std::bit_cast<const KBDLLHOOKSTRUCT *>(lParam);
    const bool  altPressed = (static_cast<USHORT>(GetAsyncKeyState(VK_MENU)) & 0x8000u) != 0;

    if (altPressed && pKeyboard->vkCode >= '1' && pKeyboard->vkCode <= '9') {
        const int desktopIndex = static_cast<int>(pKeyboard->vkCode - '1');
        PostMessage(s_active->m_hwnd, WM_SWITCH_DESKTOP, desktopIndex, 0);
        return 1;
    }

    return CallNextHookEx(s_active->m_hHook, nCode, wParam, lParam);
}

bool VirtualDesktopSwitcher::InstallHook() {
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
    return InstallHook();
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

void VirtualDesktopSwitcher::RecordForeground(int desktopIndex) {
    if (desktopIndex < 0 || desktopIndex >= kMaxDesktops) {
        return;
    }

    HWND foreground = GetForegroundWindow();
    if (foreground == nullptr || IsWindow(foreground) == FALSE) {
        return;
    }

    if (m_pVDeskHelper->IsWindowOnCurrentDesktop(foreground)) {
        m_desktopLastForeground.at(desktopIndex) = foreground;
    } else {
        m_desktopLastForeground.at(desktopIndex) = nullptr;
    }
}

bool VirtualDesktopSwitcher::TryActivatePreviousWindow(int desktopIndex) {
    if (desktopIndex < 0 || desktopIndex >= kMaxDesktops) {
        return false;
    }

    HWND targetHwnd = m_desktopLastForeground.at(desktopIndex);
    if (targetHwnd == nullptr) {
        return false;
    }

    if (IsWindow(targetHwnd) == FALSE) {
        Log(L"[DEBUG] Window on desktop " + std::to_wstring(desktopIndex) + L" is invalid: " + GetWindowTitle(targetHwnd));
        m_desktopLastForeground.at(desktopIndex) = nullptr;
        return false;
    }

    if (!m_pVDeskHelper->IsWindowOnCurrentDesktop(targetHwnd)) {
        Log(L"[DEBUG] Window on desktop " + std::to_wstring(desktopIndex) + L" not on current desktop: " + GetWindowTitle(targetHwnd));
        m_desktopLastForeground.at(desktopIndex) = nullptr;
        return false;
    }

    if (IsIconic(targetHwnd) != 0) {
        ShowWindow(targetHwnd, SW_RESTORE);
    }

    ActivateWindow(targetHwnd);
    Sleep(100);

    if (GetForegroundWindow() == targetHwnd) {
        Log(L"[DEBUG] Activated previous window on desktop " + std::to_wstring(desktopIndex) + L": " + GetWindowTitle(targetHwnd));
        return true;
    }

    Log(L"[DEBUG] Failed to activate previous window on desktop " + std::to_wstring(desktopIndex) + L": " + GetWindowTitle(targetHwnd));
    return false;
}

void VirtualDesktopSwitcher::ActivateFallbackWindow(int desktopIndex) {
    HWND hwnd = FindTopWindowOnCurrentDesktop(m_pVDeskHelper.get());
    if (hwnd != nullptr) {
        Log(L"[DEBUG] Activating fallback window on desktop " + std::to_wstring(desktopIndex) + L": " + GetWindowTitle(hwnd));
        ActivateWindow(hwnd);
    } else {
        Log(L"[DEBUG] No window to activate on desktop " + std::to_wstring(desktopIndex));
    }
}

void VirtualDesktopSwitcher::SwitchToDesktop(int index) {
    if (index < 0 || m_pVDeskHelper == nullptr) {
        return;
    }

    const int currentIndex = GetCurrentDesktopIndex();
    RecordForeground(currentIndex);

    m_pVDeskHelper->SwitchToDesktop(index);
    Sleep(100);

    if (index == currentIndex) {
        Log(L"[DEBUG] Switched to same desktop " + std::to_wstring(index) + L", no activation needed");
        return;
    }

    if (TryActivatePreviousWindow(index)) {
        return;
    }
    ActivateFallbackWindow(index);
}
