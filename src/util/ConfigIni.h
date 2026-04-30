// Adapted from Catime (Apache 2.0) - config_ini.h
// Original: https://github.com/vladelaina/Catime
#ifndef CONFIG_INI_H
#define CONFIG_INI_H

#include <windows.h>

#include <string>

std::wstring GetAppDataDir();
std::wstring ReadIniString(const std::wstring &section, const std::wstring &key, const std::wstring &defaultVal);
int          ReadIniInt(const std::wstring &section, const std::wstring &key, int defaultVal);
void         WriteIniString(const std::wstring &section, const std::wstring &key, const std::wstring &value);
void         WriteIniInt(const std::wstring &section, const std::wstring &key, int value);
std::wstring EncodeSymbol(const std::wstring &sym);
std::wstring DecodeSymbol(const std::wstring &str);

#endif
