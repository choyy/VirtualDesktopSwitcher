#pragma once

#include <windows.h>

#include <memory>

#include "core/IndicatorConfig.h"

constexpr UINT_PTR kTimerDesktopSync = 1;
constexpr UINT_PTR kTimerUpdatePoll  = 2;
constexpr UINT_PTR kDragDropDelay    = 4;

class DesktopIndicator;
class MouseFocus;
class TrayIcon;
class VirtualDesktopSwitcher;
class WindowDragHandler;

namespace SettingsDialog {
struct Result;
} // namespace SettingsDialog

class Application {
public:
    Application(const Application &)            = delete;
    Application &operator=(const Application &) = delete;
    Application(Application &&)                 = delete;
    Application &operator=(Application &&)      = delete;

    Application();
    ~Application();

    bool Initialize();

    static int Run();

private:
    // --- Static Callbacks ---
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static void CALLBACK    TimerPollUpdateProc(HWND hwnd, UINT uMsg, UINT_PTR id, DWORD dwTime);

    // --- Message Handlers ---
    void OnDestroy(HWND hwnd);
    void OnSystemResume();
    void OnDisplayChange();
    void OnTrayMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, HWND hwnd);
    void OnMenuSelect(WPARAM wParam, LPARAM lParam);
    void OnDesktopSwitch(WPARAM wParam);
    void OnTimerTick();

    // --- Internal Operations ---
    void SyncDesktopState();
    void ApplySettingsPreview(const SettingsDialog::Result &r);
    void PollUpdateProcess();
    void SpawnUpdateCheckProcess();

    // --- Initialization Sub-steps ---
    bool CreateHiddenWindow();
    void LoadConfiguration();
    void InitializeOverlay();
    bool InitializeTrayIcon();
    void SetupTrayCallbacks();

    // --- Member Variables ---
    HWND                                    m_hwnd = nullptr;
    std::unique_ptr<DesktopIndicator>       m_pOverlay;
    std::unique_ptr<MouseFocus>             m_mouseFocus;
    std::unique_ptr<TrayIcon>               m_pTrayIcon;
    std::unique_ptr<VirtualDesktopSwitcher> m_switcher;
    std::unique_ptr<WindowDragHandler>      m_dragHandler;
    IndicatorConfig                         m_indicatorCfg;
    int                                     m_lastDesktopIndex = -1;
    int                                     m_lastDesktopCount = 0;
    uint8_t                                 m_modMask          = 1;
    bool                                    m_pinByApp         = false;
    bool                                    m_autoCheckUpdates = true;
    HANDLE                                  m_hUpdateProcess   = nullptr;
    int                                     m_dragPendingSym   = -1;
    HWND                                    m_dragPendingHwnd  = nullptr;
};
