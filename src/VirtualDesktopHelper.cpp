#include "VirtualDesktopHelper.h"

#include <iostream>
// COM组件
#include <ObjectArray.h>
#include <comdef.h>
#include <initguid.h>
#include <servprov.h>

// CLSID和IID定义 (在匿名命名空间中,仅当前文件可见)
namespace {
const CLSID CLSID_ImmersiveShell                       = {0xC2F03A33, 0x21F5, 0x47FA, {0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39}};
const CLSID CLSID_VirtualDesktopManager                = {0xaa509086, 0x5ca9, 0x4c25, {0x8f, 0x95, 0x58, 0x9d, 0x3c, 0x07, 0xb4, 0x8a}};
const IID   IID_IApplicationViewCollection_1809        = {0x2C08ADF0, 0xA386, 0x4B35, {0x92, 0x50, 0x0F, 0xE1, 0x83, 0x47, 0x6F, 0xCC}};
const IID   IID_IApplicationViewCollection_1903        = {0x1841C6D7, 0x4F9D, 0x42C0, {0xAF, 0x41, 0x87, 0x47, 0x53, 0x8F, 0x10, 0xE5}};
const IID   IID_IServiceProvider                       = {0x6D5140C1, 0x7436, 0x11CE, {0x80, 0x34, 0x00, 0xAA, 0x60, 0x09, 0xFA}};
const IID   IID_IVirtualDesktop                        = {0x3F07F4BE, 0xB107, 0x441A, {0xAF, 0x0F, 0x39, 0xD8, 0x25, 0x29, 0x07, 0x2C}};
const IID   IID_IVirtualDesktopManager                 = {0xa5cd92ff, 0x29be, 0x454c, {0x9d, 0x8f, 0x46, 0x0b, 0x89, 0xc6, 0x03, 0x32}};
const IID   IID_IVirtualDesktopManagerInternal         = {0x53F5CA0B, 0x158F, 0x4124, {0x90, 0x0C, 0x05, 0x71, 0x58, 0x06, 0x0B, 0x27}};
const IID   IID_IVirtualDesktopManagerInternal_Service = {0xC5E0CDCA, 0x7B6E, 0x41B2, {0x9F, 0xC4, 0xD9, 0x39, 0x75, 0xCC, 0x46, 0x7B}};
} // namespace

VirtualDesktopHelper::VirtualDesktopHelper() {
    CoInitialize(nullptr);

    // 获取IServiceProvider
    Microsoft::WRL::ComPtr<IUnknown> immersiveShell;
    HRESULT                          hr = CoCreateInstance(CLSID_ImmersiveShell, nullptr, CLSCTX_ALL, IID_IUnknown,
                                                           reinterpret_cast<void **>(immersiveShell.ReleaseAndGetAddressOf())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    if (FAILED(hr)) {
        std::cerr << "Failed to create IImmersiveShell: 0x" << std::hex << hr << "\n";
        return;
    }

    std::cout << "IImmersiveShell created successfully\n";

    // 查询IServiceProvider接口
    Microsoft::WRL::ComPtr<IServiceProvider> serviceProvider;
    hr = immersiveShell.As(&serviceProvider);

    if (FAILED(hr)) {
        std::cerr << "Failed to query IServiceProvider: 0x" << std::hex << hr << "\n";
        return;
    }

    std::cout << "ServiceProvider queried successfully\n";

    hr = serviceProvider->QueryService(IID_IVirtualDesktopManagerInternal_Service,
                                       IID_IVirtualDesktopManagerInternal,
                                       reinterpret_cast<void **>(virtualDesktopManagerInternal.ReleaseAndGetAddressOf())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    if (FAILED(hr)) {
        std::cerr << "Failed to query IVirtualDesktopManagerInternal service: 0x" << std::hex << hr << "\n";
        return;
    }

    if (virtualDesktopManagerInternal == nullptr) {
        std::cerr << "IVirtualDesktopManagerInternal is null\n";
        return;
    }

    std::cout << "IVirtualDesktopManagerInternal created successfully\n";

    hr = serviceProvider->QueryService(IID_IApplicationViewCollection_1903,
                                       IID_IApplicationViewCollection_1903,
                                       reinterpret_cast<void **>(viewCollection.ReleaseAndGetAddressOf())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (FAILED(hr)) {
        hr = serviceProvider->QueryService(IID_IApplicationViewCollection_1809,
                                           IID_IApplicationViewCollection_1809,
                                           reinterpret_cast<void **>(viewCollection.ReleaseAndGetAddressOf())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
    if (SUCCEEDED(hr)) {
        std::cout << "IApplicationViewCollection created successfully\n";
    } else {
        std::cerr << "Failed to query IApplicationViewCollection: 0x" << std::hex << hr << "\n";
    }

    hr = CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_ALL,
                          IID_IVirtualDesktopManager,
                          reinterpret_cast<void **>(virtualDesktopManager.ReleaseAndGetAddressOf())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    if (FAILED(hr)) {
        std::cerr << "Failed to create IVirtualDesktopManager: 0x" << std::hex << hr << "\n";
        return;
    }

    std::cout << "IVirtualDesktopManager created successfully\n";
}

int VirtualDesktopHelper::GetDesktopCount() const {
    if (virtualDesktopManagerInternal.Get() == nullptr) { return 0; }

    Microsoft::WRL::ComPtr<IObjectArray> desktops;
    HRESULT                              hr = virtualDesktopManagerInternal->GetDesktops(&desktops);
    if (FAILED(hr)) { return 0; }

    UINT count = 0;
    hr         = desktops->GetCount(&count);
    if (FAILED(hr)) { return 0; }

    return static_cast<int>(count);
}

int VirtualDesktopHelper::GetCurrentDesktopIndex() const {
    if (virtualDesktopManagerInternal.Get() == nullptr) { return -1; }

    Microsoft::WRL::ComPtr<IVirtualDesktop> currentDesktop;
    HRESULT                                 hr = virtualDesktopManagerInternal->GetCurrentDesktop(&currentDesktop);
    if (FAILED(hr)) { return -1; }

    Microsoft::WRL::ComPtr<IObjectArray> desktops;
    hr = virtualDesktopManagerInternal->GetDesktops(&desktops);
    if (FAILED(hr)) { return -1; }

    UINT count = 0;
    hr         = desktops->GetCount(&count);
    if (FAILED(hr)) { return -1; }

    for (UINT i = 0; i < count; ++i) {
        Microsoft::WRL::ComPtr<IVirtualDesktop> desktop;
        hr = desktops->GetAt(i, __uuidof(IVirtualDesktop),
                             reinterpret_cast<void **>(desktop.ReleaseAndGetAddressOf())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
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

void VirtualDesktopHelper::SwitchToDesktop(int index) const {
    Microsoft::WRL::ComPtr<IObjectArray>    desktops;
    Microsoft::WRL::ComPtr<IVirtualDesktop> targetDesktop;
    UINT                                    count = 0;

    HRESULT hr = virtualDesktopManagerInternal->GetDesktops(&desktops);
    if (FAILED(hr)) { return; }
    hr = desktops->GetCount(&count);
    if (FAILED(hr)) { return; }
    hr = desktops->GetAt(index, __uuidof(IVirtualDesktop),
                         reinterpret_cast<void **>(targetDesktop.ReleaseAndGetAddressOf())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (FAILED(hr)) { return; }
    virtualDesktopManagerInternal->SwitchDesktop(targetDesktop.Get());
}

bool VirtualDesktopHelper::IsWindowOnCurrentDesktop(HWND hwnd) const {
    if (virtualDesktopManagerInternal.Get() == nullptr || hwnd == nullptr) {
        return false;
    }

    if (CheckViaViewCollection(hwnd)) {
        return true;
    }

    return CheckViaDesktopManager(hwnd);
}

bool VirtualDesktopHelper::CheckViaViewCollection(HWND hwnd) const {
    if (viewCollection.Get() == nullptr) {
        return false;
    }

    Microsoft::WRL::ComPtr<IUnknown> view;
    HRESULT                          hr = viewCollection->GetViewForHwnd(hwnd, view.GetAddressOf());
    if (FAILED(hr) || view == nullptr) {
        return false;
    }

    Microsoft::WRL::ComPtr<IVirtualDesktop> currentDesktop;
    hr = virtualDesktopManagerInternal->GetCurrentDesktop(&currentDesktop);
    if (FAILED(hr) || currentDesktop == nullptr) {
        return false;
    }

    BOOL visible = FALSE;
    hr           = currentDesktop->IsViewVisible(view.Get(), &visible);
    return SUCCEEDED(hr) && visible != FALSE;
}

bool VirtualDesktopHelper::CheckViaDesktopManager(HWND hwnd) const {
    if (virtualDesktopManager.Get() == nullptr) {
        return false;
    }

    BOOL    onCurrentDesktop = FALSE;
    HRESULT hr               = virtualDesktopManager->IsWindowOnCurrentVirtualDesktop(hwnd, &onCurrentDesktop);
    if (SUCCEEDED(hr)) {
        return onCurrentDesktop != FALSE;
    }

    GUID windowDesktopId = {};
    hr                   = virtualDesktopManager->GetWindowDesktopId(hwnd, &windowDesktopId);
    if (FAILED(hr)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IVirtualDesktop> currentDesktop;
    hr = virtualDesktopManagerInternal->GetCurrentDesktop(&currentDesktop);
    if (FAILED(hr) || currentDesktop == nullptr) {
        return false;
    }

    GUID currentDesktopId = {};
    hr                    = currentDesktop->GetID(&currentDesktopId);
    return SUCCEEDED(hr) && IsEqualGUID(windowDesktopId, currentDesktopId) != 0;
}
