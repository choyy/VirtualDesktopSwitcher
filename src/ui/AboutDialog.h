#ifndef ABOUT_DIALOG_H
#define ABOUT_DIALOG_H

#include <windows.h>

#include <string>

class AboutDialog {
public:
    struct Result {
        bool autoCheckUpdates = false;
        bool accepted         = false;
    };

    struct VersionCheckResult {
        bool        hasUpdate = false;
        std::string downloadUrl;
    };

    static VersionCheckResult CheckForNewerVersion();
    static Result             Show(HWND parent, bool currentAutoCheck);
    static void               DownloadUpdate(HWND parent, const std::string &url);
};

#endif
