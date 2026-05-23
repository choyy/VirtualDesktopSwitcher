#include "VirtualDesktopHelper.h"

#include <ObjectArray.h>
#include <servprov.h>

#include <array>

#include "util/Log.h"

namespace {

template <typename T>
void **ComPtrAsVoid(Microsoft::WRL::ComPtr<T> &ptr) {
    return reinterpret_cast<void **>(ptr.ReleaseAndGetAddressOf()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

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
} // namespace

bool VirtualDesktopHelper::InitCOMServices() {
    Microsoft::WRL::ComPtr<IUnknown> immersiveShell;
    if (!InitImmersiveShell(immersiveShell)) { return false; }

    Microsoft::WRL::ComPtr<IServiceProvider> serviceProvider;
    if (!InitServiceProvider(immersiveShell, serviceProvider)) { return false; }

    if (!InitDesktopManagerInternal(serviceProvider, immersiveShell)) { return false; }

    VerifyDesktopIID();
    InitViewCollection(serviceProvider);
    InitDesktopManager();
    return true;
}

VirtualDesktopHelper::VirtualDesktopHelper() : m_comInitResult(CoInitialize(nullptr)) {
    if (FAILED(m_comInitResult)) {
        Log(L"[ERROR] CoInitialize failed: 0x" + std::to_wstring(static_cast<uint32_t>(m_comInitResult)));
        return;
    }
    InitCOMServices();
}

void VirtualDesktopHelper::Refresh() {
    m_virtualDesktopManagerInternal = nullptr;
    m_virtualDesktopManager         = nullptr;
    m_viewCollection                = nullptr;
    InitCOMServices();
}

VirtualDesktopHelper::~VirtualDesktopHelper() {
    if (SUCCEEDED(m_comInitResult)) {
        CoUninitialize();
    }
}

bool VirtualDesktopHelper::InitImmersiveShell(Microsoft::WRL::ComPtr<IUnknown> &shell) {
    HRESULT hr = CoCreateInstance(CLSID_ImmersiveShell, nullptr, CLSCTX_LOCAL_SERVER,
                                  IID_IUnknown, ComPtrAsVoid(shell));
    if (FAILED(hr)) {
        Log(L"[ERROR] Failed to create IImmersiveShell: 0x" + std::to_wstring(static_cast<uint32_t>(hr)));
        return false;
    }
    Log(L"[INFO] IImmersiveShell created successfully");
    return true;
}

bool VirtualDesktopHelper::InitServiceProvider(Microsoft::WRL::ComPtr<IUnknown>         &shell,
                                               Microsoft::WRL::ComPtr<IServiceProvider> &sp) {
    HRESULT hr = shell.As(&sp);
    if (FAILED(hr)) {
        Log(L"[ERROR] Failed to query IServiceProvider: 0x" + std::to_wstring(static_cast<uint32_t>(hr)));
        return false;
    }
    Log(L"[INFO] ServiceProvider queried successfully");
    return true;
}

bool VirtualDesktopHelper::InitDesktopManagerInternal(Microsoft::WRL::ComPtr<IServiceProvider> &sp,
                                                      Microsoft::WRL::ComPtr<IUnknown>         &shell) {
    HRESULT hr = 0;
    struct VdiCombo {
        const IID *service;
        const IID *iid;
        bool       win10;
    };
    const std::array<VdiCombo, 6> vdiCombos = {{
        {.service = &IID_IVirtualDesktopManagerInternal_Service, .iid = &IID_IVirtualDesktopManagerInternal_Win10, .win10 = true},
        {.service = &IID_IVirtualDesktopManagerInternal_Service, .iid = &IID_IVirtualDesktopManagerInternal_Win11, .win10 = false},
        {.service = &IID_IVirtualDesktopManagerInternal_Service, .iid = &IID_IVirtualDesktopManagerInternal_Service, .win10 = false},
        {.service = &CLSID_VirtualDesktopManager, .iid = &IID_IVirtualDesktopManagerInternal_Win10, .win10 = true},
        {.service = &IID_IVirtualDesktopManagerInternal_Win10, .iid = &IID_IVirtualDesktopManagerInternal_Win10, .win10 = true},
        {.service = &CLSID_VirtualDesktopManager, .iid = &IID_IVirtualDesktopManagerInternal_Win11, .win10 = false},
    }};
    for (const auto &c : vdiCombos) {
        hr = sp->QueryService(*c.service, *c.iid, ComPtrAsVoid(m_virtualDesktopManagerInternal));
        if (SUCCEEDED(hr) && m_virtualDesktopManagerInternal != nullptr) {
            m_iidVirtualDesktop = c.win10 ? IID_IVirtualDesktop_Win10 : __uuidof(IVirtualDesktop);
            Log(L"[INFO] IVirtualDesktopManagerInternal created (QueryService)");
            return true;
        }
    }

    {
        const std::array<const IID *, 3> qiIIDs = {&IID_IVirtualDesktopManagerInternal_Win10, &IID_IVirtualDesktopManagerInternal_Win11, &IID_IVirtualDesktopManagerInternal_Service};
        for (const auto &iid : qiIIDs) {
            hr = shell->QueryInterface(*iid, ComPtrAsVoid(m_virtualDesktopManagerInternal));
            if (SUCCEEDED(hr) && m_virtualDesktopManagerInternal != nullptr) {
                m_iidVirtualDesktop = (iid == &IID_IVirtualDesktopManagerInternal_Win10) ? IID_IVirtualDesktop_Win10 : __uuidof(IVirtualDesktop);
                Log(L"[INFO] IVirtualDesktopManagerInternal created (QI from immersiveShell)");
                return true;
            }
        }
    }

    {
        Microsoft::WRL::ComPtr<IUnknown> vdMgr;
        hr = CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IUnknown, ComPtrAsVoid(vdMgr));
        if (SUCCEEDED(hr)) {
            const std::array<const IID *, 2> qiIIDs = {&IID_IVirtualDesktopManagerInternal_Win10, &IID_IVirtualDesktopManagerInternal_Win11};
            for (const auto &iid : qiIIDs) {
                hr = vdMgr->QueryInterface(*iid, ComPtrAsVoid(m_virtualDesktopManagerInternal));
                if (SUCCEEDED(hr) && m_virtualDesktopManagerInternal != nullptr) {
                    m_iidVirtualDesktop = (iid == &IID_IVirtualDesktopManagerInternal_Win10) ? IID_IVirtualDesktop_Win10 : __uuidof(IVirtualDesktop);
                    Log(L"[INFO] IVirtualDesktopManagerInternal created (QI from vdMgr)");
                    return true;
                }
            }
        }
    }

    Log(L"[ERROR] Failed to query IVirtualDesktopManagerInternal");
    return false;
}

void VirtualDesktopHelper::VerifyDesktopIID() {
    Microsoft::WRL::ComPtr<IVirtualDesktop> testDesktop;
    if (SUCCEEDED(m_virtualDesktopManagerInternal->GetCurrentDesktop(&testDesktop))) {
        Microsoft::WRL::ComPtr<IUnknown> testQI;
        if (FAILED(testDesktop->QueryInterface(m_iidVirtualDesktop, ComPtrAsVoid(testQI)))) {
            m_iidVirtualDesktop = (IsEqualGUID(m_iidVirtualDesktop, IID_IVirtualDesktop_Win10) != 0) ? __uuidof(IVirtualDesktop) : IID_IVirtualDesktop_Win10;
        }
    }
}

void VirtualDesktopHelper::InitViewCollection(Microsoft::WRL::ComPtr<IServiceProvider> &sp) {
    HRESULT hr = sp->QueryService(IID_IApplicationViewCollection_1903,
                                  IID_IApplicationViewCollection_1903,
                                  ComPtrAsVoid(m_viewCollection));
    if (FAILED(hr)) {
        hr = sp->QueryService(IID_IApplicationViewCollection_1809,
                              IID_IApplicationViewCollection_1809,
                              ComPtrAsVoid(m_viewCollection));
    }
    if (SUCCEEDED(hr)) {
        Log(L"[INFO] IApplicationViewCollection created successfully");
    } else {
        Log(L"[ERROR] Failed to query IApplicationViewCollection: 0x" + std::to_wstring(static_cast<uint32_t>(hr)));
    }
}

void VirtualDesktopHelper::InitDesktopManager() {
    HRESULT hr = CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IVirtualDesktopManager, ComPtrAsVoid(m_virtualDesktopManager));
    if (FAILED(hr)) {
        Log(L"[ERROR] Failed to create IVirtualDesktopManager: 0x" + std::to_wstring(static_cast<uint32_t>(hr)));
    } else {
        Log(L"[INFO] IVirtualDesktopManager created successfully");
    }
}

int VirtualDesktopHelper::GetDesktopCount() const {
    if (m_virtualDesktopManagerInternal == nullptr) {
        return 0;
    }

    Microsoft::WRL::ComPtr<IObjectArray> desktops;
    if (FAILED(m_virtualDesktopManagerInternal->GetDesktops(&desktops))) {
        return 0;
    }

    UINT count = 0;
    return SUCCEEDED(desktops->GetCount(&count)) ? static_cast<int>(count) : 0;
}

int VirtualDesktopHelper::GetCurrentDesktopIndex() const {
    if (m_virtualDesktopManagerInternal == nullptr) {
        return -1;
    }

    Microsoft::WRL::ComPtr<IVirtualDesktop> currentDesktop;
    if (FAILED(m_virtualDesktopManagerInternal->GetCurrentDesktop(&currentDesktop))) {
        return -1;
    }

    Microsoft::WRL::ComPtr<IObjectArray> desktops;
    if (FAILED(m_virtualDesktopManagerInternal->GetDesktops(&desktops))) {
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
    if (m_virtualDesktopManagerInternal == nullptr) {
        return;
    }

    Microsoft::WRL::ComPtr<IObjectArray> desktops;
    if (FAILED(m_virtualDesktopManagerInternal->GetDesktops(&desktops))) {
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

    m_virtualDesktopManagerInternal->SwitchDesktop(targetDesktop.Get());
}

void VirtualDesktopHelper::MoveWindowToDesktop(HWND hwnd, int targetIndex) const {
    if (m_virtualDesktopManagerInternal == nullptr || m_viewCollection == nullptr || hwnd == nullptr) {
        return;
    }

    Microsoft::WRL::ComPtr<IUnknown> view;
    HRESULT                          hr = m_viewCollection->GetViewForHwnd(hwnd, &view);
    if (FAILED(hr) || view == nullptr) {
        std::array<wchar_t, 128> titleBuf = {};
        GetWindowTextW(hwnd, titleBuf.data(), static_cast<int>(titleBuf.size()));
        Log(L"[ERROR] MoveWindowToDesktop: GetViewForHwnd failed for \""
            + std::wstring(titleBuf.data()) + L"\" hr=0x" + std::to_wstring(static_cast<uint32_t>(hr)));
        return;
    }

    Microsoft::WRL::ComPtr<IObjectArray> desktops;
    hr = m_virtualDesktopManagerInternal->GetDesktops(&desktops);
    if (FAILED(hr)) {
        Log(L"[ERROR] MoveWindowToDesktop: GetDesktops failed hr=0x"
            + std::to_wstring(static_cast<uint32_t>(hr)));
        return;
    }

    UINT count = 0;
    hr         = desktops->GetCount(&count);
    if (FAILED(hr) || static_cast<size_t>(targetIndex) >= static_cast<size_t>(count)) {
        Log(L"[ERROR] MoveWindowToDesktop: bad index " + std::to_wstring(targetIndex)
            + L" count=" + std::to_wstring(count) + L" hr=0x"
            + std::to_wstring(static_cast<uint32_t>(hr)));
        return;
    }

    Microsoft::WRL::ComPtr<IVirtualDesktop> targetDesktop;
    hr = desktops->GetAt(targetIndex, m_iidVirtualDesktop, ComPtrAsVoid(targetDesktop));
    if (FAILED(hr)) {
        Log(L"[ERROR] MoveWindowToDesktop: GetAt failed for index "
            + std::to_wstring(targetIndex) + L" hr=0x"
            + std::to_wstring(static_cast<uint32_t>(hr)));
        return;
    }

    hr                                = m_virtualDesktopManagerInternal->MoveViewToDesktop(view.Get(), targetDesktop.Get());
    std::array<wchar_t, 128> titleBuf = {};
    GetWindowTextW(hwnd, titleBuf.data(), static_cast<int>(titleBuf.size()));
    Log(L"[INFO] MoveWindowToDesktop: \"" + std::wstring(titleBuf.data()) + L"\" -> desktop "
        + std::to_wstring(targetIndex) + L" hr=0x" + std::to_wstring(static_cast<uint32_t>(hr)));
}

bool VirtualDesktopHelper::IsWindowOnCurrentDesktop(HWND hwnd) const {
    if ((m_virtualDesktopManagerInternal == nullptr) || hwnd == nullptr) {
        return false;
    }
    return CheckViaViewCollection(hwnd) || CheckViaDesktopManager(hwnd);
}

bool VirtualDesktopHelper::CheckViaViewCollection(HWND hwnd) const {
    if (m_viewCollection == nullptr) {
        return false;
    }

    Microsoft::WRL::ComPtr<IUnknown> view;
    if (FAILED(m_viewCollection->GetViewForHwnd(hwnd, view.GetAddressOf())) || (view == nullptr)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IVirtualDesktop> currentDesktop;
    if (FAILED(m_virtualDesktopManagerInternal->GetCurrentDesktop(&currentDesktop)) || (currentDesktop == nullptr)) {
        return false;
    }

    BOOL visible = FALSE;
    return SUCCEEDED(currentDesktop->IsViewVisible(view.Get(), &visible)) && (visible != FALSE);
}

bool VirtualDesktopHelper::CheckViaDesktopManager(HWND hwnd) const {
    if (m_virtualDesktopManager == nullptr) {
        return false;
    }

    BOOL onCurrentDesktop = FALSE;
    if (SUCCEEDED(m_virtualDesktopManager->IsWindowOnCurrentVirtualDesktop(hwnd, &onCurrentDesktop))) {
        return onCurrentDesktop != FALSE;
    }

    GUID windowDesktopId{};
    if (FAILED(m_virtualDesktopManager->GetWindowDesktopId(hwnd, &windowDesktopId))) {
        return false;
    }

    Microsoft::WRL::ComPtr<IVirtualDesktop> currentDesktop;
    if (FAILED(m_virtualDesktopManagerInternal->GetCurrentDesktop(&currentDesktop)) || (currentDesktop == nullptr)) {
        return false;
    }

    GUID currentDesktopId{};
    return SUCCEEDED(currentDesktop->GetID(&currentDesktopId)) && IsEqualGUID(windowDesktopId, currentDesktopId) != 0;
}

std::array<bool, kMaxDesktops> VirtualDesktopHelper::GetDesktopEmptyMask() const {
    std::array<bool, kMaxDesktops> emptyMask{};
    if ((m_virtualDesktopManagerInternal == nullptr) || (m_viewCollection == nullptr)) {
        return emptyMask;
    }

    Microsoft::WRL::ComPtr<IObjectArray> desktops;
    if (FAILED(m_virtualDesktopManagerInternal->GetDesktops(&desktops))) {
        return emptyMask;
    }

    UINT desktopCount = 0;
    if (FAILED(desktops->GetCount(&desktopCount)) || desktopCount == 0) {
        return emptyMask;
    }

    Microsoft::WRL::ComPtr<IObjectArray> views;
    if (FAILED(m_viewCollection->GetViews(&views))) {
        return emptyMask;
    }

    UINT viewCount = 0;
    if (FAILED(views->GetCount(&viewCount))) {
        return emptyMask;
    }

    std::array<int, kMaxDesktops> windowCounts{};

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
            ++windowCounts.at(visibleDesktop);
        }
    }

    for (UINT i = 0; i < desktopCount && i < kMaxDesktops; ++i) {
        emptyMask.at(i) = (windowCounts.at(i) == 0);
    }
    return emptyMask;
}
