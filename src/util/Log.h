#pragma once
#include <windows.h>

#include <string>

#include "util/ConfigIni.h"

inline HANDLE InitLogFile() {
    std::wstring p = GetAppDataDir();
    if (p.empty()) { return INVALID_HANDLE_VALUE; }
    p += L"\\log.txt";

    bool   needsTruncate = false;
    HANDLE f             = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size;
        needsTruncate = (GetFileSizeEx(f, &size) != 0) && size.QuadPart > static_cast<LONGLONG>(32 * 1024);
        CloseHandle(f);
    }

    return CreateFileW(p.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                       nullptr, needsTruncate ? CREATE_ALWAYS : OPEN_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
}

inline void Log(const std::wstring &msg) {
    static HANDLE h = InitLogFile();

    if (h == INVALID_HANDLE_VALUE) { return; }
    auto wide = msg + L"\r\n";
    int  len  = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len > 0) {
        std::string utf8(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), len, nullptr, nullptr);
        WriteFile(h, utf8.data(), static_cast<DWORD>(utf8.size()), nullptr, nullptr);
    }
}
