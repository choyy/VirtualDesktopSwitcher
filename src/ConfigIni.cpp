// Adapted from Catime (Apache 2.0) - config_ini.c
// Original: https://github.com/vladelaina/Catime
#include "ConfigIni.h"
#include <fstream>
#include <sstream>
#include <shlobj.h>
#include <unordered_map>

static std::wstring s_configPath;

std::wstring GetConfigPath() {
    if (s_configPath.empty()) {
        wchar_t appData[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
            s_configPath = std::wstring(appData) + L"\\VirtualDesktopSwitcher";
            CreateDirectoryW(s_configPath.c_str(), nullptr);
            s_configPath += L"\\settings.ini";
        }
    }
    return s_configPath;
}

std::wstring ReadIniString(const std::wstring& section, const std::wstring& key, const std::wstring& defaultVal) {
    wchar_t buf[1024];
    DWORD ret = GetPrivateProfileStringW(section.c_str(), key.c_str(), defaultVal.c_str(), buf, 1024, GetConfigPath().c_str());
    return std::wstring(buf, ret);
}

int ReadIniInt(const std::wstring& section, const std::wstring& key, int defaultVal) {
    return GetPrivateProfileIntW(section.c_str(), key.c_str(), defaultVal, GetConfigPath().c_str());
}

void WriteIniString(const std::wstring& section, const std::wstring& key, const std::wstring& value) {
    WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), GetConfigPath().c_str());
}

void WriteIniInt(const std::wstring& section, const std::wstring& key, int value) {
    wchar_t buf[32];
    swprintf_s(buf, L"%d", value);
    WritePrivateProfileStringW(section.c_str(), key.c_str(), buf, GetConfigPath().c_str());
}
