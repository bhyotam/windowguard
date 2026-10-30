#pragma once

#include "geometry.h"

namespace guard {

constexpr unsigned MaxRules = 256;

struct Settings {
    std::vector<std::wstring> blockedDisplays;
    std::wstring destination;
    std::vector<std::wstring> applications;
    bool onlyListed = false;
    bool partialOverlap = true;
    bool resizeOversized = true;
    unsigned intervalSeconds = 2;
};

bool Contains(const std::vector<std::wstring>& values, const std::wstring& value);
std::wstring ApplicationName(const std::wstring& path);
bool ValidApplication(const std::wstring& name);
bool MatchesApplication(const Settings& settings, const std::wstring& executable);
void ApplyDisplayRules(std::vector<Monitor>& monitors, const Settings& settings);
bool NeedsRescue(const RECT& rect, const std::vector<Monitor>& monitors, const Settings& settings);
const Monitor* Destination(const RECT& rect, const std::vector<Monitor>& monitors, const Settings& settings);
std::wstring SettingsPath();
bool LoadSettings(const std::wstring& path, Settings& settings);
bool SaveSettings(const std::wstring& path, const Settings& settings, std::wstring& error);

}
