#include <windows.h>

#include <commctrl.h>

#include <cstring>

#include "core/Application.h"
#include "core/UpdateChecker.h"
#include "util/Log.h"

int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR lpCmdLine, int /*nCmdShow*/) {
    if (lpCmdLine != nullptr && strstr(lpCmdLine, "--check-updates") != nullptr) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        auto hasUpdate = UpdateChecker::CheckForNewerVersion();
        CoUninitialize();
        return hasUpdate ? 1 : 0;
    }

    const HRESULT hr = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (FAILED(hr)) {
        SetProcessDPIAware();
    }

    const INITCOMMONCONTROLSEX icc = {.dwSize = sizeof(icc), .dwICC = ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    Log(L"-----------------------------------------------------------------");
    Log(L"[INFO] Virtual Desktop Switcher start...");
    Application app;
    if (!app.Initialize()) {
        return -1;
    }
    return Application::Run();
}
