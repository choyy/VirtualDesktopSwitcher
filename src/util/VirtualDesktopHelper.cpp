#include "VirtualDesktopHelper.h"

#include <ObjectArray.h>
#include <comdef.h>
#include <initguid.h>
#include <servprov.h>

#include <iostream>

namespace {
// CLSID and IID definitions
const CLSID CLSID_ImmersiveShell                       = {.Data1 = 0xC2F03A33, .Data2 = 0x21F5, .Data3 = 0x47FA, .Data4 = {0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39}};
const CLSID CLSID_VirtualDesktopManager                = {.Data1 = 0xaa509086, .Data2 = 0x5ca9, .Data3 = 0x4c25, .Data4 = {0x8f, 0x95, 0x58, 0x9d, 0x3c, 0x07, 0xb4, 0x8a}};
const IID   IID_IApplicationViewCollection_1809        = {.Data1 = 0x2C08ADF0, .Data2 = 0xA386, .Data3 = 0x4B35, .Data4 = {0x92, 0x50, 0x0F, 0xE1, 0x83, 0x47, 0x6F, 0xCC}};
const IID   IID_IApplicationViewCollection_1903        = {.Data1 = 0x1841C6D7, .Data2 = 0x4F9D, .Data3 = 0x42C0, .Data4 = {0xAF, 0x41, 0x87, 0x47, 0x53, 0x8F, 0x10, 0xE5}};
const IID   IID_IVirtualDesktopManager                 = {.Data1 = 0xa5cd92ff, .Data2 = 0x29be, .Data3 = 0x454c, .Data4 = {0x8d, 0x04, 0xd8, 0x28, 0x79, 0xfb, 0x3f, 0x1b}};
const IID   IID_IVirtualDesktopManagerInternal_Win11   = {.Data1 = 0x53F5CA0B, .Data2 = 0x158F, .Data3 = 0x4124, .Data4 = {0x90, 0x0C, 0x05, 0x71, 0x58, 0x06, 0x0B, 0x27}};
const IID   IID_IVirtualDesktopManagerInternal_Service = {.Data1 = 0xC5E0CDCA, .Data2 = 0x7B6E, .Data3 = 0x41B2, .Data4 = {0x9F, 0xC4, 0xD9, 0x39, 0x75, 0xCC, 0x46, 0x7B}};
const IID   IID_IVirtualDesktopManagerInternal_Win10   = {.Data1 = 0xF31574D6, .Data2 = 0xB682, .Data3 = 0x4CDC, .Data4 = {0xBD, 0x56, 0x18, 0x27, 0x86, 0x0A, 0xBE, 0xC6}};
const IID   IID_IVirtualDesktop_Win10                  = {.Data1 = 0xFF72FFDD, .Data2 = 0xBE7E, .Data3 = 0x43FC, .Data4 = {0x9C, 0x03, 0xAD, 0x81, 0x68, 0x1E, 0x88, 0xE4}};

template <typename T>
void **ComPtrAsVoid(Microsoft::WRL::ComPtr<T> &ptr) {
    return reinterpret_cast<void **>(ptr.ReleaseAndGetAddressOf()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}
} // namespace

VirtualDesktopHelper::VirtualDesktopHelper() {
    CoInitialize(nullptr);

    Microsoft::WRL::ComPtr<IUnknown> immersiveShell;
    HRESULT                          hr = CoCreateInstance(CLSID_ImmersiveShell, nullptr, CLSCTX_LOCAL_SERVER,
                                                           IID_IUnknown, ComPtrAsVoid(immersiveShell));
    if (FAILED(hr)) {
        std::cerr << "Failed to create IImmersiveShell: 0x" << std::hex << hr << "\n";
        return;
    }
    std::cout << "IImmersiveShell created successfully\n";

    Microsoft::WRL::ComPtr<IServiceProvider> serviceProvider;
    hr = immersiveShell.As(&serviceProvider);
    if (FAILED(hr)) {
        std::cerr << "Failed to query IServiceProvider: 0x" << std::hex << hr << "\n";
        return;
    }
    std::cout << "ServiceProvider queried successfully\n";

    // Try multiple (service GUID, interface IID) combinations for Win10 ~ Win11 compatibility
    struct {
        const IID &service;
        const IID &iid;
        bool       win10;
    } const vdiCombos[] = {
        {IID_IVirtualDesktopManagerInternal_Service, IID_IVirtualDesktopManagerInternal_Win10, true},
        {IID_IVirtualDesktopManagerInternal_Service, IID_IVirtualDesktopManagerInternal_Win11, false},
        {IID_IVirtualDesktopManagerInternal_Service, IID_IVirtualDesktopManagerInternal_Service, false},
        {CLSID_VirtualDesktopManager, IID_IVirtualDesktopManagerInternal_Win10, true},
        {IID_IVirtualDesktopManagerInternal_Win10, IID_IVirtualDesktopManagerInternal_Win10, true},
        {CLSID_VirtualDesktopManager, IID_IVirtualDesktopManagerInternal_Win11, false},
    };
    for (const auto &c : vdiCombos) {
        hr = serviceProvider->QueryService(c.service, c.iid, ComPtrAsVoid(virtualDesktopManagerInternal));
        if (SUCCEEDED(hr) && virtualDesktopManagerInternal != nullptr) {
            m_iidVirtualDesktop = c.win10 ? IID_IVirtualDesktop_Win10 : __uuidof(IVirtualDesktop);
            std::cout << "IVirtualDesktopManagerInternal created successfully\n";
            break;
        }
    }

    if (virtualDesktopManagerInternal == nullptr) {
        static const IID *qiIIDs[] = {&IID_IVirtualDesktopManagerInternal_Win10, &IID_IVirtualDesktopManagerInternal_Win11, &IID_IVirtualDesktopManagerInternal_Service};
        for (const auto &iid : qiIIDs) {
            hr = immersiveShell->QueryInterface(*iid, ComPtrAsVoid(virtualDesktopManagerInternal));
            if (SUCCEEDED(hr) && virtualDesktopManagerInternal != nullptr) {
                m_iidVirtualDesktop = (iid == &IID_IVirtualDesktopManagerInternal_Win10) ? IID_IVirtualDesktop_Win10 : __uuidof(IVirtualDesktop);
                std::cout << "IVirtualDesktopManagerInternal created successfully\n";
                break;
            }
        }
    }

    if (virtualDesktopManagerInternal == nullptr) {
        Microsoft::WRL::ComPtr<IUnknown> vdMgr;
        hr = CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IUnknown, ComPtrAsVoid(vdMgr));
        if (SUCCEEDED(hr)) {
            static const IID *qiIIDs[] = {&IID_IVirtualDesktopManagerInternal_Win10, &IID_IVirtualDesktopManagerInternal_Win11};
            for (const auto &iid : qiIIDs) {
                hr = vdMgr->QueryInterface(*iid, ComPtrAsVoid(virtualDesktopManagerInternal));
                if (SUCCEEDED(hr) && virtualDesktopManagerInternal != nullptr) {
                    m_iidVirtualDesktop = (iid == &IID_IVirtualDesktopManagerInternal_Win10) ? IID_IVirtualDesktop_Win10 : __uuidof(IVirtualDesktop);
                    std::cout << "IVirtualDesktopManagerInternal created successfully\n";
                    break;
                }
            }
        }
    }

    if (virtualDesktopManagerInternal == nullptr) {
        std::cerr << "Failed to query IVirtualDesktopManagerInternal\n";
        return;
    }

    // Verify the chosen IVirtualDesktop IID
    {
        Microsoft::WRL::ComPtr<IVirtualDesktop> testDesktop;
        if (SUCCEEDED(virtualDesktopManagerInternal->GetCurrentDesktop(&testDesktop))) {
            Microsoft::WRL::ComPtr<IUnknown> testQI;
            if (FAILED(testDesktop->QueryInterface(m_iidVirtualDesktop, ComPtrAsVoid(testQI)))) {
                m_iidVirtualDesktop = IsEqualGUID(m_iidVirtualDesktop, IID_IVirtualDesktop_Win10) ? __uuidof(IVirtualDesktop) : IID_IVirtualDesktop_Win10;
            }
        }
    }

    hr = serviceProvider->QueryService(IID_IApplicationViewCollection_1903,
                                       IID_IApplicationViewCollection_1903,
                                       ComPtrAsVoid(viewCollection));
    if (FAILED(hr)) {
        hr = serviceProvider->QueryService(IID_IApplicationViewCollection_1809,
                                           IID_IApplicationViewCollection_1809,
                                           ComPtrAsVoid(viewCollection));
    }
    if (SUCCEEDED(hr)) {
        std::cout << "IApplicationViewCollection created successfully\n";
    } else {
        std::cerr << "Failed to query IApplicationViewCollection: 0x" << std::hex << hr << "\n";
    }

    hr = CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IVirtualDesktopManager, ComPtrAsVoid(virtualDesktopManager));
    if (FAILED(hr)) {
        std::cerr << "Failed to create IVirtualDesktopManager: 0x" << std::hex << hr << "\n";
    } else {
        std::cout << "IVirtualDesktopManager created successfully\n";
    }
}

int VirtualDesktopHelper::GetDesktopCount() const {
    if (virtualDesktopManagerInternal == nullptr) {
        return 0;
    }

    Microsoft::WRL::ComPtr<IObjectArray> desktops;
    if (FAILED(virtualDesktopManagerInternal->GetDesktops(&desktops))) {
        return 0;
    }

    UINT count = 0;
    return SUCCEEDED(desktops->GetCount(&count)) ? static_cast<int>(count) : 0;
}

int VirtualDesktopHelper::GetCurrentDesktopIndex() const {
    if (virtualDesktopManagerInternal == nullptr) {
        return -1;
    }

    Microsoft::WRL::ComPtr<IVirtualDesktop> currentDesktop;
    if (FAILED(virtualDesktopManagerInternal->GetCurrentDesktop(&currentDesktop))) {
        return -1;
    }

    Microsoft::WRL::ComPtr<IObjectArray> desktops;
    if (FAILED(virtualDesktopManagerInternal->GetDesktops(&desktops))) {
        return -1;
    }

    UINT count = 0;
    if (FAILED(desktops->GetCount(&count))) {
        return -1;
    }

    for (UINT i = 0; i < count; ++i) {
        Microsoft::WRL::ComPtr<IVirtualDesktop> desktop;
        if (FAILED(desktops->GetAt(i, m_iidVirtualDesktop,
                                   ComPtrAsVoid(desktop)))) {
            continue;
        }

        GUID currentId{}, desktopId{};
        if (SUCCEEDED(currentDesktop->GetID(&currentId)) && SUCCEEDED(desktop->GetID(&desktopId)) && IsEqualGUID(currentId, desktopId) != 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void VirtualDesktopHelper::SwitchToDesktop(int index) const {
    if (virtualDesktopManagerInternal == nullptr) {
        return;
    }

    Microsoft::WRL::ComPtr<IObjectArray> desktops;
    if (FAILED(virtualDesktopManagerInternal->GetDesktops(&desktops))) {
        return;
    }

    UINT count = 0;
    if (FAILED(desktops->GetCount(&count)) || static_cast<size_t>(index) >= static_cast<size_t>(count)) {
        return;
    }

    Microsoft::WRL::ComPtr<IVirtualDesktop> targetDesktop;
    if (FAILED(desktops->GetAt(index, m_iidVirtualDesktop,
                               ComPtrAsVoid(targetDesktop)))) {
        return;
    }

    virtualDesktopManagerInternal->SwitchDesktop(targetDesktop.Get());
}

bool VirtualDesktopHelper::IsWindowOnCurrentDesktop(HWND hwnd) const {
    if ((virtualDesktopManagerInternal == nullptr) || hwnd == nullptr) {
        return false;
    }
    return CheckViaViewCollection(hwnd) || CheckViaDesktopManager(hwnd);
}

bool VirtualDesktopHelper::CheckViaViewCollection(HWND hwnd) const {
    if (viewCollection == nullptr) {
        return false;
    }

    Microsoft::WRL::ComPtr<IUnknown> view;
    if (FAILED(viewCollection->GetViewForHwnd(hwnd, view.GetAddressOf())) || (view == nullptr)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IVirtualDesktop> currentDesktop;
    if (FAILED(virtualDesktopManagerInternal->GetCurrentDesktop(&currentDesktop)) || (currentDesktop == nullptr)) {
        return false;
    }

    BOOL visible = FALSE;
    return SUCCEEDED(currentDesktop->IsViewVisible(view.Get(), &visible)) && (visible != FALSE);
}

bool VirtualDesktopHelper::CheckViaDesktopManager(HWND hwnd) const {
    if (virtualDesktopManager == nullptr) {
        return false;
    }

    BOOL onCurrentDesktop = FALSE;
    if (SUCCEEDED(virtualDesktopManager->IsWindowOnCurrentVirtualDesktop(hwnd, &onCurrentDesktop))) {
        return onCurrentDesktop != FALSE;
    }

    GUID windowDesktopId{};
    if (FAILED(virtualDesktopManager->GetWindowDesktopId(hwnd, &windowDesktopId))) {
        return false;
    }

    Microsoft::WRL::ComPtr<IVirtualDesktop> currentDesktop;
    if (FAILED(virtualDesktopManagerInternal->GetCurrentDesktop(&currentDesktop)) || (currentDesktop == nullptr)) {
        return false;
    }

    GUID currentDesktopId{};
    return SUCCEEDED(currentDesktop->GetID(&currentDesktopId)) && IsEqualGUID(windowDesktopId, currentDesktopId) != 0;
}

std::vector<bool> VirtualDesktopHelper::GetDesktopEmptyMask() const {
    if ((virtualDesktopManagerInternal == nullptr) || (viewCollection == nullptr)) {
        return {};
    }

    Microsoft::WRL::ComPtr<IObjectArray> desktops;
    if (FAILED(virtualDesktopManagerInternal->GetDesktops(&desktops))) {
        return {};
    }

    UINT desktopCount = 0;
    if (FAILED(desktops->GetCount(&desktopCount)) || desktopCount == 0) {
        return {};
    }

    Microsoft::WRL::ComPtr<IObjectArray> views;
    if (FAILED(viewCollection->GetViews(&views))) {
        return {};
    }

    UINT viewCount = 0;
    if (FAILED(views->GetCount(&viewCount))) {
        return {};
    }

    std::vector<int> windowCounts(desktopCount, 0);

    for (UINT v = 0; v < viewCount; ++v) {
        Microsoft::WRL::ComPtr<IUnknown> view;
        if (FAILED(views->GetAt(v, IID_IUnknown, ComPtrAsVoid(view)))) {
            continue;
        }

        int visibleDesktop = -1;
        int visibleCount   = 0;
        for (UINT d = 0; d < desktopCount; ++d) {
            Microsoft::WRL::ComPtr<IVirtualDesktop> desktop;
            if (FAILED(desktops->GetAt(d, m_iidVirtualDesktop,
                                       ComPtrAsVoid(desktop)))) {
                continue;
            }

            BOOL visible = FALSE;
            if (SUCCEEDED(desktop->IsViewVisible(view.Get(), &visible)) && (visible != FALSE)) {
                ++visibleCount;
                visibleDesktop = static_cast<int>(d);
            }
        }
        if (visibleCount == 1 && visibleDesktop >= 0) {
            ++windowCounts[visibleDesktop];
        }
    }

    std::vector<bool> emptyMask(desktopCount);
    for (UINT i = 0; i < desktopCount; ++i) {
        emptyMask[i] = (windowCounts[i] == 0);
    }
    return emptyMask;
}
