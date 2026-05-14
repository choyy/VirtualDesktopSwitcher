#include "UpdateChecker.h"

#include <array>
#include <string>

#include "util/Lang.h"
#include "util/Utils.h"

namespace {

std::string ExtractTag(const std::string &json) {
    auto key = json.find("\"tag_name\"");
    if (key == std::string::npos) { return {}; }
    auto val = json.find('"', key + 11);
    if (val == std::string::npos) { return {}; }
    auto end = json.find('"', val + 1);
    if (end == std::string::npos) { return {}; }
    auto tag = json.substr(val + 1, end - val - 1);
    if (tag.empty()) { return {}; }
    if (tag[0] == 'v') { tag = tag.substr(1); }
    return tag;
}

} // namespace

bool UpdateChecker::ParseVersionJson(const std::wstring &path) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) { return false; }

    DWORD       size = GetFileSize(hFile, nullptr);
    std::string json;
    if (size > 0) {
        json.resize(size);
        DWORD read = 0;
        ReadFile(hFile, json.data(), size, &read, nullptr);
        json.resize(read);
    }
    CloseHandle(hFile);

    auto tag = ExtractTag(json);
    return !tag.empty() && IsNewerVersion(tag, APP_VERSION);
}

bool UpdateChecker::CheckForNewerVersion() {
    std::array<wchar_t, MAX_PATH> buf{};
    if (GetTempPathW(static_cast<DWORD>(buf.size()), buf.data()) == 0) { return false; }

    std::wstring dest = std::wstring(buf.data()) + L"vds_version.json";
    if (!DownloadFile(L"https://api.github.com/repos/choyy/VirtualDesktopSwitcher/releases/latest", dest)) {
        return false;
    }

    auto result = ParseVersionJson(dest);
    DeleteFileW(dest.c_str());
    return result;
}

void UpdateChecker::DownloadUpdate(HWND parent) {
    std::array<wchar_t, MAX_PATH> userProfile{};
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile.data(), static_cast<DWORD>(userProfile.size())) == 0) {
        ShowDownloadFailedDialog(parent);
        return;
    }

    std::wstring dest = std::wstring(userProfile.data()) + L"\\Desktop\\VirtualDesktopSwitcher.exe";

    if (DownloadFile(L"https://github.com/choyy/VirtualDesktopSwitcher/releases/latest/download/VirtualDesktopSwitcher.exe", dest)) {
        MessageBoxW(parent, (std::wstring(Lang::Get(L"Update.DownloadedMsg")) + dest).c_str(), Lang::Get(L"Update.DownloadedTitle"), MB_OK | MB_ICONINFORMATION);
    } else {
        ShowDownloadFailedDialog(parent);
    }
}

void UpdateChecker::CheckAndDownload(HWND parent, bool silentIfUpToDate) {
    if (CheckForNewerVersion()) {
        if (MessageBoxW(parent, Lang::Get(L"Update.FoundMsg"), Lang::Get(L"Update.FoundTitle"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
            DownloadUpdate(parent);
        }
    } else if (!silentIfUpToDate) {
        MessageBoxW(parent, Lang::Get(L"Update.NoUpdateMsg"), Lang::Get(L"Update.NoUpdateTitle"), MB_OK | MB_ICONINFORMATION);
    }
}
