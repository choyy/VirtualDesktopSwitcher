#include "Application.h"
#include "ColorState.h"

#include <iostream>
#include <sstream>

Application::~Application() {
    if (m_hwnd != nullptr) {
        DestroyWindow(m_hwnd);
    }
}

LRESULT CALLBACK Application::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto *pApp = reinterpret_cast<Application *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (pApp == nullptr) return DefWindowProc(hwnd, uMsg, wParam, lParam);

    if (uMsg == WM_MEASUREITEM || uMsg == WM_DRAWITEM) {
        pApp->m_pTrayIcon->HandleMessage(hwnd, uMsg, wParam, lParam);
        return (uMsg == WM_MEASUREITEM) ? TRUE : 0;
    }

    if (uMsg == WM_TRAYICON || uMsg == WM_COMMAND) {
        pApp->m_pTrayIcon->HandleMessage(hwnd, uMsg, wParam, lParam);
        return 0;
    }

    if (uMsg == WM_MENUSELECT) {
        UINT item = LOWORD(wParam);
        UINT flags = HIWORD(wParam);
        if (flags == 0xFFFF) {
            if (pApp->m_pOverlay) pApp->m_pOverlay->CancelPreview();
        } else if (!(flags & MF_POPUP)) {
            int colorIdx = (int)item - CMD_COLOR_OPTIONS_BASE;
            const auto& colors = GetPredefinedColors();
            if (colorIdx >= 0 && colorIdx < (int)colors.size()) {
                if (pApp->m_pOverlay) pApp->m_pOverlay->SetColorPreview(colors[colorIdx].hexColor);
            }
        }
        return 0;
    }

    if (uMsg == WM_SWITCH_DESKTOP) {
        pApp->m_pSwitcher->SwitchToDesktop(static_cast<int>(wParam));
        const int           desktopCount = pApp->m_pSwitcher->GetDesktopCount();
        const int           newCurrentDesktop = pApp->m_pSwitcher->GetCurrentDesktopIndex();

        std::wostringstream oss;
        oss << L"Virtual Desktop Switcher\n"
            << L"Desktops: " << desktopCount << L" | Current: " << (newCurrentDesktop + 1);
        pApp->m_pTrayIcon->UpdateTooltip(oss.str());

        if (pApp->m_pOverlay) {
            std::wstring overlayText(desktopCount, L'0');
            if (newCurrentDesktop >= 0 && newCurrentDesktop < desktopCount)
                overlayText[newCurrentDesktop] = L'*';
            pApp->m_pOverlay->SetText(overlayText);
        }
        return 0;
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

    m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                             L"VirtualDesktopSwitcherClass", L"VirtualDesktopSwitcher",
                             WS_POPUP,
                             0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (m_hwnd == nullptr) {
        std::cerr << "Failed to create window!\n";
        return false;
    }
    m_pSwitcher->SetWindowHandle(m_hwnd);

    // 设置窗口用户数据，用于在窗口过程中访问Application实例
    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

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

    // 初始化桌面覆盖层
    m_pOverlay = std::make_unique<DesktopIndicator>();
    if (!m_pOverlay->Initialize(GetModuleHandle(nullptr))) {
        std::cerr << "Failed to initialize desktop overlay!\n";
    } else {
        std::wstring overlayText(desktopCount, L'0');
        if (currentDesktop >= 0 && currentDesktop < desktopCount) {
            overlayText[currentDesktop] = L'*';
        }
        m_pOverlay->SetText(overlayText);
        m_pOverlay->Show();
    }

    // 设置颜色和编辑模式回调
    m_pTrayIcon->SetColorCallback([this](const std::wstring& hex) {
        if (m_pOverlay) m_pOverlay->SetColor(hex);
    });
    m_pTrayIcon->SetEditModeCallback([this]() {
        if (m_pOverlay) m_pOverlay->ToggleEditMode();
    });

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

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
