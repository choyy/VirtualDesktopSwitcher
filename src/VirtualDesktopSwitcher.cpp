#include "VirtualDesktopSwitcher.h"

#include <iostream>
#include <string>

namespace {
std::string GetWindowTitleUtf8(HWND hwnd) {
    std::wstring buffer;
    buffer.resize(256);
    const int length = GetWindowTextW(hwnd, buffer.data(), 256);
    if (length == 0) { return {}; }

    buffer.resize(length);
    const int size = WideCharToMultiByte(CP_UTF8, 0, buffer.c_str(), length, nullptr, 0, nullptr, nullptr);
    if (size <= 0) { return {}; }

    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer.c_str(), length, result.data(), size, nullptr, nullptr);
    return result;
}

struct EnumContext {
    const VirtualDesktopHelper *pHelper;
    HWND                        currentForeground;
    HWND                        nextTarget;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto *ctx = reinterpret_cast<EnumContext *>(lParam); // NOLINT(performance-no-int-to-ptr, cppcoreguidelines-pro-type-reinterpret-cast)

    if ((IsWindowVisible(hwnd) == FALSE) || ((GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0) || hwnd == ctx->currentForeground) {
        return TRUE;
    }

    std::wstring windowTitle;
    windowTitle.resize(256);
    if (GetWindowTextW(hwnd, windowTitle.data(), 256) == 0) {
        return TRUE;
    }

    if (ctx->nextTarget == nullptr && ctx->pHelper != nullptr && ctx->pHelper->IsWindowOnCurrentDesktop(hwnd)) {
        ctx->nextTarget = hwnd;
    }

    return TRUE;
}

HWND FindTopWindowOnCurrentDesktop(const VirtualDesktopHelper *pHelper) {
    EnumContext ctx = {pHelper, GetForegroundWindow(), nullptr};
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx)); // NOLINT(performance-no-int-to-ptr, cppcoreguidelines-pro-type-reinterpret-cast)
    return ctx.nextTarget;
}
} // namespace

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

void VirtualDesktopSwitcher::RecordForeground(int desktopIndex) {
    if (desktopIndex < 0 || desktopIndex >= kMaxDesktops) {
        return;
    }
    HWND foreground = GetForegroundWindow();
    if (foreground != nullptr && IsWindow(foreground) != FALSE) {
        if (m_pVDeskHelper->IsWindowOnCurrentDesktop(foreground)) {
            m_desktopLastForeground[desktopIndex] = foreground;
        } else {
            m_desktopLastForeground[desktopIndex] = nullptr;
        }
    }
}

bool VirtualDesktopSwitcher::TryActivatePreviousWindow(int desktopIndex) {
    if (desktopIndex < 0 || desktopIndex >= kMaxDesktops) {
        return false;
    }

    HWND targetHwnd = m_desktopLastForeground[desktopIndex];
    if (targetHwnd == nullptr) {
        return false;
    }

    if (IsWindow(targetHwnd) == FALSE) {
        std::cout << "Previous foreground window on desktop " << desktopIndex << " is no longer valid: "
                  << GetWindowTitleUtf8(targetHwnd) << " (" << targetHwnd << ")\n";
        m_desktopLastForeground[desktopIndex] = nullptr;
        return false;
    }

    if (!m_pVDeskHelper->IsWindowOnCurrentDesktop(targetHwnd)) {
        std::cout << "Previous foreground window on desktop " << desktopIndex << " is not on current desktop: "
                  << GetWindowTitleUtf8(targetHwnd) << " (" << targetHwnd << ")\n";
        m_desktopLastForeground[desktopIndex] = nullptr;
        return false;
    }

    if (IsIconic(targetHwnd) != FALSE) {
        ShowWindow(targetHwnd, SW_RESTORE);
    }

    HWND foreHwnd = GetForegroundWindow();
    DWORD foreThreadId = 0;
    DWORD curThreadId = GetCurrentThreadId();
    BOOL attached = FALSE;

    if (foreHwnd != nullptr) {
        foreThreadId = GetWindowThreadProcessId(foreHwnd, nullptr);
        if (foreThreadId != 0 && foreThreadId != curThreadId) {
            AttachThreadInput(curThreadId, foreThreadId, TRUE);
            attached = TRUE;
        }
    }

    BringWindowToTop(targetHwnd);
    SetForegroundWindow(targetHwnd);

    if (attached) {
        AttachThreadInput(curThreadId, foreThreadId, FALSE);
    }

    Sleep(100);

    if (GetForegroundWindow() == targetHwnd) {
        std::cout << "Activating previous foreground window on desktop " << desktopIndex << ": "
                  << GetWindowTitleUtf8(targetHwnd) << " (" << targetHwnd << ")\n";
        return true;
    }

    std::cout << "Failed to activate previous foreground window on desktop " << desktopIndex << ": "
              << GetWindowTitleUtf8(targetHwnd) << " (" << targetHwnd << ")\n";
    return false;
}

void VirtualDesktopSwitcher::ActivateFallbackWindow(int desktopIndex) {
    HWND hwnd = FindTopWindowOnCurrentDesktop(m_pVDeskHelper.get());
    if (hwnd != nullptr) {
        std::cout << "Activating fallback window on desktop " << desktopIndex << ": "
                  << GetWindowTitleUtf8(hwnd) << " (" << hwnd << ")\n";

        HWND foreHwnd = GetForegroundWindow();
        DWORD foreThreadId = 0;
        DWORD curThreadId = GetCurrentThreadId();
        BOOL attached = FALSE;

        if (foreHwnd != nullptr) {
            foreThreadId = GetWindowThreadProcessId(foreHwnd, nullptr);
            if (foreThreadId != 0 && foreThreadId != curThreadId) {
                AttachThreadInput(curThreadId, foreThreadId, TRUE);
                attached = TRUE;
            }
        }

        BringWindowToTop(hwnd);
        SetForegroundWindow(hwnd);

        if (attached) {
            AttachThreadInput(curThreadId, foreThreadId, FALSE);
        }
    } else {
        std::cout << "No window to activate on desktop " << desktopIndex << '\n';
    }
}

void VirtualDesktopSwitcher::SwitchToDesktop(int index) {
    if (index < 0 || m_pVDeskHelper == nullptr) { return; }

    const int currentIndex = GetCurrentDesktopIndex();
    RecordForeground(currentIndex);

    m_pVDeskHelper->SwitchToDesktop(index);
    Sleep(100);

    if (index == currentIndex) {
        std::cout << "Switched to same desktop " << index << ", no activation needed.\n";
        return;
    }

    if (TryActivatePreviousWindow(index)) { return; }

    ActivateFallbackWindow(index);
}
