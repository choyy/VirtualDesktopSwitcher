#pragma once

#include <windows.h>

#include <string>

namespace UpdateChecker {

struct VersionCheckResult {
    bool        hasUpdate = false;
    std::string downloadUrl;
};

VersionCheckResult CheckForNewerVersion();
void               DownloadUpdate(HWND parent, const std::string &url);
void               CheckAndDownload(HWND parent, bool silentIfUpToDate = false);

} // namespace UpdateChecker
