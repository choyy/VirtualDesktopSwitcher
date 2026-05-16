#include "VirtualDesktopSwitcher.h"

#include <dwmapi.h>

#include <string>

#include "core/VirtualDesktopHelper.h"
#include "util/Log.h"
#include "util/Utils.h"

namespace {

struct FindContext {
    const VirtualDesktopHelper *pHelper;
    HWND                        foundWindow;
};

BOOL CALLBACK FindTopWndProc(HWND hwnd, LPARAM lParam) {
    auto *ctx = LParamToPtr<FindContext>(lParam);
    if (IsWindowVisible(hwnd) == 0) { return TRUE; }
    if (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) { return TRUE; }
    std::wstring title(256, L'\0');
    if (GetWindowTextW(hwnd, title.data(), 256) == 0) { return TRUE; }

    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && (cloaked != 0)) {
        return TRUE;
    }

    if (ctx->foundWindow == nullptr && ctx->pHelper->IsWindowOnCurrentDesktop(hwnd)) {
        ctx->foundWindow = hwnd;
    }
    return TRUE;
}

HWND FindTopOnDesktop(const VirtualDesktopHelper *pHelper) {
    FindContext ctx = {.pHelper = pHelper, .foundWindow = nullptr};
    EnumWindows(FindTopWndProc, PtrToLParam(&ctx));
    return ctx.foundWindow;
}

} // namespace

VirtualDesktopSwitcher *VirtualDesktopSwitcher::s_active = nullptr;
ModKey                  VirtualDesktopSwitcher::s_modKey = ModKey::Alt;

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

    bool match = false;
    switch (s_modKey) {
    case ModKey::Alt: match = alt && !ctrl && !shift; break;
    case ModKey::Ctrl: match = ctrl && !alt && !shift; break;
    case ModKey::CtrlAlt: match = ctrl && alt && !shift; break;
    case ModKey::AltShift: match = alt && shift && !ctrl; break;
    }

    if (match && pKeyboard->vkCode >= '1' && pKeyboard->vkCode <= '9') {
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

void VirtualDesktopSwitcher::RefreshCOM() {
    if (m_pVDeskHelper) { m_pVDeskHelper->Refresh(); }
}

void VirtualDesktopSwitcher::SwitchToDesktop(int index) {
    if (index < 0 || m_pVDeskHelper == nullptr) { return; }

    const int currentIndex = GetCurrentDesktopIndex();
    if (index == currentIndex) { return; }

    m_pVDeskHelper->SwitchToDesktop(index);
    for (int retry = 0; retry < 20; ++retry) {
        if (GetCurrentDesktopIndex() == index) { break; }
        Sleep(10);
    }

    HWND hwnd = FindTopOnDesktop(m_pVDeskHelper.get());
    if (hwnd == nullptr) {
        hwnd = GetForegroundWindow();
    }
    if (hwnd != nullptr) {
        if (IsIconic(hwnd) != 0) { ShowWindow(hwnd, SW_RESTORE); }
        ActivateWindow(hwnd);
        if (hwnd != GetShellWindow()) {
            Log(L"[DEBUG] SwitchToDesktop: index=" + std::to_wstring(index)
                + L" activated=" + GetWindowTitle(hwnd));
        } else {
            Log(L"[DEBUG] SwitchToDesktop: index=" + std::to_wstring(index) + L" empty desktop");
        }
    } else {
        Log(L"[DEBUG] SwitchToDesktop: index=" + std::to_wstring(index) + L" no window");
    }
}
