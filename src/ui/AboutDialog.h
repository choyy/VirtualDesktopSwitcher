#pragma once

#include <windows.h>

#include <string>

namespace AboutDialog {

struct Result {
    bool autoCheckUpdates = false;
    bool accepted         = false;
};

struct VersionCheckResult {
    bool        hasUpdate = false;
    std::string downloadUrl;
};

VersionCheckResult CheckForNewerVersion();
Result             Show(HWND parent, bool currentAutoCheck);
void               DownloadUpdate(HWND parent, const std::string &url);
void               CheckAndDownload(HWND parent, bool silentIfUpToDate = false);

} // namespace AboutDialog
