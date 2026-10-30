#include "guard.h"
#include <dwmapi.h>
#include <cwchar>

namespace guard {

struct DisplayName {
    std::wstring device;
    std::wstring name;
    std::wstring identity;
};

static std::vector<DisplayName> ReadDisplayNames() {
    std::vector<DisplayName> names;
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        UINT32 pathCount = 0;
        UINT32 modeCount = 0;
        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) break;
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        const auto result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
        if (result == ERROR_INSUFFICIENT_BUFFER) continue;
        if (result != ERROR_SUCCESS) break;
        for (UINT32 index = 0; index < pathCount; ++index) {
            const auto& path = paths[index];
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
            source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            source.header.size = sizeof(source);
            source.header.adapterId = path.sourceInfo.adapterId;
            source.header.id = path.sourceInfo.id;
            DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
            target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            target.header.size = sizeof(target);
            target.header.adapterId = path.targetInfo.adapterId;
            target.header.id = path.targetInfo.id;
            if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS) continue;
            const bool identified = DisplayConfigGetDeviceInfo(&target.header) == ERROR_SUCCESS;
            const std::wstring friendlyName = identified ? target.monitorFriendlyDeviceName : L"Unknown display";
            names.push_back({source.viewGdiDeviceName, friendlyName, identified ? target.monitorDevicePath : L""});
        }
        break;
    }
    return names;
}

static BOOL CALLBACK CollectMonitor(HMONITOR handle, HDC, LPRECT, LPARAM context) {
    auto& monitors = *reinterpret_cast<std::vector<Monitor>*>(context);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(handle, &info)) {
        monitors.push_back({handle, info.rcMonitor, info.rcWork, info.szDevice, L"Unknown display", false,
                            (info.dwFlags & MONITORINFOF_PRIMARY) != 0, {}});
    }
    return TRUE;
}

std::vector<Monitor> ReadMonitors() {
    const auto names = ReadDisplayNames();
    std::vector<Monitor> monitors;
    EnumDisplayMonitors(nullptr, nullptr, CollectMonitor, reinterpret_cast<LPARAM>(&monitors));
    for (auto& monitor : monitors) {
        monitor.name.clear();
        for (const auto& display : names) {
            if (display.device != monitor.device) continue;
            if (!monitor.name.empty()) monitor.name += L" + ";
            monitor.name += display.name;
            monitor.identities.push_back(display.identity);
        }
        if (monitor.name.empty()) monitor.name = L"Unknown display";
    }
    std::sort(monitors.begin(), monitors.end(), [](const Monitor& first, const Monitor& second) {
        const auto firstNumber = first.device.find(L"DISPLAY");
        const auto secondNumber = second.device.find(L"DISPLAY");
        if (firstNumber == std::wstring::npos || secondNumber == std::wstring::npos) return first.device < second.device;
        return wcstol(first.device.c_str() + firstNumber + 7, nullptr, 10) < wcstol(second.device.c_str() + secondNumber + 7, nullptr, 10);
    });
    return monitors;
}

std::wstring ProcessName(DWORD process) {
    const auto handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process);
    if (!handle) return {};
    std::wstring path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    const bool found = QueryFullProcessImageNameW(handle, 0, path.data(), &length) != FALSE;
    CloseHandle(handle);
    if (!found) return {};
    path.resize(length);
    return ApplicationName(path);
}

bool DisplayIdentitiesComplete(const std::vector<Monitor>& monitors) {
    if (monitors.empty()) return false;
    for (const auto& monitor : monitors) {
        if (monitor.identities.empty()) return false;
        for (const auto& identity : monitor.identities) if (identity.empty()) return false;
    }
    return true;
}

static BOOL CALLBACK CollectApplications(HWND window, LPARAM context) {
    if (!IsApplicationWindow(window)) return TRUE;
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    const auto name = ProcessName(process);
    auto& applications = *reinterpret_cast<std::vector<std::wstring>*>(context);
    if (ValidApplication(name) && !Contains(applications, name)) applications.push_back(name);
    return TRUE;
}

std::vector<std::wstring> RunningApplications() {
    std::vector<std::wstring> applications;
    EnumWindows(CollectApplications, reinterpret_cast<LPARAM>(&applications));
    std::sort(applications.begin(), applications.end());
    return applications;
}

bool WindowBounds(HWND window, RECT& outer, RECT& visible) {
    if (!GetWindowRect(window, &outer) || Width(outer) <= 0 || Height(outer) <= 0) return false;
    visible = outer;
    RECT frame{};
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame))) &&
        Width(frame) > 0 && Height(frame) > 0) visible = frame;
    return true;
}

bool IsApplicationWindow(HWND window) {
    if (!IsWindowVisible(window) || IsIconic(window) || window == GetShellWindow() || window == GetDesktopWindow()) return false;
    const auto style = GetWindowLongPtrW(window, GWL_STYLE);
    const auto extended = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if ((style & WS_CHILD) || (extended & WS_EX_NOACTIVATE)) return false;
    if (!(style & (WS_CAPTION | WS_THICKFRAME)) && !(extended & WS_EX_APPWINDOW) && GetWindowTextLengthW(window) == 0) return false;
    wchar_t className[128]{};
    GetClassNameW(window, className, static_cast<int>(std::size(className)));
    const wchar_t* excluded[] = {L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd", L"#32768", L"tooltips_class32"};
    for (const auto name : excluded) if (_wcsicmp(className, name) == 0) return false;
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) return false;
    GUITHREADINFO threadInfo{};
    threadInfo.cbSize = sizeof(threadInfo);
    if (GetGUIThreadInfo(GetWindowThreadProcessId(window, nullptr), &threadInfo) && (threadInfo.flags & GUI_INMOVESIZE)) return false;
    return true;
}

void WindowGuard::Refresh() {
    monitors = ReadMonitors();
    ApplyDisplayRules(monitors, settings);
    attempts.clear();
}

bool WindowGuard::Ready() const {
    bool blocked = false;
    bool allowed = false;
    for (const auto& monitor : monitors) {
        blocked |= monitor.blocked;
        allowed |= !monitor.blocked;
    }
    return blocked && allowed;
}

bool WindowGuard::Repair(HWND window) {
    if (paused || !Ready() || !IsApplicationWindow(window)) return false;
    RECT outer{};
    RECT visible{};
    if (!WindowBounds(window, outer, visible) || !NeedsRescue(visible, monitors, settings)) return false;
    const auto destination = Destination(visible, monitors, settings);
    if (!destination) return false;
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (settings.onlyListed || !settings.applications.empty()) {
        if (!MatchesApplication(settings, ProcessName(process))) return false;
    }
    const auto now = GetTickCount64();
    auto& attempt = attempts[window];
    attempt.seen = now;
    if (attempt.process != process || !EqualRect(&attempt.bounds, &visible)) {
        attempt = {process, visible, 0, 0, now};
    }
    if (now < attempt.next) return false;
    attempt.next = now + (++attempt.failures >= 3 ? 30000 : 1500);
    bool moved = false;
    if (IsZoomed(window)) {
        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);
        if (!GetWindowPlacement(window, &placement)) return false;
        RECT normal = placement.rcNormalPosition;
        const long width = std::max(100L, Width(normal));
        const long height = std::max(100L, Height(normal));
        normal = {destination->work.left, destination->work.top, destination->work.left + width, destination->work.top + height};
        normal = FitInside(normal, destination->work, settings.resizeOversized);
        placement.rcNormalPosition = ToWorkspace(normal, *destination, (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0);
        placement.flags = WPF_ASYNCWINDOWPLACEMENT;
        placement.showCmd = SW_SHOWMAXIMIZED;
        placement.ptMaxPosition = {-1, -1};
        const auto leftMargin = visible.left - outer.left;
        const auto topMargin = visible.top - outer.top;
        const auto rightMargin = outer.right - visible.right;
        const auto bottomMargin = outer.bottom - visible.bottom;
        moved = SetWindowPos(window, nullptr, destination->work.left - leftMargin, destination->work.top - topMargin,
            Width(destination->work) + leftMargin + rightMargin, Height(destination->work) + topMargin + bottomMargin,
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_ASYNCWINDOWPOS) != FALSE;
        if (moved) SetWindowPlacement(window, &placement);
    } else {
        const bool resizable = settings.resizeOversized && (GetWindowLongPtrW(window, GWL_STYLE) & WS_THICKFRAME) != 0;
        const auto fitted = FitInside(visible, destination->work, resizable);
        const auto leftMargin = visible.left - outer.left;
        const auto topMargin = visible.top - outer.top;
        const auto rightMargin = outer.right - visible.right;
        const auto bottomMargin = outer.bottom - visible.bottom;
        moved = SetWindowPos(window, nullptr, fitted.left - leftMargin, fitted.top - topMargin,
            Width(fitted) + leftMargin + rightMargin, Height(fitted) + topMargin + bottomMargin,
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_ASYNCWINDOWPOS) != FALSE;
    }
    return moved;
}

static BOOL CALLBACK RepairCallback(HWND window, LPARAM context) {
    reinterpret_cast<WindowGuard*>(context)->Repair(window);
    return TRUE;
}

void WindowGuard::Scan() {
    if (paused || !Ready() || (GetAsyncKeyState(VK_LBUTTON) & 0x8000) || (GetAsyncKeyState(VK_RBUTTON) & 0x8000)) return;
    EnumWindows(RepairCallback, reinterpret_cast<LPARAM>(this));
    const auto now = GetTickCount64();
    for (auto entry = attempts.begin(); entry != attempts.end();) {
        if (now - entry->second.seen > 60000 || !IsWindow(entry->first)) entry = attempts.erase(entry);
        else ++entry;
    }
}

}
