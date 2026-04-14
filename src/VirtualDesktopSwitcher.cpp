#include <comdef.h>
#include <iostream>
#include <windows.h>
#include <wrl/client.h>

// 导入COM组件
#include <ObjectArray.h>
#include <initguid.h>
#include <servprov.h>
#include <shobjidl.h>

using namespace Microsoft::WRL;

// 自定义消息用于延迟执行桌面切换
#define WM_SWITCH_DESKTOP (WM_USER + 1)

// 全局变量用于存储要切换的桌面索引
static int g_desktopIndexToSwitch = -1;

// IVirtualDesktop接口定义 - 使用VD.ahk中版本26100的GUID
MIDL_INTERFACE("3F07F4BE-B107-441A-AF0F-39D82529072C")
IVirtualDesktop : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE IsViewVisible(IUnknown * pView, BOOL * pfVisible) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetID(GUID * pGuid)                               = 0;
    virtual HRESULT STDMETHODCALLTYPE GetName(LPWSTR * ppwszName)                       = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRemote(BOOL * pfIsRemote)                       = 0;
};

// IVirtualDesktopManagerInternal接口定义 - 使用VD.ahk中版本26100的GUID
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

// 定义CLSID和IID - 根据VD.ahk的实现
static const CLSID CLSID_ImmersiveShell                       = {0xC2F03A33, 0x21F5, 0x47FA, {0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39}};
static const IID   IID_IServiceProvider                       = {0x6D5140C1, 0x7436, 0x11CE, {0x80, 0x34, 0x00, 0xAA, 0x60, 0x09, 0xFA}};
static const IID   IID_IVirtualDesktopManagerInternal_Service = {0xC5E0CDCA, 0x7B6E, 0x41B2, {0x9F, 0xC4, 0xD9, 0x39, 0x75, 0xCC, 0x46, 0x7B}};
static const IID   IID_IVirtualDesktopManagerInternal         = {0x53F5CA0B, 0x158F, 0x4124, {0x90, 0x0C, 0x05, 0x71, 0x58, 0x06, 0x0B, 0x27}};
static const IID   IID_IVirtualDesktop                        = {0x3F07F4BE, 0xB107, 0x441A, {0xAF, 0x0F, 0x39, 0xD8, 0x25, 0x29, 0x07, 0x2C}};

class VirtualDesktopHelper {
private:
    ComPtr<IVirtualDesktopManagerInternal> virtualDesktopManagerInternal;

public:
    VirtualDesktopHelper() {
        CoInitialize(nullptr);

        // 使用VD.ahk中的方式获取IServiceProvider
        ComPtr<IUnknown> immersiveShell;
        HRESULT          hr = CoCreateInstance(CLSID_ImmersiveShell, nullptr, CLSCTX_ALL,
                                               IID_IUnknown, reinterpret_cast<void **>(immersiveShell.ReleaseAndGetAddressOf()));

        if (FAILED(hr)) {
            std::cerr << "Failed to create IImmersiveShell: 0x" << std::hex << hr << std::endl;
            return;
        }

        std::cout << "IImmersiveShell created successfully" << std::endl;

        // 查询IServiceProvider接口
        ComPtr<IServiceProvider> serviceProvider;
        hr = immersiveShell.As(&serviceProvider);

        if (FAILED(hr)) {
            std::cerr << "Failed to query IServiceProvider: 0x" << std::hex << hr << std::endl;
            return;
        }

        std::cout << "IServiceProvider queried successfully" << std::endl;

        // 使用VD.ahk中的服务ID
        hr = serviceProvider->QueryService(IID_IVirtualDesktopManagerInternal_Service,
                                           IID_IVirtualDesktopManagerInternal,
                                           reinterpret_cast<void **>(virtualDesktopManagerInternal.ReleaseAndGetAddressOf()));

        if (FAILED(hr)) {
            std::cerr << "Failed to query IVirtualDesktopManagerInternal service: 0x" << std::hex << hr << std::endl;
            return;
        }

        if (!virtualDesktopManagerInternal) {
            std::cerr << "IVirtualDesktopManagerInternal is null" << std::endl;
            return;
        }

        std::cout << "IVirtualDesktopManagerInternal created successfully" << std::endl;
    }

    ~VirtualDesktopHelper() {
        CoUninitialize();
    }

    int GetDesktopCount() {
        if (!virtualDesktopManagerInternal.Get()) return 0;

        ComPtr<IObjectArray> desktops;
        HRESULT              hr = virtualDesktopManagerInternal->GetDesktops(&desktops);
        if (FAILED(hr)) return 0;

        UINT count = 0;
        hr         = desktops->GetCount(&count);
        if (FAILED(hr)) return 0;

        return static_cast<int>(count);
    }

    bool SwitchToDesktop(int index) {
        if (!virtualDesktopManagerInternal.Get()) return false;

        ComPtr<IObjectArray> desktops;
        HRESULT              hr = virtualDesktopManagerInternal->GetDesktops(&desktops);
        if (FAILED(hr)) return false;

        UINT count = 0;
        hr         = desktops->GetCount(&count);
        if (FAILED(hr)) return false;

        if (index >= static_cast<int>(count) || index < 0) return false;

        ComPtr<IVirtualDesktop> targetDesktop;
        hr = desktops->GetAt(index, __uuidof(IVirtualDesktop), reinterpret_cast<void **>(targetDesktop.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) return false;

        hr = virtualDesktopManagerInternal->SwitchDesktop(targetDesktop.Get());
        return SUCCEEDED(hr);
    }

    int GetCurrentDesktopIndex() {
        if (!virtualDesktopManagerInternal.Get()) return -1;

        ComPtr<IVirtualDesktop> currentDesktop;
        HRESULT                 hr = virtualDesktopManagerInternal->GetCurrentDesktop(&currentDesktop);
        if (FAILED(hr)) return -1;

        ComPtr<IObjectArray> desktops;
        hr = virtualDesktopManagerInternal->GetDesktops(&desktops);
        if (FAILED(hr)) return -1;

        UINT count = 0;
        hr         = desktops->GetCount(&count);
        if (FAILED(hr)) return -1;

        for (UINT i = 0; i < count; ++i) {
            ComPtr<IVirtualDesktop> desktop;
            hr = desktops->GetAt(i, __uuidof(IVirtualDesktop), reinterpret_cast<void **>(desktop.ReleaseAndGetAddressOf()));
            if (SUCCEEDED(hr)) {
                GUID currentId, desktopId;
                currentDesktop->GetID(&currentId);
                desktop->GetID(&desktopId);

                if (IsEqualGUID(currentId, desktopId)) {
                    return static_cast<int>(i);
                }
            }
        }

        return -1;
    }
};

// 全局变量
HHOOK                 g_hHook        = nullptr;
VirtualDesktopHelper *g_pVDeskHelper = nullptr;

// 键盘钩子回调函数
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *pKeyboard = (KBDLLHOOKSTRUCT *)lParam;

        // 检查是否按下了 Alt + 数字键
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            bool isAltPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

            if (isAltPressed && pKeyboard->vkCode >= '1' && pKeyboard->vkCode <= '9') {
                int desktopIndex = pKeyboard->vkCode - '1'; // 将字符转换为索引(0-8)

                // 使用消息队列延迟执行桌面切换
                g_desktopIndexToSwitch = desktopIndex;
                PostThreadMessage(GetCurrentThreadId(), WM_SWITCH_DESKTOP, 0, 0);

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

    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

// 安装键盘钩子
bool InstallHook() {
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    return g_hHook != nullptr;
}

// 卸载键盘钩子
void UninstallHook() {
    if (g_hHook) {
        UnhookWindowsHookEx(g_hHook);
        g_hHook = nullptr;
    }
}

int main() {
    std::cout << "Windows 11 Virtual Desktop Switcher\n";
    std::cout << "Press Alt + [1-9] to switch to corresponding virtual desktop.\n";
    std::cout << "Press ESC to exit.\n";

    // 初始化虚拟桌面帮助器
    g_pVDeskHelper = new VirtualDesktopHelper();

    // 显示当前虚拟桌面信息
    int desktopCount   = g_pVDeskHelper->GetDesktopCount();
    int currentDesktop = g_pVDeskHelper->GetCurrentDesktopIndex();
    std::cout << "Total desktops: " << desktopCount << ", Current desktop: " << (currentDesktop + 1) << std::endl;

    // 安装键盘钩子
    if (!InstallHook()) {
        std::cerr << "Failed to install keyboard hook!" << std::endl;
        delete g_pVDeskHelper;
        return -1;
    }

    std::cout << "Virtual Desktop Switcher running... Press ESC to exit." << std::endl;

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        // 检查ESC键退出
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            break;
        }

        // 处理自定义消息：切换桌面
        if (msg.message == WM_SWITCH_DESKTOP) {
            if (g_desktopIndexToSwitch >= 0 && g_pVDeskHelper) {
                g_pVDeskHelper->SwitchToDesktop(g_desktopIndexToSwitch);
                g_desktopIndexToSwitch = -1;
            }
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理资源
    UninstallHook();
    delete g_pVDeskHelper;

    return 0;
}
