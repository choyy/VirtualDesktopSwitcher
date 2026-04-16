#include "Application.h"

#include <iostream>
#include <sstream>

Application::~Application() {
    if (m_hwnd != nullptr) {
        DestroyWindow(m_hwnd);
    }
}

LRESULT CALLBACK Application::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto *pApp = reinterpret_cast<Application *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
    if (pApp != nullptr && pApp->m_pTrayIcon != nullptr) {
        pApp->m_pTrayIcon->HandleMessage(hwnd, uMsg, wParam, lParam);
    }

    if (uMsg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool Application::Initialize() {
    // 获取VirtualDesktopSwitcher单例实例
    m_pSwitcher = VirtualDesktopSwitcher::GetInstance();

    // 注册窗口类
    WNDCLASSW wc     = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.lpszClassName = L"VirtualDesktopSwitcherClass";

    if (RegisterClassW(&wc) == 0) {
        std::cerr << "Failed to register window class!\n";
        return false;
    }

    // 创建一个隐藏窗口用于接收消息
    m_hwnd = CreateWindowExW(0, L"VirtualDesktopSwitcherClass", L"VirtualDesktopSwitcher",
                             0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
    if (m_hwnd == nullptr) {
        std::cerr << "Failed to create message window!\n";
        return false;
    }
    m_pSwitcher->SetWindowHandle(m_hwnd);

    // 设置窗口用户数据，用于在窗口过程中访问Application实例
    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    // 初始化托盘图标
    m_pTrayIcon = std::make_unique<TrayIcon>();
    if (!m_pTrayIcon->Initialize(m_hwnd, GetModuleHandle(nullptr))) {
        std::cerr << "Failed to initialize tray icon!\n";
        return false;
    }

    // 显示当前虚拟桌面信息
    const int desktopCount   = m_pSwitcher->GetDesktopCount();
    const int currentDesktop = m_pSwitcher->GetCurrentDesktopIndex();

    // 更新托盘提示信息
    std::wostringstream oss;
    oss << L"Virtual Desktop Switcher\n"
        << L"Desktops: " << desktopCount << L" | Current: " << (currentDesktop + 1);
    m_pTrayIcon->UpdateTooltip(oss.str());

    // 安装键盘钩子
    if (!m_pSwitcher->InstallHook()) {
        std::cerr << "Failed to install keyboard hook!\n";
        return false;
    }

    return true;
}

int Application::Run() {
    std::cout << "Virtual Desktop Switcher running...\n";
    std::cout << "Use Alt + [1-9] to switch desktops\n";
    std::cout << "Use tray icon to exit or manage autostart\n";

    const int desktopCount = m_pSwitcher->GetDesktopCount();

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        // 处理自定义消息：切换桌面
        if (msg.message == WM_SWITCH_DESKTOP) {
            m_pSwitcher->SwitchToDesktop(static_cast<int>(msg.wParam));

            // 更新托盘提示信息
            const int           newCurrentDesktop = m_pSwitcher->GetCurrentDesktopIndex();
            std::wostringstream oss;
            oss << L"Virtual Desktop Switcher\n"
                << L"Desktops: " << desktopCount << L" | Current: " << (newCurrentDesktop + 1);
            m_pTrayIcon->UpdateTooltip(oss.str());
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
