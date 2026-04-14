#include "VirtualDesktopHelper.h"

#include <iostream>
// COM组件
#include <ObjectArray.h>
#include <comdef.h>
#include <initguid.h>
#include <servprov.h>

// CLSID和IID定义
const CLSID CLSID_ImmersiveShell                       = {0xC2F03A33, 0x21F5, 0x47FA, {0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39}};
const IID   IID_IServiceProvider                       = {0x6D5140C1, 0x7436, 0x11CE, {0x80, 0x34, 0x00, 0xAA, 0x60, 0x09, 0xFA}};
const IID   IID_IVirtualDesktopManagerInternal_Service = {0xC5E0CDCA, 0x7B6E, 0x41B2, {0x9F, 0xC4, 0xD9, 0x39, 0x75, 0xCC, 0x46, 0x7B}};
const IID   IID_IVirtualDesktopManagerInternal         = {0x53F5CA0B, 0x158F, 0x4124, {0x90, 0x0C, 0x05, 0x71, 0x58, 0x06, 0x0B, 0x27}};
const IID   IID_IVirtualDesktop                        = {0x3F07F4BE, 0xB107, 0x441A, {0xAF, 0x0F, 0x39, 0xD8, 0x25, 0x29, 0x07, 0x2C}};

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
    hr         = desktops->GetCount(&count);
    hr         = desktops->GetAt(index, __uuidof(IVirtualDesktop),
                                 reinterpret_cast<void **>(targetDesktop.ReleaseAndGetAddressOf())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    hr         = virtualDesktopManagerInternal->SwitchDesktop(targetDesktop.Get());
}
