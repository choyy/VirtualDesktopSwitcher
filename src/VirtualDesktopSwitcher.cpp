#include <iostream>
#include <memory>

#include <windows.h>

// COM组件
#include <ObjectArray.h>
#include <comdef.h>
#include <initguid.h>
#include <servprov.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// 自定义消息用于延迟执行桌面切换
#define WM_SWITCH_DESKTOP (WM_USER + 1)

namespace {
// IVirtualDesktop接口定义
MIDL_INTERFACE("3F07F4BE-B107-441A-AF0F-39D82529072C")
IVirtualDesktop : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE IsViewVisible(IUnknown * pView, BOOL * pfVisible) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetID(GUID * pGuid)                               = 0;
    virtual HRESULT STDMETHODCALLTYPE GetName(LPWSTR * ppwszName)                       = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRemote(BOOL * pfIsRemote)                       = 0;
};

// IVirtualDesktopManagerInternal接口定义
MIDL_INTERFACE("53F5CA0B-158F-4124-900C-057158060B27")
IVirtualDesktopManagerInternal : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetCount(UINT * pCount)                                                                             = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveViewToDesktop(IUnknown * pView, IUnknown * pDesktop)                                            = 0;
    virtual HRESULT STDMETHODCALLTYPE CanViewMoveDesktops(IUnknown * pView, BOOL * pfCanViewMove)                                         = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentDesktop(IVirtualDesktop * *ppDesktop)                                                     = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDesktops(IObjectArray * *ppDesktops)                                                             = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAdjacentDesktop(IVirtualDesktop * pDesktopReference, int direction, IVirtualDesktop **ppDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE SwitchDesktop(IVirtualDesktop * pDesktop)                                                           = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateDesktop(IVirtualDesktop * *ppNewDesktop)                                                      = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveDesktop(IVirtualDesktop * pRemove, IVirtualDesktop * pFallbackDesktop)                        = 0;
    virtual HRESULT STDMETHODCALLTYPE FindDesktop(GUID * desktopId, IVirtualDesktop * *ppDesktop)                                         = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDesktopByID(GUID desktopId, IVirtualDesktop * *ppDesktop)                                        = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDesktopName(IVirtualDesktop * pDesktop, LPCWSTR wszName)                                         = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDesktopWallpaper(IVirtualDesktop * pDesktop, LPCWSTR wszWallpaper)                               = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveViewToZone(IUnknown * pView, GUID zoneId)                                                       = 0;
    virtual HRESULT STDMETHODCALLTYPE CanViewMoveToZone(IUnknown * pView, GUID zoneId, BOOL * pfCanMove)                                  = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWindowZone(IUnknown * pView, GUID * pZoneId)                                                     = 0;
    virtual HRESULT STDMETHODCALLTYPE GetZoneRectsFromVirtualDesktop(IVirtualDesktop * pDesktop, IUnknown * *ppRects)                     = 0;
    virtual HRESULT STDMETHODCALLTYPE GetZoneCount(IUnknown * pMonitor, UINT * pCount)                                                    = 0;
    virtual HRESULT STDMETHODCALLTYPE RegisterForVirtualDesktopChanges(IUnknown * pObserver, DWORD * pdwCookie)                           = 0;
    virtual HRESULT STDMETHODCALLTYPE UnregisterForVirtualDesktopChanges(DWORD dwCookie)                                                  = 0;
};

// CLSID和IID定义
const CLSID CLSID_ImmersiveShell                       = {0xC2F03A33, 0x21F5, 0x47FA, {0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39}};
const IID   IID_IServiceProvider                       = {0x6D5140C1, 0x7436, 0x11CE, {0x80, 0x34, 0x00, 0xAA, 0x60, 0x09, 0xFA}};
const IID   IID_IVirtualDesktopManagerInternal_Service = {0xC5E0CDCA, 0x7B6E, 0x41B2, {0x9F, 0xC4, 0xD9, 0x39, 0x75, 0xCC, 0x46, 0x7B}};
const IID   IID_IVirtualDesktopManagerInternal         = {0x53F5CA0B, 0x158F, 0x4124, {0x90, 0x0C, 0x05, 0x71, 0x58, 0x06, 0x0B, 0x27}};
const IID   IID_IVirtualDesktop                        = {0x3F07F4BE, 0xB107, 0x441A, {0xAF, 0x0F, 0x39, 0xD8, 0x25, 0x29, 0x07, 0x2C}};

class VirtualDesktopHelper {
private:
    ComPtr<IVirtualDesktopManagerInternal> virtualDesktopManagerInternal;

public:
    // 删除拷贝和移动操作，防止意外复制或移动导致资源管理混乱
    VirtualDesktopHelper(const VirtualDesktopHelper &)            = delete;
    VirtualDesktopHelper &operator=(const VirtualDesktopHelper &) = delete;
    VirtualDesktopHelper(VirtualDesktopHelper &&)                 = delete;
    VirtualDesktopHelper &operator=(VirtualDesktopHelper &&)      = delete;

    VirtualDesktopHelper() {
        CoInitialize(nullptr);

        // 获取IServiceProvider
        ComPtr<IUnknown> immersiveShell;
        HRESULT          hr = CoCreateInstance(CLSID_ImmersiveShell, nullptr, CLSCTX_ALL,
                                               IID_IUnknown, reinterpret_cast<void **>(immersiveShell.ReleaseAndGetAddressOf()));

        if (FAILED(hr)) {
            std::cerr << "Failed to create IImmersiveShell: 0x" << std::hex << hr << "\n";
            return;
        }

        std::cout << "IImmersiveShell created successfully\n";

        // 查询IServiceProvider接口
        ComPtr<IServiceProvider> serviceProvider;
        hr = immersiveShell.As(&serviceProvider);

        if (FAILED(hr)) {
            std::cerr << "Failed to query IServiceProvider: 0x" << std::hex << hr << "\n";
            return;
        }

        std::cout << "ServiceProvider queried successfully\n";

        hr = serviceProvider->QueryService(IID_IVirtualDesktopManagerInternal_Service,
                                           IID_IVirtualDesktopManagerInternal,
                                           reinterpret_cast<void **>(virtualDesktopManagerInternal.ReleaseAndGetAddressOf()));

        if (FAILED(hr)) {
            std::cerr << "Failed to query IVirtualDesktopManagerInternal service: 0x" << std::hex << hr << "\n";
            return;
        }

        if (virtualDesktopManagerInternal == nullptr) {
            std::cerr << "IVirtualDesktopManagerInternal is null\n";
            return;
        }

        std::cout << "IVirtualDesktopManagerInternal created successfully\n";
    }

    ~VirtualDesktopHelper() {
        CoUninitialize();
    }

    [[nodiscard]] int GetDesktopCount() const {
        if (virtualDesktopManagerInternal.Get() == nullptr) { return 0; }

        ComPtr<IObjectArray> desktops;
        HRESULT              hr = virtualDesktopManagerInternal->GetDesktops(&desktops);
        if (FAILED(hr)) { return 0; }

        UINT count = 0;
        hr         = desktops->GetCount(&count);
        if (FAILED(hr)) { return 0; }

        return static_cast<int>(count);
    }

    [[nodiscard]] int GetCurrentDesktopIndex() const {
        if (virtualDesktopManagerInternal.Get() == nullptr) { return -1; }

        ComPtr<IVirtualDesktop> currentDesktop;
        HRESULT                 hr = virtualDesktopManagerInternal->GetCurrentDesktop(&currentDesktop);
        if (FAILED(hr)) { return -1; }

        ComPtr<IObjectArray> desktops;
        hr = virtualDesktopManagerInternal->GetDesktops(&desktops);
        if (FAILED(hr)) { return -1; }

        UINT count = 0;
        hr         = desktops->GetCount(&count);
        if (FAILED(hr)) { return -1; }

        for (UINT i = 0; i < count; ++i) {
            ComPtr<IVirtualDesktop> desktop;
            hr = desktops->GetAt(i, __uuidof(IVirtualDesktop), reinterpret_cast<void **>(desktop.ReleaseAndGetAddressOf()));
            if (SUCCEEDED(hr)) {
                GUID currentId, desktopId;
                currentDesktop->GetID(&currentId);
                desktop->GetID(&desktopId);

                if (IsEqualGUID(currentId, desktopId) != 0) {
                    return static_cast<int>(i);
                }
            }
        }

        return -1;
    }

    void SwitchToDesktop(int index) const {
        ComPtr<IObjectArray> desktops;
        HRESULT              hr    = virtualDesktopManagerInternal->GetDesktops(&desktops);
        UINT                 count = 0;
        hr                         = desktops->GetCount(&count);
        ComPtr<IVirtualDesktop> targetDesktop;
        hr = desktops->GetAt(index, __uuidof(IVirtualDesktop), reinterpret_cast<void **>(targetDesktop.ReleaseAndGetAddressOf()));
        hr = virtualDesktopManagerInternal->SwitchDesktop(targetDesktop.Get());
    }
};

class VirtualDesktopSwitcher {
private:
    int                                   m_desktopIndexToSwitch = -1;
    HHOOK                                 m_hHook                = nullptr;
    std::unique_ptr<VirtualDesktopHelper> m_pVDeskHelper;
    HWND                                  m_hwnd = nullptr;

    // 键盘钩子回调函数
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        VirtualDesktopSwitcher *pInstance = VirtualDesktopSwitcher::GetInstance();
        if (pInstance == nullptr) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        if (nCode == HC_ACTION) {
            const auto *pKeyboard = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);

            // 检查是否按下了 Alt + 数字键
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                const bool isAltPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

                if (isAltPressed && pKeyboard->vkCode >= '1' && pKeyboard->vkCode <= '9') {
                    const int desktopIndex = static_cast<int>(pKeyboard->vkCode - '1'); // 将字符转换为索引(0-8)

                    // 使用消息队列延迟执行桌面切换
                    pInstance->SetDesktopIndexToSwitch(desktopIndex);
                    PostMessage(pInstance->GetWindowHandle(), WM_SWITCH_DESKTOP, 0, 0);

                    // 阻止原始按键事件传递
                    return 1;
                }

                // ESC键退出
                if (pKeyboard->vkCode == VK_ESCAPE) {
                    PostQuitMessage(0);
                    return 1;
                }
            }
        }

        return CallNextHookEx(pInstance->GetHook(), nCode, wParam, lParam);
    }

public:
    // 删除拷贝和移动操作，防止意外复制或移动导致资源管理混乱
    VirtualDesktopSwitcher(const VirtualDesktopSwitcher &)            = delete;
    VirtualDesktopSwitcher &operator=(const VirtualDesktopSwitcher &) = delete;
    VirtualDesktopSwitcher(VirtualDesktopSwitcher &&)                 = delete;
    VirtualDesktopSwitcher &operator=(VirtualDesktopSwitcher &&)      = delete;

    VirtualDesktopSwitcher() {
        m_pVDeskHelper = std::make_unique<VirtualDesktopHelper>();
    }

    ~VirtualDesktopSwitcher() {
        UninstallHook();
    }

    // 获取单例实例
    static VirtualDesktopSwitcher *GetInstance() {
        static VirtualDesktopSwitcher instance;
        return &instance;
    }

    // 安装键盘钩子
    bool InstallHook() {
        m_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
        return m_hHook != nullptr;
    }

    // 卸载键盘钩子
    void UninstallHook() {
        if (m_hHook != nullptr) {
            UnhookWindowsHookEx(m_hHook);
            m_hHook = nullptr;
        }
    }

    // 设置窗口句柄
    void SetWindowHandle(HWND hwnd) {
        m_hwnd = hwnd;
    }

    // 获取窗口句柄
    [[nodiscard]] HWND GetWindowHandle() const {
        return m_hwnd;
    }

    // 获取钩子句柄
    [[nodiscard]] HHOOK GetHook() const {
        return m_hHook;
    }

    // 设置要切换的桌面索引
    void SetDesktopIndexToSwitch(int index) {
        m_desktopIndexToSwitch = index;
    }

    // 切换到指定的桌面
    void SwitchToDesktop() {
        if (m_desktopIndexToSwitch >= 0 && m_pVDeskHelper != nullptr) {
            m_pVDeskHelper->SwitchToDesktop(m_desktopIndexToSwitch);
            m_desktopIndexToSwitch = -1;
        }
    }

    // 获取桌面数量
    [[nodiscard]] int GetDesktopCount() const {
        return m_pVDeskHelper ? m_pVDeskHelper->GetDesktopCount() : 0;
    }

    // 获取当前桌面索引
    [[nodiscard]] int GetCurrentDesktopIndex() const {
        return m_pVDeskHelper ? m_pVDeskHelper->GetCurrentDesktopIndex() : -1;
    }
};
} // namespace

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
            pSwitcher->SwitchToDesktop();
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理资源
    DestroyWindow(hwnd);

    return 0;
}