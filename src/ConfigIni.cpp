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
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, GetConfigPath().c_str());
}

void WriteIniInt(const std::wstring& section, const std::wstring& key, int value) {
    wchar_t buf[32];
    swprintf_s(buf, L"%d", value);
    WritePrivateProfileStringW(section.c_str(), key.c_str(), buf, GetConfigPath().c_str());
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, GetConfigPath().c_str());
}

std::wstring EncodeSymbol(const std::wstring& sym) {
    if (sym.empty()) return L"";
    wchar_t buf[16];
    swprintf_s(buf, L"\\u%04X", (unsigned)sym[0]);
    return buf;
}

std::wstring DecodeSymbol(const std::wstring& str) {
    if (str.empty()) return L"";
    if (str[0] == L'\\' && str.size() >= 6 && str[1] == L'u') {
        unsigned code = 0;
        for (size_t i = 2; i < 6; ++i) {
            wchar_t c = str[i];
            code = (code << 4) + (c >= L'a' ? (c - L'a' + 10) : (c >= L'A' ? (c - L'A' + 10) : (c - L'0')));
        }
        if (code > 0) {
            wchar_t ch = (wchar_t)code;
            return std::wstring(1, ch);
        }
    }
    return str;
}
