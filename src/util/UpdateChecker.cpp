#include "UpdateChecker.h"

#include <array>
#include <string>

#include "config.h"
#include "util/Lang.h"

namespace {

bool DownloadFile(const std::wstring &url, const std::wstring &dest) {
    using URLDownloadToFileW_t = HRESULT(WINAPI *)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPUNKNOWN);
    HMODULE hUrlmon            = LoadLibraryW(L"urlmon.dll");
    if (hUrlmon == nullptr) { return false; }

    auto pDownload = reinterpret_cast<URLDownloadToFileW_t>(GetProcAddress(hUrlmon, "URLDownloadToFileW")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (pDownload == nullptr) {
        FreeLibrary(hUrlmon);
        return false;
    }

    HRESULT hr = pDownload(nullptr, url.c_str(), dest.c_str(), 0, nullptr);
    FreeLibrary(hUrlmon);
    return SUCCEEDED(hr);
}

void ShowDownloadFailedDialog(HWND parent) {
    if (MessageBoxW(parent, Lang::Get(L"Download.FailedMsg"), Lang::Get(L"Download.FailedTitle"), MB_YESNO | MB_ICONERROR) == IDYES) {
        ShellExecuteW(parent, L"open", L"https://github.com/choyy/VirtualDesktopSwitcher/releases", nullptr, nullptr, SW_SHOW);
    }
}

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
