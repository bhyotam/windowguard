#pragma once

#include "geometry.h"
#include "settings.h"
#include <map>

namespace guard {

std::vector<Monitor> ReadMonitors();
bool DisplayIdentitiesComplete(const std::vector<Monitor>& monitors);
bool WindowBounds(HWND window, RECT& outer, RECT& visible);
bool IsApplicationWindow(HWND window);
std::wstring ProcessName(DWORD process);
std::vector<std::wstring> RunningApplications();

class WindowGuard {
public:
    std::vector<Monitor> monitors;
    bool paused = false;
    Settings settings;
    void Refresh();
    bool Ready() const;
    bool Repair(HWND window);
    void Scan();

private:
    struct Attempt {
        DWORD process = 0;
        RECT bounds{};
        unsigned failures = 0;
        ULONGLONG next = 0;
        ULONGLONG seen = 0;
    };
    std::map<HWND, Attempt> attempts;
};

}
