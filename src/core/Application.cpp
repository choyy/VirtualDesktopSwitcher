#include "Application.h"

#include <bit>
#include <iostream>
#include <sstream>

#include "ui/AboutDialog.h"
#include "ui/SettingsDialog.h"
#include "util/ColorState.h"

namespace {

std::wstring BuildTooltipText(int desktopCount, int currentDesktop) {
    std::wostringstream oss;
    oss << L"虚拟桌面切换器\n"
        << L"桌面：" << desktopCount << L" | 当前：" << (currentDesktop + 1);
    return oss.str();
}

} // namespace

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
            int         colorIdx = static_cast<int>(item - CMD_COLOR_OPTIONS_BASE);
            const auto &colors   = GetPredefinedColors();
            if (colorIdx >= 0 && static_cast<size_t>(colorIdx) < colors.size()) {
                if (pApp->m_pOverlay != nullptr) {
                    pApp->m_pOverlay->SetColorPreview(colors[colorIdx]);
                }
            }
        }
        return 0;
    }

    case WM_SWITCH_DESKTOP: {
        pApp->m_switcher->SwitchToDesktop(static_cast<int>(wParam));
        int desktopCount         = pApp->m_switcher->GetDesktopCount();
        int currentDesktop       = pApp->m_switcher->GetCurrentDesktopIndex();
        pApp->m_lastDesktopIndex = currentDesktop;

        pApp->m_pTrayIcon->UpdateTooltip(BuildTooltipText(desktopCount, currentDesktop));

        if (pApp->m_pOverlay) {
            auto emptyMask = pApp->m_switcher->GetDesktopEmptyMask();
            pApp->m_pOverlay->SetDesktopState(desktopCount, currentDesktop, emptyMask);
        }
        return 0;
    }

    case WM_TIMER: {
        int desktopCount   = pApp->m_switcher->GetDesktopCount();
        int currentDesktop = pApp->m_switcher->GetCurrentDesktopIndex();
        if (currentDesktop != pApp->m_lastDesktopIndex) {
            pApp->m_lastDesktopIndex = currentDesktop;
            pApp->m_pTrayIcon->UpdateTooltip(BuildTooltipText(desktopCount, currentDesktop));
            if (pApp->m_pOverlay) {
                auto emptyMask = pApp->m_switcher->GetDesktopEmptyMask();
                pApp->m_pOverlay->SetDesktopState(desktopCount, currentDesktop, emptyMask);
            }
        }
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    default:
        break;
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
        std::cerr << "Failed to register window class!\n";
        return false;
    }

    m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                             L"VirtualDesktopSwitcherClass", L"VirtualDesktopSwitcher",
                             WS_POPUP,
                             0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (m_hwnd == nullptr) {
        std::cerr << "Failed to create window!\n";
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
        std::cerr << "Failed to initialize tray icon!\n";
        return false;
    }

    // Show current desktop info
    int desktopCount   = m_switcher->GetDesktopCount();
    int currentDesktop = m_switcher->GetCurrentDesktopIndex();
    m_lastDesktopIndex = currentDesktop;
    m_pTrayIcon->UpdateTooltip(BuildTooltipText(desktopCount, currentDesktop));

    // Initialize desktop overlay
    if (!m_pOverlay->Initialize(GetModuleHandle(nullptr))) {
        std::cerr << "Failed to initialize desktop indicator!\n";
    } else {
        auto emptyMask = m_switcher->GetDesktopEmptyMask();
        m_pOverlay->SetDesktopState(desktopCount, currentDesktop, emptyMask);
        m_pOverlay->Show();
    }

    // Set up tray icon callbacks
    m_pTrayIcon->SetColorCallback([this](const std::wstring &hex) {
        if (m_pOverlay != nullptr) {
            m_pOverlay->SetColor(hex);
        }
    });
    m_pTrayIcon->SetEditModeCallback([this]() {
        if (m_pOverlay != nullptr) {
            m_pOverlay->ToggleEditMode();
        }
    });
    m_pTrayIcon->SetPositionCallback([this](int preset) {
        if (m_pOverlay != nullptr) {
            m_pOverlay->SetPositionPreset(preset);
        }
    });
    m_pTrayIcon->SetSettingsCallback([this]() {
        if (m_pOverlay == nullptr) {
            return;
        }
        SettingsDialog::Result cur = {
            .currentSymbol = m_pOverlay->GetCurrentSymbol(),
            .otherSymbol   = m_pOverlay->GetOtherSymbol(),
            .emptySymbol   = m_pOverlay->GetEmptySymbol(),
            .fontName      = m_pOverlay->GetFontName(),
            .charSpacing   = m_pOverlay->GetCharSpacing()};

        auto previewCb = [this](const SettingsDialog::Result &preview) {
            m_pOverlay->SetCurrentSymbol(preview.currentSymbol);
            m_pOverlay->SetOtherSymbol(preview.otherSymbol);
            m_pOverlay->SetEmptySymbol(preview.emptySymbol);
            m_pOverlay->SetFontName(preview.fontName);
            m_pOverlay->SetCharSpacing(preview.charSpacing);
        };

        SettingsDialog::Result res = SettingsDialog::Show(m_hwnd, cur, previewCb);
        if (res.accepted) {
            previewCb(res);
        } else {
            previewCb(cur); // Restore original values on cancel
        }
    });
    m_pTrayIcon->SetAboutCallback([this]() {
        if (m_pOverlay == nullptr) {
            return;
        }
        auto res = AboutDialog::Show(m_hwnd, m_pOverlay->IsAutoCheckUpdates());
        if (res.accepted && res.autoCheckUpdates != m_pOverlay->IsAutoCheckUpdates()) {
            m_pOverlay->SetAutoCheckUpdates(res.autoCheckUpdates);
        }
    });

    // Check for updates on startup
    if (m_pOverlay != nullptr && m_pOverlay->IsAutoCheckUpdates()) {
        auto checkResult = AboutDialog::CheckForNewerVersion();
        if (checkResult.hasUpdate) {
            int ret = MessageBoxW(m_hwnd, L"A new version is available. Download now?",
                                  L"Update Available", MB_YESNO | MB_ICONQUESTION);
            if (ret == IDYES && !checkResult.downloadUrl.empty()) {
                AboutDialog::DownloadUpdate(m_hwnd, checkResult.downloadUrl);
            }
        }
    }

    // Install keyboard hook
    if (!m_switcher->InstallHook()) {
        std::cerr << "Failed to install keyboard hook!\n";
        return false;
    }

    // Polling timer to detect desktop switches not triggered by our hook (taskbar, Win+Tab, etc.)
    SetTimer(m_hwnd, 1, 300, nullptr);

    return true;
}

int Application::Run() {
    std::cout << "Virtual Desktop Switcher running...\n";
    std::cout << "Use Alt + [1-9] to switch desktops\n";
    std::cout << "Use tray icon to exit or manage autostart\n";

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
