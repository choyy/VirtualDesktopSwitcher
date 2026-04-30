#ifndef APPLICATION_H
#define APPLICATION_H

#include <windows.h>

#include <memory>

class VirtualDesktopSwitcher;
class TrayIcon;
class DesktopIndicator;

class Application {
private:
    HWND                                    m_hwnd = nullptr;
    std::unique_ptr<VirtualDesktopSwitcher> m_switcher;
    std::unique_ptr<TrayIcon>               m_pTrayIcon;
    std::unique_ptr<DesktopIndicator>       m_pOverlay;
    int                                     m_lastDesktopIndex = -1;
    UINT                                    m_uTaskbarCreated  = 0;

    // 窗口过程函数
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void                    OnSystemResume();
    void                    SyncDesktopState();

public:
    Application(const Application &)            = delete;
    Application &operator=(const Application &) = delete;
    Application(Application &&)                 = delete;
    Application &operator=(Application &&)      = delete;

    Application();
    ~Application();

    // 初始化应用程序
    bool Initialize();

    // 运行消息循环
    static int Run();
};

#endif // APPLICATION_H
