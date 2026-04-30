#include "Application.h"

#include <bit>
#include <string>

#include "core/VirtualDesktopSwitcher.h"
#include "ui/AboutDialog.h"
#include "ui/DesktopIndicator.h"
#include "ui/SettingsDialog.h"
#include "ui/TrayIcon.h"
#include "util/ColorState.h"
#include "util/Log.h"

namespace {

std::wstring BuildTooltipText(int desktopCount, int currentDesktop) {
    return L"虚拟桌面切换器\n桌面：" + std::to_wstring(desktopCount) + L" | 当前：" + std::to_wstring(currentDesktop + 1);
}

} // namespace

Application::Application() = default;

Application::~Application() {
    if (m_hwnd != nullptr) {
        DestroyWindow(m_hwnd);
    }
}

LRESULT CALLBACK Application::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto *pApp = std::bit_cast<Application *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (pApp == nullptr) {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
    case WM_MEASUREITEM:
    case WM_DRAWITEM: {
        pApp->m_pTrayIcon->HandleMessage(hwnd, uMsg, wParam, lParam);
        return (uMsg == WM_MEASUREITEM) ? TRUE : 0;
    }

    case WM_TRAYICON:
    case WM_COMMAND:
        pApp->m_pTrayIcon->HandleMessage(hwnd, uMsg, wParam, lParam);
        return 0;

    case WM_MENUSELECT: {
        UINT item  = LOWORD(wParam);
        UINT flags = HIWORD(wParam);
        if (flags == 0xFFFF) {
            if (pApp->m_pOverlay != nullptr) {
                pApp->m_pOverlay->CancelPreview();
            }
        } else if ((flags & MF_POPUP) == 0u) {
            int colorIdx = static_cast<int>(item - CMD_COLOR_OPTIONS_BASE);
            if (colorIdx >= 0 && static_cast<size_t>(colorIdx) < kPredefinedColors.size()) {
                if (pApp->m_pOverlay != nullptr) {
                    pApp->m_pOverlay->SetColorPreview(kPredefinedColors.at(colorIdx));
                }
            }
        }
        return 0;
    }

    case WM_SWITCH_DESKTOP: {
        pApp->m_switcher->SwitchToDesktop(static_cast<int>(wParam));
        pApp->SyncDesktopState();
        return 0;
    }

    case WM_TIMER: {
        int currentDesktop = pApp->m_switcher->GetCurrentDesktopIndex();
        if (currentDesktop != pApp->m_lastDesktopIndex) {
            pApp->SyncDesktopState();
        }
        return 0;
    }

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            pApp->OnSystemResume();
        }
        return TRUE;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    if (uMsg == pApp->m_uTaskbarCreated) {
        pApp->OnSystemResume();
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool Application::Initialize() {
    m_switcher = std::make_unique<VirtualDesktopSwitcher>();

    WNDCLASSW wc     = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.lpszClassName = L"VirtualDesktopSwitcherClass";

    if (RegisterClassW(&wc) == 0) {
        Log(L"[ERROR] Failed to register window class");
        return false;
    }

    m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                             L"VirtualDesktopSwitcherClass", L"VirtualDesktopSwitcher",
                             WS_POPUP,
                             0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (m_hwnd == nullptr) {
        Log(L"[ERROR] Failed to create window");
        return false;
    }
    m_switcher->SetWindowHandle(m_hwnd);
    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, std::bit_cast<LONG_PTR>(this));

    // Load saved config first (DesktopIndicator constructor calls LoadConfig)
    m_pOverlay = std::make_unique<DesktopIndicator>();

    // Initialize tray icon with saved preset
    m_pTrayIcon = std::make_unique<TrayIcon>();
    m_pTrayIcon->SetActivePositionPreset(m_pOverlay->GetPositionPreset());
    if (!m_pTrayIcon->Initialize(m_hwnd, GetModuleHandle(nullptr))) {
        Log(L"[ERROR] Failed to initialize tray icon");
        return false;
    }

    // Show current desktop info
    int desktopCount   = m_switcher->GetDesktopCount();
    int currentDesktop = m_switcher->GetCurrentDesktopIndex();
    m_lastDesktopIndex = currentDesktop;
    m_pTrayIcon->UpdateTooltip(BuildTooltipText(desktopCount, currentDesktop));

    // Initialize desktop overlay
    if (!m_pOverlay->Initialize(GetModuleHandle(nullptr))) {
        Log(L"[ERROR] Failed to initialize desktop indicator");
    } else {
        auto emptyMask = m_switcher->GetDesktopEmptyMask();
        m_pOverlay->SetDesktopState(desktopCount, currentDesktop, emptyMask);
        m_pOverlay->Show();
    }

    // Set up tray icon callbacks
    m_pTrayIcon->SetColorCallback([](const std::wstring &hex, void *ctx) {
        auto *app = static_cast<Application *>(ctx);
        if (app->m_pOverlay != nullptr) { app->m_pOverlay->SetColor(hex); }
    },
                                  this);
    m_pTrayIcon->SetEditModeCallback([](void *ctx) {
        auto *app = static_cast<Application *>(ctx);
        if (app->m_pOverlay != nullptr) { app->m_pOverlay->ToggleEditMode(); }
    },
                                     this);
    m_pTrayIcon->SetPositionCallback([](int preset, void *ctx) {
        auto *app = static_cast<Application *>(ctx);
        if (app->m_pOverlay != nullptr) { app->m_pOverlay->SetPositionPreset(preset); }
    },
                                     this);
    m_pTrayIcon->SetSettingsCallback([](void *ctx) {
        auto *app = static_cast<Application *>(ctx);
        if (app->m_pOverlay == nullptr) { return; }
        SettingsDialog::Result cur = {
            .currentSymbol = app->m_pOverlay->GetCurrentSymbol(),
            .otherSymbol   = app->m_pOverlay->GetOtherSymbol(),
            .emptySymbol   = app->m_pOverlay->GetEmptySymbol(),
            .fontName      = app->m_pOverlay->GetFontName(),
            .charSpacing   = app->m_pOverlay->GetCharSpacing()};

        SettingsDialog::Result res = SettingsDialog::Show(app->m_hwnd, cur, [](const SettingsDialog::Result &preview, void *ctx2) {
                auto *app2 = static_cast<Application *>(ctx2);
                app2->m_pOverlay->SetCurrentSymbol(preview.currentSymbol);
                app2->m_pOverlay->SetOtherSymbol(preview.otherSymbol);
                app2->m_pOverlay->SetEmptySymbol(preview.emptySymbol);
                app2->m_pOverlay->SetFontName(preview.fontName);
                app2->m_pOverlay->SetCharSpacing(preview.charSpacing); }, app);
        if (res.accepted) {
            app->m_pOverlay->SetCurrentSymbol(res.currentSymbol);
            app->m_pOverlay->SetOtherSymbol(res.otherSymbol);
            app->m_pOverlay->SetEmptySymbol(res.emptySymbol);
            app->m_pOverlay->SetFontName(res.fontName);
            app->m_pOverlay->SetCharSpacing(res.charSpacing);
        }
    },
                                     this);
    m_pTrayIcon->SetAboutCallback([](void *ctx) {
        auto *app = static_cast<Application *>(ctx);
        if (app->m_pOverlay == nullptr) { return; }
        auto res = AboutDialog::Show(app->m_hwnd, app->m_pOverlay->IsAutoCheckUpdates());
        if (res.accepted && res.autoCheckUpdates != app->m_pOverlay->IsAutoCheckUpdates()) {
            app->m_pOverlay->SetAutoCheckUpdates(res.autoCheckUpdates);
        }
    },
                                  this);

    // Register taskbar creation message (for Explorer restart / wake from sleep)
    m_uTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    // Check for updates on startup
    if (m_pOverlay != nullptr && m_pOverlay->IsAutoCheckUpdates()) {
        AboutDialog::CheckAndDownload(m_hwnd, true);
    }

    // Install keyboard hook
    if (!m_switcher->InstallHook()) {
        Log(L"[ERROR] Failed to install keyboard hook");
        return false;
    }

    // Polling timer to detect desktop switches not triggered by our hook (taskbar, Win+Tab, etc.)
    SetTimer(m_hwnd, 1, 300, nullptr);

    return true;
}

void Application::SyncDesktopState() {
    int desktopCount   = m_switcher->GetDesktopCount();
    int currentDesktop = m_switcher->GetCurrentDesktopIndex();
    m_lastDesktopIndex = currentDesktop;
    m_pTrayIcon->UpdateTooltip(BuildTooltipText(desktopCount, currentDesktop));
    if (m_pOverlay) {
        auto emptyMask = m_switcher->GetDesktopEmptyMask();
        m_pOverlay->SetDesktopState(desktopCount, currentDesktop, emptyMask);
    }
}

void Application::OnSystemResume() {
    m_switcher->ReinstallHook();
    m_pTrayIcon->Reinitialize();
    SyncDesktopState();
}

int Application::Run() {
    Log(L"-----------------------------------------------------------------");
    Log(L"[INFO] Virtual Desktop Switcher start...");
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
