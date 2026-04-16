#ifndef APPLICATION_H
#define APPLICATION_H

#include <memory>

#include "TrayIcon.h"
#include "VirtualDesktopSwitcher.h"

class Application {
private:
    HWND                      m_hwnd      = nullptr;
    VirtualDesktopSwitcher   *m_pSwitcher = nullptr;
    std::unique_ptr<TrayIcon> m_pTrayIcon;

    // 窗口过程函数
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
    Application(const Application &)            = delete;
    Application &operator=(const Application &) = delete;
    Application(Application &&)                 = delete;
    Application &operator=(Application &&)      = delete;

    Application() = default;
    ~Application();

    // 初始化应用程序
    bool Initialize();

    // 运行消息循环
    int Run();
};

#endif // APPLICATION_H
