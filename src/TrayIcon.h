#ifndef TRAY_ICON_H
#define TRAY_ICON_H

#include <windows.h>

#include <shellapi.h>
#include <string>

// 自定义消息定义
#define WM_TRAYICON (WM_USER + 2)
#define WM_TRAY_EXIT (WM_USER + 3)
#define WM_TRAY_TOGGLE_AUTOSTART (WM_USER + 4)

class TrayIcon {
private:
    NOTIFYICONDATAW m_nid{};
    HMENU           m_hMenu            = nullptr;
    bool            m_autoStartEnabled = false;

    // 注册表相关函数
    [[nodiscard]] static bool IsAutoStartEnabled();
    static void               SetAutoStart(bool enable);

public:
    TrayIcon(const TrayIcon &)            = delete;
    TrayIcon &operator=(const TrayIcon &) = delete;
    TrayIcon(TrayIcon &&)                 = delete;
    TrayIcon &operator=(TrayIcon &&)      = delete;

    TrayIcon() = default;
    ~TrayIcon();

    // 初始化托盘图标
    bool Initialize(HWND hwnd, HINSTANCE hInstance);

    // 处理托盘消息
    void HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // 更新托盘图标提示信息
    void UpdateTooltip(const std::wstring &tooltip);

    // 获取自动启动状态
    [[nodiscard]] bool GetAutoStartStatus() const { return m_autoStartEnabled; }
};

#endif // TRAY_ICON_H
