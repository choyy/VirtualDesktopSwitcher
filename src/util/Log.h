#pragma once
#include <windows.h>

#include <string>

#include "util/ConfigIni.h"

inline void Log(const std::wstring &msg) {
    static HANDLE h = [] {
        std::wstring p = GetAppDataDir();
        if (p.empty()) { return INVALID_HANDLE_VALUE; }
        p += L"\\log.txt";

        bool   needsTruncate = false;
        HANDLE f             = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER size;
            needsTruncate = GetFileSizeEx(f, &size) && size.QuadPart > static_cast<LONGLONG>(1024 * 1024);
            CloseHandle(f);
        }

        return CreateFileW(p.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                           nullptr, needsTruncate ? CREATE_ALWAYS : OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    }();

    if (h == INVALID_HANDLE_VALUE) { return; }
    auto line = msg + L"\r\n";
    WriteFile(h, line.c_str(), static_cast<DWORD>(line.size() * sizeof(wchar_t)), nullptr, nullptr);
}