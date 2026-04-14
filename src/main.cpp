#include <iostream>

#include "VirtualDesktopSwitcher.h"

int main() {
    std::cout << "Windows 11 Virtual Desktop Switcher\n";
    std::cout << "Press Alt + [1-9] to switch to corresponding virtual desktop.\n";
    std::cout << "Press ESC to exit.\n";

    // 获取VirtualDesktopSwitcher单例实例
    VirtualDesktopSwitcher *pSwitcher = VirtualDesktopSwitcher::GetInstance();

    // 创建一个隐藏窗口用于接收消息
    HWND hwnd = CreateWindowEx(0, "STATIC", "VirtualDesktopSwitcher", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
    if (hwnd == nullptr) {
        std::cerr << "Failed to create message window!\n";
        return -1;
    }
    pSwitcher->SetWindowHandle(hwnd);

    // 显示当前虚拟桌面信息
    const int desktopCount   = pSwitcher->GetDesktopCount();
    const int currentDesktop = pSwitcher->GetCurrentDesktopIndex();
    std::cout << "Total desktops: " << desktopCount << ", Current desktop: " << (currentDesktop + 1) << "\n";

    // 安装键盘钩子
    if (!pSwitcher->InstallHook()) {
        std::cerr << "Failed to install keyboard hook!\n";
        DestroyWindow(hwnd);
        return -1;
    }

    std::cout << "Virtual Desktop Switcher running... Press ESC to exit.\n";

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        // 检查ESC键退出
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            break;
        }

        // 处理自定义消息：切换桌面
        if (msg.message == WM_SWITCH_DESKTOP) {
            pSwitcher->SwitchToDesktop(static_cast<int>(msg.wParam));
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理资源
    DestroyWindow(hwnd);

    return 0;
}
