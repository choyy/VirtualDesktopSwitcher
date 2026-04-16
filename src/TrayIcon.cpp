#include "TrayIcon.h"

#include <array>

#include <shlwapi.h>

TrayIcon::~TrayIcon() {
    if (m_nid.hWnd != nullptr) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
    }
    if (m_hMenu != nullptr) {
        DestroyMenu(m_hMenu);
    }
}

bool TrayIcon::Initialize(HWND hwnd, HINSTANCE hInstance) {
    // 获取当前可执行文件路径
    std::array<wchar_t, MAX_PATH> exePath{};
    GetModuleFileNameW(nullptr, exePath.data(), MAX_PATH);

    // 获取图标
    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(101));
    if (hIcon == nullptr) {
        // 如果没有资源图标，使用系统图标
        hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    }

    // 初始化NOTIFYICONDATA结构
    m_nid.cbSize           = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd             = hwnd;
    m_nid.uID              = 1;
    m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon            = hIcon;

    // 设置提示文本
    UpdateTooltip(L"Virtual Desktop Switcher");

    // 添加托盘图标
    if (Shell_NotifyIconW(NIM_ADD, &m_nid) == FALSE) {
        return false;
    }

    // 创建右键菜单
    m_hMenu = CreatePopupMenu();
    if (m_hMenu == nullptr) {
        return false;
    }

    // 获取当前自动启动状态
    m_autoStartEnabled = IsAutoStartEnabled();

    // 添加菜单项
    AppendMenuW(m_hMenu, MF_STRING, WM_TRAY_TOGGLE_AUTOSTART,
                m_autoStartEnabled ? L"关闭开机自启" : L"开机自启");
    AppendMenuW(m_hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_hMenu, MF_STRING, WM_TRAY_EXIT, L"退出");

    return true;
}

void TrayIcon::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP) {
            // 显示右键菜单
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);

            // 更新自启动菜单项文本
            m_autoStartEnabled = IsAutoStartEnabled();
            ModifyMenuW(m_hMenu, 0, MF_BYPOSITION | MF_STRING, WM_TRAY_TOGGLE_AUTOSTART,
                        m_autoStartEnabled ? L"关闭开机自启" : L"开机自启");

            // 显示菜单
            TrackPopupMenu(m_hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
        } else if (lParam == WM_LBUTTONDBLCLK) {
            // 双击托盘图标切换开机自启
            PostMessage(hwnd, WM_COMMAND, WM_TRAY_TOGGLE_AUTOSTART, 0);
        }
    } else if (msg == WM_COMMAND) {
        if (LOWORD(wParam) == WM_TRAY_EXIT) {
            PostQuitMessage(0);
        } else if (LOWORD(wParam) == WM_TRAY_TOGGLE_AUTOSTART) {
            // 切换自启动状态
            SetAutoStart(!IsAutoStartEnabled());

            // 重新读取实际状态并更新菜单项文本
            m_autoStartEnabled = IsAutoStartEnabled();
            ModifyMenuW(m_hMenu, 0, MF_BYPOSITION | MF_STRING, WM_TRAY_TOGGLE_AUTOSTART,
                        m_autoStartEnabled ? L"关闭开机自启" : L"开机自启");
        }
    }
}

void TrayIcon::UpdateTooltip(const std::wstring &tooltip) {
    wcsncpy_s(m_nid.szTip, tooltip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

bool TrayIcon::IsAutoStartEnabled() {
    HKEY hKey   = nullptr;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
                                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    std::array<wchar_t, MAX_PATH> exePath{};
    GetModuleFileNameW(nullptr, exePath.data(), MAX_PATH);

    std::array<wchar_t, MAX_PATH> value{};
    auto                          valueSize = static_cast<DWORD>(sizeof(value));
    result                                  = RegQueryValueExW(hKey, L"VirtualDesktopSwitcher", nullptr, nullptr,
                                                               reinterpret_cast<LPBYTE>(value.data()), &valueSize); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        return false;
    }

    // 处理注册表中可能包含双引号的路径
    std::wstring registryPath(value.data());
    std::wstring currentPath(exePath.data());
    
    // 如果注册表路径以双引号开头和结尾，去除它们
    if (registryPath.length() >= 2 && 
        registryPath.front() == L'"' && 
        registryPath.back() == L'"') {
        registryPath = registryPath.substr(1, registryPath.length() - 2);
    }
    
    return _wcsicmp(registryPath.c_str(), currentPath.c_str()) == 0;
}

void TrayIcon::SetAutoStart(bool enable) {
    HKEY       hKey   = nullptr;
    const LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
                                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                      0, KEY_WRITE, &hKey);
    if (result != ERROR_SUCCESS) {
        return;
    }

    if (enable) {
        std::array<wchar_t, MAX_PATH> exePath{};
        GetModuleFileNameW(nullptr, exePath.data(), MAX_PATH);
        // 标准化路径并用引号包裹（处理含空格的路径）
        std::wstring quotedPath = L"\"" + std::wstring(exePath.data()) + L"\"";
        RegSetValueExW(hKey, L"VirtualDesktopSwitcher", 0, REG_SZ,
                       reinterpret_cast<const BYTE *>(quotedPath.c_str()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                       static_cast<DWORD>((quotedPath.length() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, L"VirtualDesktopSwitcher");
    }

    RegCloseKey(hKey);
}