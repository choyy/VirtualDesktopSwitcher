#include <windows.h>
#include <commctrl.h>
#include "Application.h"

int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/) {
    HRESULT hr = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (FAILED(hr)) {
        SetProcessDPIAware();
    }

    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    Application app;
    if (!app.Initialize()) {
        return -1;
    }
    return app.Run();
}
