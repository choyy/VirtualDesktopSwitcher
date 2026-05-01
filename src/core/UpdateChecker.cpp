#include "UpdateChecker.h"

#include <array>
#include <cstdlib>
#include <string>

#include "util/Utils.h"

namespace {

bool IsNewerVersion(const std::string &remote, const std::string &local) {
    size_t r_pos = 0, l_pos = 0;
    while (r_pos < remote.size() && l_pos < local.size()) {
        char   *re = nullptr, *le = nullptr;
        int32_t rn = strtol(remote.c_str() + r_pos, &re, 10);
        int32_t ln = strtol(local.c_str() + l_pos, &le, 10);

        if (rn > ln) { return true; }
        if (rn < ln) { return false; }

        r_pos = re - remote.c_str();
        l_pos = le - local.c_str();

        if (r_pos < remote.size() && remote[r_pos] == '.') { r_pos++; }
        if (l_pos < local.size() && local[l_pos] == '.') { l_pos++; }
    }
    return r_pos < remote.size();
}

} // namespace

UpdateChecker::VersionCheckResult UpdateChecker::CheckForNewerVersion() {
    VersionCheckResult            result;
    std::array<wchar_t, MAX_PATH> tempPath{};
    if (GetTempPathW(static_cast<DWORD>(tempPath.size()), tempPath.data()) == 0) {
        return result;
    }

    std::wstring dest = std::wstring(tempPath.data()) + L"vds_version.json";

    if (!DownloadFile(L"https://api.github.com/repos/choyy/VirtualDesktopSwitcher/releases/latest", dest)) {
        return result;
    }

    HANDLE hFile = CreateFileW(dest.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        DeleteFileW(dest.c_str());
        return result;
    }

    DWORD       fileSize = GetFileSize(hFile, nullptr);
    std::string content;
    if (fileSize > 0) {
        content.resize(fileSize);
        DWORD bytesRead = 0;
        ReadFile(hFile, content.data(), fileSize, &bytesRead, nullptr);
        content.resize(bytesRead);
    }
    CloseHandle(hFile);
    DeleteFileW(dest.c_str());

    auto extractJsonString = [&content](const std::string &key) -> std::string {
        auto pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) { return {}; }
        pos = content.find('"', pos + key.size() + 3);
        if (pos == std::string::npos) { return {}; }
        auto start = pos + 1;
        auto end   = content.find('"', start);
        if (end == std::string::npos) { return {}; }
        return content.substr(start, end - start);
    };

    std::string latestTag = extractJsonString("tag_name");
    if (latestTag.empty()) { return result; }
    if (latestTag[0] == 'v') { latestTag = latestTag.substr(1); }

    result.downloadUrl = extractJsonString("browser_download_url");
    result.hasUpdate   = IsNewerVersion(latestTag, APP_VERSION);
    return result;
}

void UpdateChecker::DownloadUpdate(HWND parent, const std::string &url) {
    std::wstring                  wideUrl = Utf8ToWide(url);
    std::array<wchar_t, MAX_PATH> userProfile{};
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile.data(), static_cast<DWORD>(userProfile.size())) == 0) {
        ShowDownloadFailedDialog(parent);
        return;
    }

    std::wstring dest = std::wstring(userProfile.data()) + L"\\Desktop\\VirtualDesktopSwitcher.exe";

    if (DownloadFile(wideUrl, dest)) {
        MessageBoxW(parent, (L"已下载到：\n" + dest).c_str(), L"下载完成", MB_OK | MB_ICONINFORMATION);
    } else {
        ShowDownloadFailedDialog(parent);
    }
}

void UpdateChecker::CheckAndDownload(HWND parent, bool silentIfUpToDate) {
    auto result = CheckForNewerVersion();
    if (result.hasUpdate) {
        if (MessageBoxW(parent, L"发现新版本，是否立即下载？", L"发现更新", MB_YESNO | MB_ICONQUESTION) == IDYES && !result.downloadUrl.empty()) {
            DownloadUpdate(parent, result.downloadUrl);
        }
    } else if (!silentIfUpToDate) {
        MessageBoxW(parent, L"您已使用最新版本。", L"无需更新", MB_OK | MB_ICONINFORMATION);
    }
}
