#pragma once

#include <windows.h>

#include <string>

namespace UpdateChecker {

bool CheckForNewerVersion();
bool ParseVersionJson(const std::wstring &filePath);
void DownloadUpdate(HWND parent);
void CheckAndDownload(HWND parent, bool silentIfUpToDate = false);

} // namespace UpdateChecker
