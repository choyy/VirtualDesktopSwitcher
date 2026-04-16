#include "VirtualDesktopSwitcher.h"

VirtualDesktopSwitcher *VirtualDesktopSwitcher::GetInstance() {
    static VirtualDesktopSwitcher instance;
    return &instance;
}

LRESULT CALLBACK VirtualDesktopSwitcher::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    const VirtualDesktopSwitcher *pInstance = VirtualDesktopSwitcher::GetInstance();
    if (pInstance == nullptr) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    if (nCode == HC_ACTION) {
        const auto *pKeyboard = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam); // NOLINT(performance-no-int-to-ptr, cppcoreguidelines-pro-type-reinterpret-cast)

        // 检查是否按下了 Alt + 数字键
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            const bool isAltPressed = (static_cast<uint16_t>(GetAsyncKeyState(VK_MENU)) & 0x8000u) != 0;

            if (isAltPressed && pKeyboard->vkCode >= '1' && pKeyboard->vkCode <= '9') {
                const int desktopIndex = static_cast<int>(pKeyboard->vkCode - '1'); // 将字符转换为索引(0-8)

                // 使用消息队列延迟执行桌面切换
                PostMessage(pInstance->GetWindowHandle(), WM_SWITCH_DESKTOP, desktopIndex, 0);

                // 阻止原始按键事件传递
                return 1;
            }
        }
    }

    return CallNextHookEx(pInstance->GetHook(), nCode, wParam, lParam);
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

void VirtualDesktopSwitcher::SwitchToDesktop(int index) {
    if (index >= 0 && m_pVDeskHelper != nullptr) {
        m_pVDeskHelper->SwitchToDesktop(index);
    }
}
