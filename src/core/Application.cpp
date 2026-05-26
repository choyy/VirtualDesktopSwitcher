#include "Application.h"

#include <string>

#include "core/VirtualDesktopSwitcher.h"
#include "ui/AboutDialog.h"
#include "ui/DesktopIndicator.h"
#include "ui/SettingsDialog.h"
#include "ui/TrayIcon.h"
#include "util/ConfigIni.h"
#include "util/Lang.h"
#include "util/Log.h"
#include "util/UpdateChecker.h"
#include "util/Utils.h"

namespace {

std::wstring BuildTooltipText(int desktopCount, int currentDesktop) {
    return std::wstring(Lang::Get(L"Tray.DefaultTip")) + L"\n" + Lang::Get(L"Tooltip.Desktops") + std::to_wstring(desktopCount) + L" | " + Lang::Get(L"Tooltip.Current") + std::to_wstring(currentDesktop + 1);
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

    case WM_TOGGLE_PIN_ALL_DESKTOPS:
        if (HWND fore = GetForegroundWindow()) {
            if (pApp->m_switcher->IsWindowPinned(fore)) {
                pApp->m_switcher->UnpinWindow(fore);
            } else {
                pApp->m_switcher->PinWindow(fore);
            }
        }
        return 0;

    case WM_TIMER:
        pApp->OnTimerTick();
        return 0;

    case WM_DISPLAYCHANGE:
        pApp->OnDisplayChange();
        return 0;

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            pApp->OnSystemResume();
        }
        return TRUE;

    case WM_DESTROY:
        pApp->OnDestroy(hwnd);
        return 0;
    default:
        break;
    }

    static UINT uTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    if (uMsg == uTaskbarCreated) {
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

void Application::PollUpdateProcess() {
    if (m_hUpdateProcess == nullptr || WaitForSingleObject(m_hUpdateProcess, 0) != WAIT_OBJECT_0) {
        return;
    }

    KillTimer(m_hwnd, 2);

    DWORD exitCode = 0;
    GetExitCodeProcess(m_hUpdateProcess, &exitCode);
    CloseHandle(m_hUpdateProcess);
    m_hUpdateProcess = nullptr;

    if (exitCode != 1) { return; }

    if (MessageBoxW(m_hwnd, Lang::Get(L"Update.FoundMsg"), Lang::Get(L"Update.FoundTitle"),
                    MB_YESNO | MB_ICONQUESTION)
        == IDYES) {
        UpdateChecker::DownloadUpdate(m_hwnd);
    }
}

void Application::SpawnUpdateCheckProcess() {
    std::wstring exePath = GetCurrentExePath();
    std::wstring cmdLine = L"\"" + exePath + L"\" --check-updates";

    PROCESS_INFORMATION pi{};
    STARTUPINFOW        si = {.cb = sizeof(si)};
    if (CreateProcessW(exePath.c_str(), cmdLine.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW | IDLE_PRIORITY_CLASS, nullptr, nullptr, &si, &pi)
        != 0) {
        CloseHandle(pi.hThread);
        m_hUpdateProcess = pi.hProcess;
        SetTimer(m_hwnd, 2, 3000, TimerPollUpdateProc);
    }
}

void CALLBACK Application::TimerPollUpdateProc(HWND hwnd, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/) {
    auto *pApp = GetWndUserData<Application>(hwnd);
    if (pApp != nullptr) {
        pApp->PollUpdateProcess();
    }
}

void Application::OnDestroy(HWND hwnd) {
    if (m_hUpdateProcess != nullptr) {
        WaitForSingleObject(m_hUpdateProcess, 500);
        CloseHandle(m_hUpdateProcess);
    }
    KillTimer(hwnd, 1);
    KillTimer(hwnd, 2);
    PostQuitMessage(0);
}

void Application::ApplySettingsPreview(const SettingsDialog::Result &r) {
    if (m_pOverlay == nullptr) { return; }
    m_pOverlay->SetCurrentSymbol(r.currentSymbol);
    m_pOverlay->SetOtherSymbol(r.otherSymbol);
    m_pOverlay->SetEmptySymbol(r.emptySymbol);
    m_pOverlay->SetFontName(r.fontName);
    m_pOverlay->SetCharSpacing(r.charSpacing);
}

bool Application::CreateHiddenWindow() {
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

    SetWndUserData(m_hwnd, this);
    return true;
}

void Application::LoadConfiguration() {
    m_autoCheckUpdates = ReadIniInt(L"General", L"AutoCheckUpdates", 1) != 0;
    m_modMask          = static_cast<uint8_t>(ReadIniInt(L"General", L"ModMask", 1));
    m_indicatorCfg.LoadFromIni();
    VirtualDesktopSwitcher::SetModMask(m_modMask);
    VirtualDesktopSwitcher::SetPrevDesktopKey(
        static_cast<uint8_t>(ReadIniInt(L"General", L"PrevDesktopKey", VK_OEM_3)));
    VirtualDesktopSwitcher::SetPinAllDesktopsKey(
        static_cast<uint8_t>(ReadIniInt(L"General", L"PinAllDesktopsKey", 'D')));
    for (int i = 0; i < static_cast<int>(kMaxDesktops); ++i) {
        std::wstring keyName = L"DesktopKey" + std::to_wstring(i + 1);
        VirtualDesktopSwitcher::SetDesktopKey(i,
                                              static_cast<uint8_t>(ReadIniInt(L"General", keyName, '1' + i)));
    }
}

void Application::InitializeOverlay() {
    m_pOverlay = std::make_unique<DesktopIndicator>();
    m_pOverlay->SetConfig(&m_indicatorCfg);
}

bool Application::InitializeTrayIcon() {
    m_pTrayIcon = std::make_unique<TrayIcon>();
    m_pTrayIcon->SetActivePositionPreset(m_indicatorCfg.positionPreset);
    if (!m_pTrayIcon->Initialize(m_hwnd, GetModuleHandle(nullptr))) {
        Log(L"[ERROR] Failed to initialize tray icon");
        return false;
    }
    return true;
}

void Application::SetupTrayCallbacks() {
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
            .charSpacing   = m_indicatorCfg.charSpacing,
            .modMask       = m_modMask};

        SettingsDialog::Result res = SettingsDialog::Show(m_hwnd, cur,
                                                          [this](const SettingsDialog::Result &preview) {
                                                              ApplySettingsPreview(preview);
                                                          });

        if (res.accepted) {
            m_indicatorCfg.SaveToIni();
            if (res.modMask != m_modMask) {
                m_modMask = res.modMask;
                WriteIniInt(L"General", L"ModMask", m_modMask);
                VirtualDesktopSwitcher::SetModMask(m_modMask);
            }
        } else {
            ApplySettingsPreview(cur);
        }
    });
    m_pTrayIcon->SetAboutCallback([this]() {
        if (m_pOverlay == nullptr) { return; }
        bool autoCheckUpdates = AboutDialog::Show(m_hwnd, m_autoCheckUpdates);
        if (autoCheckUpdates != m_autoCheckUpdates) {
            m_autoCheckUpdates = autoCheckUpdates;
            WriteIniInt(L"General", L"AutoCheckUpdates", m_autoCheckUpdates ? 1 : 0);
        }
    });
    m_pTrayIcon->SetShowModeCallback([this](int mode) {
        if (m_pOverlay) { m_pOverlay->SetShowMode(static_cast<ShowMode>(mode)); }
    });
    m_pTrayIcon->SetAnimModeCallback([this](bool on) {
        if (m_pOverlay) { m_pOverlay->SetAnimMode(on); }
    });
    m_pTrayIcon->SetAutoContrastCallback([this](bool on) {
        if (m_pOverlay) { m_pOverlay->SetAutoContrast(on); }
    });
}

bool Application::Initialize() {
    m_switcher = std::make_unique<VirtualDesktopSwitcher>();
    Lang::Init();

    if (!CreateHiddenWindow()) { return false; }

    LoadConfiguration();

    InitializeOverlay();

    if (!InitializeTrayIcon()) { return false; }

    bool overlayOk = m_pOverlay->Initialize(GetModuleHandle(nullptr));
    if (!overlayOk) {
        Log(L"[ERROR] Failed to initialize desktop indicator");
    }

    SyncDesktopState();

    if (overlayOk) {
        if (m_indicatorCfg.animMode != 0) { m_pOverlay->SetAnimMode(true); }
        if (m_indicatorCfg.autoContrast) { m_pOverlay->SetAutoContrast(true); }
        m_pOverlay->SetScrollSwitchCallback([this](int target) {
            OnDesktopSwitch(target);
        });
        m_pOverlay->SetMoveWindowCallback([this](int targetIdx, HWND hwnd) {
            if (m_switcher) {
                m_switcher->MoveWindowToDesktop(hwnd, targetIdx);
                m_switcher->SwitchToDesktop(targetIdx);
                SyncDesktopState();
            }
        });
    }

    SetupTrayCallbacks();

    if (m_autoCheckUpdates) {
        SpawnUpdateCheckProcess();
    }

    if (!m_switcher->InstallHook(m_hwnd)) {
        Log(L"[ERROR] Failed to install keyboard hook");
        return false;
    }

    SetTimer(m_hwnd, 1, 1000, nullptr);
    return true;
}

void Application::SyncDesktopState() {
    int desktopCount   = m_switcher->GetDesktopCount();
    int currentDesktop = m_switcher->GetCurrentDesktopIndex();

    bool changed = (currentDesktop != m_lastDesktopIndex) || (desktopCount != m_lastDesktopCount);
    if (changed) {
        m_lastDesktopIndex = currentDesktop;
        m_lastDesktopCount = desktopCount;
        m_pTrayIcon->UpdateTooltip(BuildTooltipText(desktopCount, currentDesktop));
        if (m_pOverlay) { m_pOverlay->ShowTemporarily(); }
    }

    if (m_pOverlay) {
        auto emptyMask = m_switcher->GetDesktopEmptyMask();
        m_pOverlay->SetDesktopState(desktopCount, currentDesktop, emptyMask);
    }
}

void Application::OnDisplayChange() {
    if (m_pOverlay) { m_pOverlay->Rebuild(); }
}

void Application::OnSystemResume() {
    m_switcher->ReinstallHook();
    m_switcher->RefreshCOM();
    m_pTrayIcon->Reinitialize();
    SyncDesktopState();
}

int Application::Run() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
