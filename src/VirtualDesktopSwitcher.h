#ifndef VIRTUAL_DESKTOP_SWITCHER_H
#define VIRTUAL_DESKTOP_SWITCHER_H

#include <memory>

#include "VirtualDesktopHelper.h"

// 自定义消息用于延迟执行桌面切换
#define WM_SWITCH_DESKTOP (WM_USER + 1)

class VirtualDesktopSwitcher {
private:
    std::unique_ptr<VirtualDesktopHelper> m_pVDeskHelper;
    HHOOK                                 m_hHook = nullptr;
    HWND                                  m_hwnd  = nullptr;

    // 键盘钩子回调函数
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

public:
    VirtualDesktopSwitcher(const VirtualDesktopSwitcher &)            = delete;
    VirtualDesktopSwitcher &operator=(const VirtualDesktopSwitcher &) = delete;
    VirtualDesktopSwitcher(VirtualDesktopSwitcher &&)                 = delete;
    VirtualDesktopSwitcher &operator=(VirtualDesktopSwitcher &&)      = delete;

    VirtualDesktopSwitcher() : m_pVDeskHelper(std::make_unique<VirtualDesktopHelper>()) {}
    ~VirtualDesktopSwitcher() { UninstallHook(); }

    // 获取单例实例
    static VirtualDesktopSwitcher *GetInstance();

    // 安装键盘钩子
    bool InstallHook();

    // 卸载键盘钩子
    void UninstallHook();

    // 切换到指定的桌面
    void SwitchToDesktop(int index);

    // 设置窗口句柄
    void SetWindowHandle(HWND hwnd) { m_hwnd = hwnd; }

    // 获取窗口句柄
    [[nodiscard]] HWND GetWindowHandle() const { return m_hwnd; }

    // 获取钩子句柄
    [[nodiscard]] HHOOK GetHook() const { return m_hHook; }

    // 获取桌面数量
    [[nodiscard]] int GetDesktopCount() const { return m_pVDeskHelper ? m_pVDeskHelper->GetDesktopCount() : 0; }

    // 获取当前桌面索引
    [[nodiscard]] int GetCurrentDesktopIndex() const { return m_pVDeskHelper ? m_pVDeskHelper->GetCurrentDesktopIndex() : -1; }
};

#endif // VIRTUAL_DESKTOP_SWITCHER_H
