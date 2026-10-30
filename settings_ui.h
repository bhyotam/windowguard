#pragma once

#include "guard.h"
#include <functional>

namespace guard {

using ApplySettings = std::function<bool(const Settings&, bool, std::wstring&)>;
void ShowSettings(HINSTANCE instance, HWND owner, HICON icon, const Settings& settings, bool startup,
                  const std::wstring& path, ApplySettings apply);
bool SettingsMessage(MSG& message);
void CloseSettings();

}
