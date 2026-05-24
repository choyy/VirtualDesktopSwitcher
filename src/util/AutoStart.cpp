#include "AutoStart.h"

#include <windows.h>

#include <array>
#include <string>

#include "util/Utils.h"

namespace {

constexpr auto kRegRunPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool RunSchtasks(const wchar_t *args) {
    std::array<wchar_t, MAX_PATH> sys32{};
    GetSystemDirectoryW(sys32.data(), static_cast<UINT>(sys32.size()));
    std::wstring        cmd = std::wstring(sys32.data()) + L"\\schtasks.exe " + args;
    PROCESS_INFORMATION pi{};
    STARTUPINFOW        si = {.cb = sizeof(si)};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)
        == 0) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}

void DeleteRunReg() {
    HKEY hKey{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegRunPath, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"VirtualDesktopSwitcher");
        RegCloseKey(hKey);
    }
}

} // namespace

bool AutoStart::IsEnabled() {
    if (RunSchtasks(L"/query /tn \"VirtualDesktopSwitcher\"")) {
        return true;
    }
    HKEY hKey   = nullptr;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, kRegRunPath, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) { return false; }
    std::wstring                  currentPath = GetCurrentExePath();
    std::array<wchar_t, MAX_PATH> value{};
    auto                          valueSize = static_cast<DWORD>(sizeof(value));
    result                                  = RegQueryValueExW(hKey, L"VirtualDesktopSwitcher", nullptr, nullptr,
                                                               reinterpret_cast<LPBYTE>(value.data()), &valueSize); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    RegCloseKey(hKey);
    if (result != ERROR_SUCCESS) { return false; }
    std::wstring registryPath(value.data());
    if (registryPath.length() >= 2 && registryPath.front() == L'"' && registryPath.back() == L'"') {
        registryPath = registryPath.substr(1, registryPath.length() - 2);
    }
    return _wcsicmp(registryPath.c_str(), currentPath.c_str()) == 0;
}

void AutoStart::SetEnabled(bool enable) {
    if (IsAdminProcess()) {
        if (enable) {
            std::wstring exePath = GetCurrentExePath();
            std::wstring args    = L"/create /tn \"VirtualDesktopSwitcher\" /tr \\\"";
            args += exePath;
            args += L"\\\" /sc onlogon /delay 0000:10 /rl highest /f";
            RunSchtasks(args.c_str());
            DeleteRunReg();
        } else {
            RunSchtasks(L"/delete /tn \"VirtualDesktopSwitcher\" /f");
            DeleteRunReg();
        }
        return;
    }
    HKEY       hKey   = nullptr;
    const LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, kRegRunPath, 0, KEY_WRITE, &hKey);
    if (result != ERROR_SUCCESS) { return; }
    if (enable) {
        std::wstring       exePath    = GetCurrentExePath();
        const std::wstring quotedPath = L"\"" + exePath + L"\"";
        RegSetValueExW(hKey, L"VirtualDesktopSwitcher", 0, REG_SZ,
                       reinterpret_cast<const BYTE *>(quotedPath.c_str()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                       static_cast<DWORD>((quotedPath.length() + 1) * sizeof(wchar_t)));
    } else {
        DeleteRunReg();
    }
    RegCloseKey(hKey);
}

void AutoStart::RemoveAll() {
    DeleteRunReg();
    RunSchtasks(L"/delete /tn \"VirtualDesktopSwitcher\" /f");
}

void AutoStart::RemoveTask() {
    RunSchtasks(L"/delete /tn \"VirtualDesktopSwitcher\" /f");
}
