#include "Application.h"

#include <string>

#include "core/UpdateChecker.h"
#include "core/VirtualDesktopSwitcher.h"
#include "ui/AboutDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/TrayIcon.h"
#include "util/ConfigIni.h"
#include "util/Log.h"
#include "util/Utils.h"

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
    auto *pApp = GetWndUserData<Application>(hwnd);
    if (pApp == nullptr) {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
    case WM_MEASUREITEM:
    case WM_DRAWITEM:
    case WM_TRAYICON:
    case WM_COMMAND:
        pApp->OnTrayMessage(uMsg, wParam, lParam, hwnd);
        return (uMsg == WM_MEASUREITEM) ? TRUE : 0;

    case WM_MENUSELECT:
        pApp->OnMenuSelect(wParam, lParam);
        return 0;

    case WM_SWITCH_DESKTOP:
        pApp->OnDesktopSwitch(wParam);
        return 0;

    case WM_TIMER:
        pApp->OnTimerTick();
        return 0;

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            pApp->OnSystemResume();
        }
        return TRUE;

    case WM_DESTROY:
        Application::OnDestroy(hwnd);
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

void Application::OnTrayMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, HWND hwnd) {
    m_pTrayIcon->HandleMessage(hwnd, uMsg, wParam, lParam);
}

void Application::OnMenuSelect(WPARAM wParam, LPARAM /*lParam*/) {
    UINT item  = LOWORD(wParam);
    UINT flags = HIWORD(wParam);
    if (flags == 0xFFFF) {
        if (m_pOverlay) { m_pOverlay->CancelPreview(); }
    } else if ((flags & MF_POPUP) == 0u) {
        int colorIdx = static_cast<int>(item - CMD_COLOR_OPTIONS_BASE);
        if (colorIdx >= 0 && static_cast<size_t>(colorIdx) < kPredefinedColors.size()) {
            if (m_pOverlay) { m_pOverlay->SetColorPreview(kPredefinedColors.at(colorIdx)); }
        }
    }
}

void Application::OnDesktopSwitch(WPARAM wParam) {
    m_switcher->SwitchToDesktop(static_cast<int>(wParam));
    SyncDesktopState();
}

void Application::OnTimerTick() {
    SyncDesktopState();
}

void Application::OnDestroy(HWND hwnd) {
    KillTimer(hwnd, 1);
    PostQuitMessage(0);
}

void Application::ApplySettingsPreview(const SettingsDialog::Result &r) {
    if (m_pOverlay == nullptr) { return; }
    m_indicatorCfg.currentSymbol = r.currentSymbol;
    m_indicatorCfg.otherSymbol   = r.otherSymbol;
    m_indicatorCfg.emptySymbol   = r.emptySymbol;
    m_indicatorCfg.fontName      = r.fontName;
    m_indicatorCfg.charSpacing   = r.charSpacing;
    m_pOverlay->SetCurrentSymbol(r.currentSymbol);
    m_pOverlay->SetOtherSymbol(r.otherSymbol);
    m_pOverlay->SetEmptySymbol(r.emptySymbol);
    m_pOverlay->SetFontName(r.fontName);
    m_pOverlay->SetCharSpacing(r.charSpacing);
}

bool Application::Initialize() {
    m_switcher = std::make_unique<VirtualDesktopSwitcher>();

    HINSTANCE hInst = GetModuleHandle(nullptr);

    WNDCLASSW wc     = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"VirtualDesktopSwitcherClass";

    if (RegisterClassW(&wc) == 0) {
        Log(L"[ERROR] Failed to register window class");
        return false;
    }

    m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                             L"VirtualDesktopSwitcherClass", L"VirtualDesktopSwitcher",
                             WS_POPUP,
                             0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);

    if (m_hwnd == nullptr) {
        Log(L"[ERROR] Failed to create window");
        return false;
    }
    m_switcher->SetWindowHandle(m_hwnd);
    SetWndUserData(m_hwnd, this);

    m_autoCheckUpdates = ReadIniInt(L"General", L"AutoCheckUpdates", 1) != 0;

    m_indicatorCfg.LoadFromIni();

    m_pOverlay = std::make_unique<DesktopIndicator>();
    m_pOverlay->SetConfig(&m_indicatorCfg);
    m_pOverlay->SetOnConfigChanged([this]() {
        m_indicatorCfg.SaveToIni();
    });

    // Initialize tray icon with saved preset
    m_pTrayIcon = std::make_unique<TrayIcon>();
    m_pTrayIcon->SetActivePositionPreset(m_indicatorCfg.positionPreset);
    if (!m_pTrayIcon->Initialize(m_hwnd, hInst)) {
        Log(L"[ERROR] Failed to initialize tray icon");
        return false;
    }

    // Show current desktop info
    int desktopCount   = m_switcher->GetDesktopCount();
    int currentDesktop = m_switcher->GetCurrentDesktopIndex();
    m_lastDesktopIndex = currentDesktop;
    m_pTrayIcon->UpdateTooltip(BuildTooltipText(desktopCount, currentDesktop));

    // Initialize desktop overlay
    if (!m_pOverlay->Initialize(hInst)) {
        Log(L"[ERROR] Failed to initialize desktop indicator");
    } else {
        auto emptyMask = m_switcher->GetDesktopEmptyMask();
        m_pOverlay->SetDesktopState(desktopCount, currentDesktop, emptyMask);
        m_pOverlay->Show();
    }

    // Set up tray icon callbacks
    m_pTrayIcon->SetColorCallback([this](const std::wstring &hex) {
        if (m_pOverlay) {
            m_pOverlay->SetColor(hex);
            m_indicatorCfg.SaveToIni();
        }
    });
    m_pTrayIcon->SetEditModeCallback([this]() {
        if (m_pOverlay) { m_pOverlay->ToggleEditMode(); }
    });
    m_pTrayIcon->SetPositionCallback([this](int preset) {
        if (m_pOverlay) {
            m_pOverlay->SetPositionPreset(preset);
            m_indicatorCfg.SaveToIni();
        }
    });
    m_pTrayIcon->SetSettingsCallback([this]() {
        if (m_pOverlay == nullptr) { return; }

        SettingsDialog::Result cur = {
            .currentSymbol = m_indicatorCfg.currentSymbol,
            .otherSymbol   = m_indicatorCfg.otherSymbol,
            .emptySymbol   = m_indicatorCfg.emptySymbol,
            .fontName      = m_indicatorCfg.fontName,
            .charSpacing   = m_indicatorCfg.charSpacing};

        SettingsDialog::Result res = SettingsDialog::Show(m_hwnd, cur, [](const SettingsDialog::Result &preview, void *ctx) { static_cast<Application *>(ctx)->ApplySettingsPreview(preview); }, this);

        if (res.accepted) {
            m_indicatorCfg.SaveToIni();
        } else {
            ApplySettingsPreview(cur);
        }
    });
    m_pTrayIcon->SetAboutCallback([this]() {
        if (m_pOverlay == nullptr) { return; }
        auto res = AboutDialog::Show(m_hwnd, m_autoCheckUpdates);
        if (res.accepted && res.autoCheckUpdates != m_autoCheckUpdates) {
            m_autoCheckUpdates = res.autoCheckUpdates;
            WriteIniInt(L"General", L"AutoCheckUpdates", m_autoCheckUpdates ? 1 : 0);
        }
    });

    // Register taskbar creation message (for Explorer restart / wake from sleep)
    m_uTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    // Check for updates on startup
    if (m_autoCheckUpdates) {
        UpdateChecker::CheckAndDownload(m_hwnd, true);
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
