#include "settings.h"
#include <shlobj.h>
#include <sstream>
#include <cwctype>

namespace guard {

bool Contains(const std::vector<std::wstring>& values, const std::wstring& value) {
    for (const auto& candidate : values) if (_wcsicmp(candidate.c_str(), value.c_str()) == 0) return true;
    return false;
}

std::wstring ApplicationName(const std::wstring& path) {
    const auto separator = path.find_last_of(L"/\\");
    auto name = separator == std::wstring::npos ? path : path.substr(separator + 1);
    const auto first = name.find_first_not_of(L" \t\r\n\"");
    if (first == std::wstring::npos) return {};
    name = name.substr(first, name.find_last_not_of(L" \t\r\n\"") - first + 1);
    std::transform(name.begin(), name.end(), name.begin(), [](wchar_t character) { return static_cast<wchar_t>(towlower(character)); });
    return name;
}

bool ValidApplication(const std::wstring& name) {
    return name.size() > 4 && name.size() <= 260 && name.find_first_of(L"\\/:*?\"<>|\r\n=[]") == std::wstring::npos &&
           _wcsicmp(name.c_str() + name.size() - 4, L".exe") == 0;
}

bool MatchesApplication(const Settings& settings, const std::wstring& executable) {
    if (!settings.onlyListed && settings.applications.empty()) return true;
    if (executable.empty()) return false;
    const auto listed = Contains(settings.applications, ApplicationName(executable));
    return settings.onlyListed ? listed : !listed;
}

void ApplyDisplayRules(std::vector<Monitor>& monitors, const Settings& settings) {
    for (auto& monitor : monitors) {
        monitor.blocked = !monitor.identities.empty();
        for (const auto& identity : monitor.identities) {
            if (identity.empty() || !Contains(settings.blockedDisplays, identity)) monitor.blocked = false;
        }
    }
}

bool NeedsRescue(const RECT& rect, const std::vector<Monitor>& monitors, const Settings& settings) {
    if (!TouchesBlocked(rect, monitors)) return false;
    if (settings.partialOverlap) return true;
    for (const auto& monitor : monitors) if (!monitor.blocked && IntersectionArea(rect, monitor.bounds) > 0) return false;
    return true;
}

const Monitor* Destination(const RECT& rect, const std::vector<Monitor>& monitors, const Settings& settings) {
    if (!settings.destination.empty()) {
        for (const auto& monitor : monitors) {
            if (!monitor.blocked && Contains(monitor.identities, settings.destination)) return &monitor;
        }
    }
    return PickDestination(rect, monitors);
}

std::wstring SettingsPath() {
    PWSTR roaming = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming))) return {};
    const std::wstring path = std::wstring(roaming) + L"\\WindowGuard\\settings.ini";
    CoTaskMemFree(roaming);
    return path;
}

bool LoadSettings(const std::wstring& path, Settings& settings) {
    settings = Settings{};
    if (path.empty() || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    settings.intervalSeconds = std::clamp(GetPrivateProfileIntW(L"General", L"IntervalSeconds", 2, path.c_str()), 1U, 30U);
    settings.partialOverlap = GetPrivateProfileIntW(L"General", L"PartialOverlap", 1, path.c_str()) != 0;
    settings.resizeOversized = GetPrivateProfileIntW(L"General", L"ResizeOversized", 1, path.c_str()) != 0;
    settings.onlyListed = GetPrivateProfileIntW(L"Applications", L"OnlyListed", 0, path.c_str()) != 0;
    wchar_t buffer[4096]{};
    GetPrivateProfileStringW(L"Displays", L"Destination", L"", buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
    settings.destination = buffer;
    const auto readList = [&](const wchar_t* section, const wchar_t* countKey, std::vector<std::wstring>& output, bool applications) {
        const auto count = std::min(GetPrivateProfileIntW(section, countKey, 0, path.c_str()), MaxRules);
        for (unsigned index = 0; index < count; ++index) {
            const auto key = std::to_wstring(index);
            GetPrivateProfileStringW(section, key.c_str(), L"", buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
            const auto value = applications ? ApplicationName(buffer) : std::wstring(buffer);
            if (!value.empty() && (!applications || ValidApplication(value)) && !Contains(output, value)) output.push_back(value);
        }
    };
    readList(L"Displays", L"BlockedCount", settings.blockedDisplays, false);
    readList(L"Applications", L"Count", settings.applications, true);
    return true;
}

bool SaveSettings(const std::wstring& path, const Settings& settings, std::wstring& error) {
    if (path.empty()) { error = L"The settings location is unavailable."; return false; }
    if (settings.applications.size() > MaxRules || settings.blockedDisplays.size() > MaxRules) {
        error = L"Each rule list supports up to " + std::to_wstring(MaxRules) + L" entries. Remove unused rules and try again.";
        return false;
    }
    const auto separator = path.find_last_of(L"\\/");
    if (separator != std::wstring::npos) {
        const auto result = SHCreateDirectoryExW(nullptr, path.substr(0, separator).c_str(), nullptr);
        if (result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS && result != ERROR_FILE_EXISTS) {
            error = L"Could not create the settings folder.";
            return false;
        }
    }
    std::wostringstream content;
    content << L"\xFEFF[General]\r\nIntervalSeconds=" << settings.intervalSeconds << L"\r\nPartialOverlap=" << settings.partialOverlap
            << L"\r\nResizeOversized=" << settings.resizeOversized << L"\r\n\r\n[Displays]\r\nDestination=" << settings.destination
            << L"\r\nBlockedCount=" << settings.blockedDisplays.size() << L"\r\n";
    for (size_t index = 0; index < settings.blockedDisplays.size(); ++index) content << index << L"=" << settings.blockedDisplays[index] << L"\r\n";
    content << L"\r\n[Applications]\r\nOnlyListed=" << settings.onlyListed << L"\r\nCount=" << settings.applications.size() << L"\r\n";
    for (size_t index = 0; index < settings.applications.size(); ++index) content << index << L"=" << settings.applications[index] << L"\r\n";
    const auto text = content.str();
    const auto temporary = path + L".tmp." + std::to_wstring(GetCurrentProcessId());
    const auto file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { error = L"Could not write the settings file."; return false; }
    const auto bytes = static_cast<DWORD>(text.size() * sizeof(wchar_t));
    DWORD written = 0;
    const bool saved = WriteFile(file, text.data(), bytes, &written, nullptr) && written == bytes && FlushFileBuffers(file);
    CloseHandle(file);
    if (!saved || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        error = L"Could not replace the settings file. Your previous settings are unchanged.";
        return false;
    }
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, path.c_str());
    return true;
}

}
