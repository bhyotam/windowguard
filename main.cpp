#include "guard.h"
#include "settings_ui.h"
#include <commctrl.h>
#include <shellapi.h>
#include <fstream>
#include <strsafe.h>

namespace {

constexpr wchar_t WindowClass[] = L"WindowGuard.Control.2";
constexpr wchar_t RunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t RunValue[] = L"WindowGuard";
constexpr UINT TrayMessage = WM_APP + 1;
constexpr UINT StopMessage = WM_APP + 2;
constexpr UINT ShowMessage = WM_APP + 3;
constexpr UINT ScanTimer = 1;
constexpr UINT RefreshTimer = 2;
constexpr UINT TrayRetryTimer = 3;
constexpr UINT MenuPause = 101;
constexpr UINT MenuRescue = 102;
constexpr UINT MenuSettings = 104;
constexpr UINT MenuExit = 105;
guard::WindowGuard windowGuard;
NOTIFYICONDATAW tray{};
UINT taskbarCreated = 0;
HICON icon = nullptr;
HINSTANCE appInstance = nullptr;
std::wstring settingsPath;
bool trayAdded = false;

std::wstring ExecutablePath() {
    std::wstring path(32768, L'\0');
    path.resize(GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size())));
    return path;
}

std::wstring StartupCommand() {
    return L"\"" + ExecutablePath() + L"\" --background --config \"" + settingsPath + L"\"";
}

bool StartsWithWindows() {
    wchar_t value[32768]{};
    DWORD bytes = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, RunKey, RunValue, RRF_RT_REG_SZ, nullptr, value, &bytes) != ERROR_SUCCESS) return false;
    return std::wstring(value) == StartupCommand();
}

bool SetStartup(bool enabled, std::wstring& error) {
    HKEY key = nullptr;
    auto result = RegCreateKeyExW(HKEY_CURRENT_USER, RunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (result == ERROR_SUCCESS) {
        if (!enabled) {
            result = RegDeleteValueW(key, RunValue);
            if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
        }
        else {
            const auto command = StartupCommand();
            result = RegSetValueExW(key, RunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
                                  static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        }
        RegCloseKey(key);
    }
    if (result != ERROR_SUCCESS) error = L"Windows could not update the startup setting.";
    return result == ERROR_SUCCESS;
}

HICON CreateGuardIcon() {
    DWORD pixels[32 * 32]{};
    for (int row = 4; row < 25; ++row) {
        for (int column = 2; column < 30; ++column) {
            const bool edge = row < 7 || row >= 22 || column < 5 || column >= 27;
            pixels[row * 32 + column] = edge ? 0xFF55C9FF : 0xFF16324B;
        }
    }
    for (int row = 10; row <= 18; ++row) {
        for (int column = 9; column <= 21; ++column) {
            if ((row >= 13 && row <= 15) || column <= 13 - abs(row - 14)) pixels[row * 32 + column] = 0xFF78F0B0;
        }
    }
    for (int row = 25; row < 29; ++row) for (int column = 12; column < 20; ++column) pixels[row * 32 + column] = 0xFF55C9FF;
    const auto color = CreateBitmap(32, 32, 1, 32, pixels);
    BYTE maskBits[128]{};
    const auto mask = CreateBitmap(32, 32, 1, 1, maskBits);
    ICONINFO info{};
    info.fIcon = TRUE;
    info.hbmColor = color;
    info.hbmMask = mask;
    const auto result = CreateIconIndirect(&info);
    DeleteObject(color);
    DeleteObject(mask);
    return result;
}

std::wstring Status() {
    if (windowGuard.paused) return L"Paused";
    if (windowGuard.settings.blockedDisplays.empty()) return L"No blocked displays selected";
    if (!windowGuard.Ready()) return L"Waiting for your display layout";
    return L"Active - every " + std::to_wstring(windowGuard.settings.intervalSeconds) + L" seconds";
}

void UpdateTray(bool add = false) {
    const auto tip = L"Window Guard\n" + Status();
    StringCchCopyW(tray.szTip, std::size(tray.szTip), tip.c_str());
    const bool adding = add || !trayAdded;
    trayAdded = Shell_NotifyIconW(adding ? NIM_ADD : NIM_MODIFY, &tray) != FALSE;
    if (trayAdded) {
        KillTimer(tray.hWnd, TrayRetryTimer);
        if (adding) {
            tray.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &tray);
        }
    } else {
        SetTimer(tray.hWnd, TrayRetryTimer, 10000, nullptr);
    }
}

void RefreshDisplays(HWND window) {
    windowGuard.Refresh();
    if (!windowGuard.settings.blockedDisplays.empty() && !guard::DisplayIdentitiesComplete(windowGuard.monitors)) {
        SetTimer(window, RefreshTimer, 10000, nullptr);
    } else {
        KillTimer(window, RefreshTimer);
    }
}

void OpenSettings(HWND window) {
    guard::ShowSettings(appInstance, window, icon, windowGuard.settings, StartsWithWindows(), settingsPath,
        [window](const guard::Settings& next, bool startup, std::wstring& error) {
            if (!guard::SaveSettings(settingsPath, next, error)) return false;
            if (startup != StartsWithWindows() && !SetStartup(startup, error)) {
                std::wstring ignored;
                guard::SaveSettings(settingsPath, windowGuard.settings, ignored);
                return false;
            }
            windowGuard.settings = next;
            RefreshDisplays(window);
            SetCoalescableTimer(window, ScanTimer, next.intervalSeconds * 1000, nullptr, 100);
            UpdateTray();
            return true;
        });
}

void ShowMenu(HWND window) {
    const auto menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, Status().c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MenuSettings, L"Settings...");
    SetMenuDefaultItem(menu, MenuSettings, FALSE);
    AppendMenuW(menu, MF_STRING | (windowGuard.paused ? MF_CHECKED : 0), MenuPause, L"Pause protection");
    AppendMenuW(menu, MF_STRING, MenuRescue, L"Move stranded windows now");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MenuExit, L"Exit");
    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window);
    const auto selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, window, nullptr);
    DestroyMenu(menu);
    PostMessageW(window, WM_NULL, 0, 0);
    if (selected == MenuPause) windowGuard.paused = !windowGuard.paused;
    if (selected == MenuRescue) {
        const auto wasPaused = windowGuard.paused;
        windowGuard.paused = false;
        RefreshDisplays(window);
        windowGuard.Scan();
        windowGuard.paused = wasPaused;
    }
    if (selected == MenuSettings) OpenSettings(window);
    if (selected == MenuExit) DestroyWindow(window);
    else UpdateTray();
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM parameter, LPARAM extra) {
    if (taskbarCreated && message == taskbarCreated) {
        UpdateTray(true);
        return 0;
    }
    switch (message) {
    case WM_TIMER:
        if (parameter == TrayRetryTimer) {
            UpdateTray(true);
            return 0;
        }
        if (parameter == RefreshTimer) {
            RefreshDisplays(window);
            UpdateTray();
        }
        windowGuard.Scan();
        return 0;
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
    case WM_POWERBROADCAST:
        windowGuard.monitors.clear();
        SetTimer(window, RefreshTimer, 1500, nullptr);
        return message == WM_POWERBROADCAST ? TRUE : 0;
    case TrayMessage:
        if (LOWORD(extra) == WM_CONTEXTMENU) ShowMenu(window);
        if (LOWORD(extra) == NIN_SELECT || LOWORD(extra) == NIN_KEYSELECT) OpenSettings(window);
        return 0;
    case ShowMessage:
        OpenSettings(window);
        return 0;
    case StopMessage:
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        guard::CloseSettings();
        KillTimer(window, ScanTimer);
        KillTimer(window, RefreshTimer);
        KillTimer(window, TrayRetryTimer);
        Shell_NotifyIconW(NIM_DELETE, &tray);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, parameter, extra);
    }
}

std::string Utf8(const std::wstring& value) {
    const int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string output(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), output.data(), length, nullptr, nullptr);
    return output;
}

int WriteReport(const wchar_t* path) {
    std::ofstream report(path, std::ios::binary);
    if (!report) return 1;
    auto monitors = guard::ReadMonitors();
    guard::ApplyDisplayRules(monitors, windowGuard.settings);
    for (const auto& monitor : monitors) {
        report << Utf8(monitor.device) << " | " << Utf8(monitor.name) << " | " << (monitor.blocked ? "BLOCKED" : "allowed")
               << " | bounds " << monitor.bounds.left << ',' << monitor.bounds.top << ',' << monitor.bounds.right << ',' << monitor.bounds.bottom
               << " | work " << monitor.work.left << ',' << monitor.work.top << ',' << monitor.work.right << ',' << monitor.work.bottom
               << " | primary " << monitor.primary << '\n';
        for (const auto& identity : monitor.identities) report << "identity=" << Utf8(identity) << '\n';
    }
    return report.good() ? 0 : 1;
}

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    appInstance = instance;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&controls);
    settingsPath = guard::SettingsPath();
    int argumentCount = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    bool background = false;
    bool stop = false;
    std::wstring report;
    for (int index = 1; arguments && index < argumentCount; ++index) {
        if (wcscmp(arguments[index], L"--background") == 0) background = true;
        else if (wcscmp(arguments[index], L"--stop") == 0) stop = true;
        else if (wcscmp(arguments[index], L"--config") == 0 && index + 1 < argumentCount) settingsPath = arguments[++index];
        else if (wcscmp(arguments[index], L"--report") == 0 && index + 1 < argumentCount) report = arguments[++index];
        else {
            MessageBoxW(nullptr, L"WindowGuard.exe [--background] [--config full-path]\n\n--stop: stop the running copy\n--report full-path: write display diagnostics", L"Window Guard", MB_OK);
            LocalFree(arguments);
            return 1;
        }
    }
    if (arguments) LocalFree(arguments);
    guard::LoadSettings(settingsPath, windowGuard.settings);
    if (!report.empty()) return WriteReport(report.c_str());
    if (stop) {
        const auto existing = FindWindowW(WindowClass, nullptr);
        if (existing) PostMessageW(existing, StopMessage, 0, 0);
        return 0;
    }
    const auto mutex = CreateMutexW(nullptr, TRUE, L"Local\\WindowGuard.904121D2");
    if (!mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        const auto existing = FindWindowW(WindowClass, nullptr);
        if (existing && !background) PostMessageW(existing, ShowMessage, 0, 0);
        CloseHandle(mutex);
        return 0;
    }
    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = WindowClass;
    RegisterClassW(&windowClass);
    const auto window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, WindowClass, L"Window Guard",
                                       WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!window) { CloseHandle(mutex); return 1; }
    icon = CreateGuardIcon();
    tray.cbSize = sizeof(tray);
    tray.hWnd = window;
    tray.uID = 1;
    tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    tray.uCallbackMessage = TrayMessage;
    tray.hIcon = icon;
    RefreshDisplays(window);
    UpdateTray(true);
    SetCoalescableTimer(window, ScanTimer, windowGuard.settings.intervalSeconds * 1000, nullptr, 100);
    windowGuard.Scan();
    if (!background) OpenSettings(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (guard::SettingsMessage(message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (icon) DestroyIcon(icon);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return 0;
}
