// Adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#include "ConfigIni.h"

#include <shlobj.h>

#include <array>

namespace {
std::wstring GetOrCreateAppDataDir() {
    static std::wstring path = []() {
        std::array<wchar_t, MAX_PATH> appData{};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData.data()))) {
            std::wstring p = std::wstring(appData.data()) + L"\\VirtualDesktopSwitcher";
            CreateDirectoryW(p.c_str(), nullptr);
            return p;
        }
        return std::wstring();
    }();
    return path;
}

const std::wstring &GetSettingsPath() {
    static std::wstring path = GetOrCreateAppDataDir() + L"\\settings.ini";
    return path;
}
} // namespace

std::wstring GetAppDataDir() {
    return GetOrCreateAppDataDir();
}

std::wstring ReadIniString(const std::wstring &section, const std::wstring &key,
                           const std::wstring &defaultVal) {
    std::array<wchar_t, 1024> buf{};
    const std::wstring       &path = GetSettingsPath();
    DWORD                     ret  = GetPrivateProfileStringW(section.c_str(), key.c_str(),
                                                              defaultVal.c_str(), buf.data(), static_cast<DWORD>(buf.size()),
                                                              path.c_str());
    return {buf.data(), ret};
}

int ReadIniInt(const std::wstring &section, const std::wstring &key, int defaultVal) {
    const std::wstring &path = GetSettingsPath();
    return static_cast<int>(GetPrivateProfileIntW(section.c_str(), key.c_str(),
                                                  defaultVal,
                                                  path.c_str()));
}

void WriteIniString(const std::wstring &section, const std::wstring &key,
                    const std::wstring &value) {
    const std::wstring &path = GetSettingsPath();
    WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), path.c_str());
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, path.c_str());
}

void WriteIniInt(const std::wstring &section, const std::wstring &key, int value) {
    std::wstring        valueStr = std::to_wstring(value);
    const std::wstring &path     = GetSettingsPath();
    WritePrivateProfileStringW(section.c_str(), key.c_str(), valueStr.c_str(), path.c_str());
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, path.c_str());
}

std::wstring EncodeSymbol(const std::wstring &sym) {
    if (sym.empty()) {
        return {};
    }
    std::array<wchar_t, 8> buf{};
    swprintf_s(buf.data(), buf.size(), L"\\u%04X", static_cast<unsigned>(sym[0])); // NOLINT
    return buf.data();
}

std::wstring DecodeSymbol(const std::wstring &str) {
    if (str.empty() || str[0] != L'\\' || str.size() < 6 || str[1] != L'u') {
        return str;
    }

    unsigned code = 0;
    for (size_t i = 2; i < 6; ++i) {
        wchar_t c = str[i];
        if (c >= L'a') {
            code = (code << 4u) + (c - L'a' + 10);
        } else if (c >= L'A') {
            code = (code << 4u) + (c - L'A' + 10);
        } else {
            code = (code << 4u) + (c - L'0');
        }
    }
    if (code > 0) {
        return {1, static_cast<wchar_t>(code)};
    }
    return str;
}

std::wstring ReadIniSymbol(const std::wstring &section, const std::wstring &key, const std::wstring &defaultSym) {
    std::wstring decoded = DecodeSymbol(ReadIniString(section, key, EncodeSymbol(defaultSym)));
    return (decoded.size() == 1) ? decoded : defaultSym;
}
