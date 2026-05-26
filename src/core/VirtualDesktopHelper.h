#pragma once

#include <ObjectArray.h>
#include <wrl/client.h>

#include <array>

#include "util/Utils.h"

// IVirtualDesktop接口定义
MIDL_INTERFACE("3F07F4BE-B107-441A-AF0F-39D82529072C")
IVirtualDesktop : public IUnknown { // NOLINT(cppcoreguidelines-virtual-class-destructor)
public:
    virtual HRESULT STDMETHODCALLTYPE IsViewVisible(IUnknown * pView, BOOL * pfVisible) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetID(GUID * pGuid)                               = 0;
    virtual HRESULT STDMETHODCALLTYPE GetName(LPWSTR * ppwszName)                       = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRemote(BOOL * pfIsRemote)                       = 0;
};

// IVirtualDesktopManagerInternal接口定义
MIDL_INTERFACE("53F5CA0B-158F-4124-900C-057158060B27")
IVirtualDesktopManagerInternal : public IUnknown { // NOLINT(cppcoreguidelines-virtual-class-destructor)
public:
    virtual HRESULT STDMETHODCALLTYPE GetCount(UINT * pCount)                                                                             = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveViewToDesktop(IUnknown * pView, IUnknown * pDesktop)                                            = 0;
    virtual HRESULT STDMETHODCALLTYPE CanViewMoveDesktops(IUnknown * pView, BOOL * pfCanViewMove)                                         = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentDesktop(IVirtualDesktop * *ppDesktop)                                                     = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDesktops(IObjectArray * *ppDesktops)                                                             = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAdjacentDesktop(IVirtualDesktop * pDesktopReference, int direction, IVirtualDesktop **ppDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE SwitchDesktop(IVirtualDesktop * pDesktop)                                                           = 0;
};

// IVirtualDesktopManager接口定义（标准Windows API）
MIDL_INTERFACE("a5cd92ff-29be-454c-9d8f-460b89c60332")
IVirtualDesktopManager : public IUnknown { // NOLINT(cppcoreguidelines-virtual-class-destructor)
public:
    virtual HRESULT STDMETHODCALLTYPE IsWindowOnCurrentVirtualDesktop(HWND topLevelHwnd, BOOL * pfOnCurrentDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWindowDesktopId(HWND topLevelHwnd, GUID * pDesktopId)                      = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveWindowToDesktop(HWND topLevelHwnd, REFGUID desktopId)                     = 0;
};

// IApplicationView接口定义
MIDL_INTERFACE("372E1D3B-38D3-42E4-A15B-8AB2B178F513")
IApplicationView : public IUnknown { // NOLINT(cppcoreguidelines-virtual-class-destructor)
public:
    // IInspectable methods (3)
    virtual HRESULT STDMETHODCALLTYPE GetIids(ULONG *, IID **)     = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRuntimeClassName(void **) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTrustLevel(int *)         = 0;

    // IApplicationView methods before GetAppUserModelId (11)
    virtual HRESULT STDMETHODCALLTYPE SetFocus()                       = 0;
    virtual HRESULT STDMETHODCALLTYPE SwitchTo()                       = 0;
    virtual HRESULT STDMETHODCALLTYPE TryInvokeBack(void *)            = 0;
    virtual HRESULT STDMETHODCALLTYPE GetThumbnailWindow(HWND *)       = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMonitor(void **)              = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVisibility(int *)             = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCloak(int, int)               = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPosition(REFIID, void **)     = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPosition(void *)              = 0;
    virtual HRESULT STDMETHODCALLTYPE InsertAfterWindow(HWND)          = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExtendedFramePosition(RECT *) = 0;

    // slot 17 — the one we actually call
    virtual HRESULT STDMETHODCALLTYPE GetAppUserModelId(PWSTR *) = 0;
};

// IVirtualDesktopPinnedApps接口定义
MIDL_INTERFACE("4CE81583-1E4C-4632-A621-07A53543148F")
IVirtualDesktopPinnedApps : public IUnknown { // NOLINT(cppcoreguidelines-virtual-class-destructor)
public:
    // AppId-based methods
    virtual HRESULT STDMETHODCALLTYPE IsAppIdPinned(PCWSTR appId, BOOL * pinned) = 0;
    virtual HRESULT STDMETHODCALLTYPE PinAppID(PCWSTR appId)                     = 0;
    virtual HRESULT STDMETHODCALLTYPE UnpinAppID(PCWSTR appId)                   = 0;

    // View-based methods
    virtual HRESULT STDMETHODCALLTYPE IsViewPinned(IApplicationView * view, BOOL * pinned) = 0;
    virtual HRESULT STDMETHODCALLTYPE PinView(IApplicationView * view)                     = 0;
    virtual HRESULT STDMETHODCALLTYPE UnpinView(IApplicationView * view)                   = 0;
};

// IApplicationViewCollection接口定义
MIDL_INTERFACE("1841C6D7-4F9D-42C0-AF41-8747538F10E5")
IApplicationViewCollection : public IUnknown { // NOLINT(cppcoreguidelines-virtual-class-destructor)
public:
    virtual HRESULT STDMETHODCALLTYPE GetViews(IObjectArray * *ppViews)                             = 0;
    virtual HRESULT STDMETHODCALLTYPE GetViewsByZOrder(IObjectArray * *ppViews)                     = 0;
    virtual HRESULT STDMETHODCALLTYPE GetViewsByAppUserModelId(LPCWSTR id, IObjectArray * *ppViews) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetViewForHwnd(HWND hwnd, IApplicationView * *ppView)         = 0;
};

class VirtualDesktopHelper {
public:
    VirtualDesktopHelper(const VirtualDesktopHelper &)            = delete;
    VirtualDesktopHelper &operator=(const VirtualDesktopHelper &) = delete;
    VirtualDesktopHelper(VirtualDesktopHelper &&)                 = delete;
    VirtualDesktopHelper &operator=(VirtualDesktopHelper &&)      = delete;

    VirtualDesktopHelper();
    ~VirtualDesktopHelper();

    void                                         Refresh();
    [[nodiscard]] int                            GetDesktopCount() const;
    [[nodiscard]] int                            GetCurrentDesktopIndex() const;
    [[nodiscard]] bool                           IsWindowOnCurrentDesktop(HWND hwnd) const;
    [[nodiscard]] std::array<bool, kMaxDesktops> GetDesktopEmptyMask() const;
    void                                         SwitchToDesktop(int index) const;
    void                                         MoveWindowToDesktop(HWND hwnd, int targetIndex) const;
    void                                         SetPinByApp(bool on) { m_pinByApp = on; }
    void                                         PinWindow(HWND hwnd) const;
    void                                         UnpinWindow(HWND hwnd) const;
    [[nodiscard]] bool                           IsWindowPinned(HWND hwnd) const;

private:
    bool InitCOMServices();

    [[nodiscard]] bool CheckViaViewCollection(HWND hwnd) const;
    [[nodiscard]] bool CheckViaDesktopManager(HWND hwnd) const;
    [[nodiscard]] bool GetDesktopArray(Microsoft::WRL::ComPtr<IObjectArray> &desktops, UINT &count) const;

    static bool InitImmersiveShell(Microsoft::WRL::ComPtr<IUnknown> &shell);
    static bool InitServiceProvider(Microsoft::WRL::ComPtr<IUnknown> &shell, Microsoft::WRL::ComPtr<IServiceProvider> &sp);
    bool        InitDesktopManagerInternal(Microsoft::WRL::ComPtr<IServiceProvider> &sp, Microsoft::WRL::ComPtr<IUnknown> &shell);
    void        VerifyDesktopIID();
    void        InitViewCollection(Microsoft::WRL::ComPtr<IServiceProvider> &sp);
    void        InitDesktopManager();
    void        InitPinnedApps(Microsoft::WRL::ComPtr<IServiceProvider> &sp);

    // --- Member Variables ---
    HRESULT                                                m_comInitResult = E_FAIL;
    Microsoft::WRL::ComPtr<IVirtualDesktopManagerInternal> m_virtualDesktopManagerInternal;
    Microsoft::WRL::ComPtr<IVirtualDesktopManager>         m_virtualDesktopManager;
    Microsoft::WRL::ComPtr<IApplicationViewCollection>     m_viewCollection;
    Microsoft::WRL::ComPtr<IVirtualDesktopPinnedApps>      m_pinnedApps;
    bool                                                   m_pinByApp          = false;
    IID                                                    m_iidVirtualDesktop = __uuidof(IVirtualDesktop);
};
