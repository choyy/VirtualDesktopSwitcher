#ifndef VIRTUAL_DESKTOP_HELPER_H
#define VIRTUAL_DESKTOP_HELPER_H

#include <ObjectArray.h>
#include <wrl/client.h>
#include <vector>

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

// IVirtualDesktopManager接口定义（标准Windows API）
MIDL_INTERFACE("a5cd92ff-29be-454c-9d8f-460b89c60332")
IVirtualDesktopManager : public IUnknown { // NOLINT(cppcoreguidelines-virtual-class-destructor)
public:
    virtual HRESULT STDMETHODCALLTYPE IsWindowOnCurrentVirtualDesktop(HWND topLevelHwnd, BOOL * pfOnCurrentDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWindowDesktopId(HWND topLevelHwnd, GUID * pDesktopId)                      = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveWindowToDesktop(HWND topLevelHwnd, REFGUID desktopId)                     = 0;
};

// IApplicationViewCollection接口定义
MIDL_INTERFACE("1841C6D7-4F9D-42C0-AF41-8747538F10E5")
IApplicationViewCollection : public IUnknown { // NOLINT(cppcoreguidelines-virtual-class-destructor)
public:
    virtual HRESULT STDMETHODCALLTYPE GetViews(IObjectArray * *ppViews)                             = 0;
    virtual HRESULT STDMETHODCALLTYPE GetViewsByZOrder(IObjectArray * *ppViews)                     = 0;
    virtual HRESULT STDMETHODCALLTYPE GetViewsByAppUserModelId(LPCWSTR id, IObjectArray * *ppViews) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetViewForHwnd(HWND hwnd, IUnknown * *ppView)                 = 0;
};

class VirtualDesktopHelper {
private:
    Microsoft::WRL::ComPtr<IVirtualDesktopManagerInternal> virtualDesktopManagerInternal;
    Microsoft::WRL::ComPtr<IVirtualDesktopManager>         virtualDesktopManager;
    Microsoft::WRL::ComPtr<IApplicationViewCollection>     viewCollection;

    [[nodiscard]] bool CheckViaViewCollection(HWND hwnd) const;
    [[nodiscard]] bool CheckViaDesktopManager(HWND hwnd) const;

public:
    VirtualDesktopHelper(const VirtualDesktopHelper &)            = delete;
    VirtualDesktopHelper &operator=(const VirtualDesktopHelper &) = delete;
    VirtualDesktopHelper(VirtualDesktopHelper &&)                 = delete;
    VirtualDesktopHelper &operator=(VirtualDesktopHelper &&)      = delete;

    VirtualDesktopHelper();
    ~VirtualDesktopHelper() { CoUninitialize(); }

    [[nodiscard]] int  GetDesktopCount() const;
    [[nodiscard]] int  GetCurrentDesktopIndex() const;
    [[nodiscard]] bool IsWindowOnCurrentDesktop(HWND hwnd) const;
    [[nodiscard]] std::vector<bool> GetDesktopEmptyMask() const;
    void               SwitchToDesktop(int index) const;
};

#endif // VIRTUAL_DESKTOP_HELPER_H
