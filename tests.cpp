#include "guard.h"
#include <cstdio>
#include <cstdlib>
#include <dwmapi.h>

namespace {

unsigned checks = 0;
unsigned failures = 0;

void Check(bool condition, const char* label) {
    ++checks;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition) ++failures;
}

guard::Monitor MakeMonitor(RECT bounds, bool blocked = false, bool primary = false) {
    guard::Monitor monitor;
    monitor.bounds = bounds;
    monitor.work = bounds;
    monitor.blocked = blocked;
    monitor.primary = primary;
    return monitor;
}

bool Same(RECT first, RECT second) { return EqualRect(&first, &second) != FALSE; }

void GeometryTests() {
    std::vector<guard::Monitor> monitors = {
        MakeMonitor({-1920, 0, 0, 1080}),
        MakeMonitor({0, 0, 2560, 1440}, false, true),
        MakeMonitor({2560, 0, 4480, 1080}, true)
    };
    Check(!guard::TouchesBlocked({20, 20, 1200, 800}, monitors), "safe window is untouched");
    Check(!guard::TouchesBlocked({2000, 20, 2560, 800}, monitors), "touching edge is not overlap");
    Check(guard::TouchesBlocked({2000, 20, 2561, 800}, monitors), "one visible pixel triggers rescue");
    Check(guard::TouchesBlocked({2700, 20, 3400, 800}, monitors), "fully stranded window triggers rescue");
    Check(!guard::TouchesBlocked({2700, 1100, 3400, 1300}, monitors), "separate vertical region is not blocked");
    Check(guard::PickDestination({2400, 20, 2900, 800}, monitors) == &monitors[1], "prefer monitor containing visible part");
    Check(guard::PickDestination({3000, 20, 3500, 800}, monitors) == &monitors[1], "fully stranded uses nearest real monitor");
    Check(guard::PickDestination({-1500, 20, -500, 800}, monitors) == &monitors[0], "negative monitor coordinates work");
    Check(Same(guard::FitInside({2400, 20, 2900, 800}, monitors[1].work, true), {2060, 20, 2560, 800}), "partial window is moved minimally");
    Check(Same(guard::FitInside({20, 20, 3020, 2020}, monitors[1].work, true), {0, 0, 2560, 1440}), "oversized resizable window shrinks to fit");
    Check(Same(guard::FitInside({20, 20, 3020, 2020}, monitors[1].work, false), {0, 0, 3000, 2000}), "fixed-size window keeps dimensions");
    Check(Same(guard::FitInside({-2500, -200, -2000, 300}, monitors[0].work, true), {-1920, 0, -1420, 500}), "negative-position clamping");
    auto taskbarMonitor = MakeMonitor({-1920, -1080, 0, 0});
    taskbarMonitor.work = {-1880, -1040, 0, 0};
    Check(Same(guard::ToWorkspace({-1800, -1000, -800, -200}, taskbarMonitor, false), {-1840, -1040, -840, -240}), "workspace conversion handles top and left taskbars");
    Check(Same(guard::ToWorkspace({-1800, -1000, -800, -200}, taskbarMonitor, true), {-1800, -1000, -800, -200}), "tool windows keep screen coordinates");
    Check(guard::IntersectionArea({-50000, -50000, 50000, 50000}, {-50000, -50000, 50000, 50000}) == 10000000000LL, "area uses 64-bit math");
    guard::WindowGuard protector;
    Check(!protector.Ready(), "no monitors fails safely");
    protector.monitors = {monitors[2]};
    Check(!protector.Ready(), "no safe destination fails safely");
    Check(guard::PickDestination({2700, 20, 3400, 800}, protector.monitors) == nullptr, "blocked-only layout has no destination");
    protector.monitors = {monitors[0], monitors[1]};
    Check(!protector.Ready(), "disconnected audio display disables repairs");
    protector.monitors = monitors;
    Check(protector.Ready(), "normal topology enables repairs");
    monitors[2].blocked = false;
    Check(!guard::TouchesBlocked({2700, 20, 3400, 800}, monitors), "mirrored allowed target is not blocked");
}

void PolicyTests() {
    guard::Settings settings;
    auto safe = MakeMonitor({0, 0, 1920, 1080}, false, true);
    safe.name = L"Same model";
    safe.identities = {L"screen-a"};
    auto blocked = MakeMonitor({1920, 0, 3840, 1080});
    blocked.name = L"Same model";
    blocked.identities = {L"screen-b"};
    auto other = MakeMonitor({-1920, 0, 0, 1080});
    other.identities = {L"screen-c"};
    std::vector<guard::Monitor> monitors = {safe, blocked, other};
    Check(guard::DisplayIdentitiesComplete(monitors), "identified display layout does not need a retry");
    Check(!guard::DisplayIdentitiesComplete({}), "unavailable display layout is retried");
    auto unidentified = monitors;
    unidentified[0].identities.clear();
    Check(!guard::DisplayIdentitiesComplete(unidentified), "missing monitor identity is retried");
    unidentified[0].identities = {L""};
    Check(!guard::DisplayIdentitiesComplete(unidentified), "empty monitor identity is retried");
    guard::ApplyDisplayRules(monitors, settings);
    Check(!monitors[0].blocked && !monitors[1].blocked, "new installs do not block any display");
    settings.blockedDisplays = {L"SCREEN-B"};
    guard::ApplyDisplayRules(monitors, settings);
    Check(!monitors[0].blocked && monitors[1].blocked, "identical monitor names use distinct device identities");
    settings.destination = L"screen-c";
    Check(guard::Destination({2000, 100, 2400, 500}, monitors, settings) == &monitors[2], "chosen destination overrides nearest monitor");
    settings.destination = L"disconnected-screen";
    Check(guard::Destination({2000, 100, 2400, 500}, monitors, settings) == &monitors[0], "missing preferred display falls back safely");
    settings.destination = L"screen-b";
    Check(guard::Destination({2000, 100, 2400, 500}, monitors, settings) == &monitors[0], "blocked destination is never used");
    settings.partialOverlap = false;
    Check(!guard::NeedsRescue({1800, 100, 2200, 500}, monitors, settings), "partial-overlap option can be disabled");
    Check(guard::NeedsRescue({2000, 100, 2400, 500}, monitors, settings), "fully stranded window still rescued with partial option disabled");
    settings.partialOverlap = true;
    Check(guard::NeedsRescue({1800, 100, 2200, 500}, monitors, settings), "partial-overlap option can be enabled");
    monitors[1].identities.push_back(L"clone-allowed");
    guard::ApplyDisplayRules(monitors, settings);
    Check(!monitors[1].blocked, "a clone with an allowed target stays available");
    settings.blockedDisplays.push_back(L"clone-allowed");
    guard::ApplyDisplayRules(monitors, settings);
    Check(monitors[1].blocked, "a clone group can be blocked explicitly");
    monitors[1].identities.push_back(L"");
    guard::ApplyDisplayRules(monitors, settings);
    Check(!monitors[1].blocked, "unidentified mirrored target fails safely");
    Check(guard::MatchesApplication(settings, L"anything.exe"), "default application policy covers all apps");
    settings.applications = {L"vlc.exe"};
    Check(!guard::MatchesApplication(settings, L"C:\\Apps\\VLC.EXE"), "excluded app matching is case-insensitive");
    Check(guard::MatchesApplication(settings, L"notepad.exe"), "exclusion does not affect other apps");
    Check(!guard::MatchesApplication(settings, L""), "unidentified process is skipped with app rules");
    settings.onlyListed = true;
    Check(guard::MatchesApplication(settings, L"vlc.exe"), "only-listed mode includes chosen app");
    Check(!guard::MatchesApplication(settings, L"notepad.exe"), "only-listed mode skips other apps");
    settings.applications.clear();
    Check(!guard::MatchesApplication(settings, L"notepad.exe"), "empty inclusion list moves nothing");
    Check(!guard::ValidApplication(L"*"), "wildcards are not silently accepted");
    Check(!guard::ValidApplication(L"app.exe\nnext"), "multiline application names are rejected");
    Check(guard::ValidApplication(L"my player.exe"), "executable names may contain spaces");
    Check(guard::ApplicationName(L"\"C:\\Apps\\Player.EXE\"") == L"player.exe", "pasted executable paths are normalized");

    wchar_t temporaryDirectory[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temporaryDirectory);
    const auto path = std::wstring(temporaryDirectory) + L"WindowGuard-test-" + std::to_wstring(GetCurrentProcessId()) + L".ini";
    settings.applications = {L"player.exe", L"נגן.exe"};
    settings.intervalSeconds = 7;
    settings.resizeOversized = false;
    std::wstring error;
    Check(guard::SaveSettings(path, settings, error), "settings are saved atomically");
    guard::Settings loaded;
    Check(guard::LoadSettings(path, loaded), "settings reload succeeds");
    Check(loaded.applications == settings.applications, "Unicode application rules round-trip");
    Check(loaded.blockedDisplays == settings.blockedDisplays && loaded.destination == settings.destination, "display selections round-trip");
    Check(loaded.intervalSeconds == 7 && loaded.onlyListed && !loaded.resizeOversized, "behavior settings round-trip");
    WritePrivateProfileStringW(L"General", L"IntervalSeconds", L"9999", path.c_str());
    guard::LoadSettings(path, loaded);
    Check(loaded.intervalSeconds == 30, "out-of-range interval is clamped");
    settings.applications.clear();
    for (unsigned index = 0; index < guard::MaxRules; ++index) settings.applications.push_back(L"app" + std::to_wstring(index) + L".exe");
    Check(guard::SaveSettings(path, settings, error), "maximum application rule count can be saved");
    Check(guard::LoadSettings(path, loaded) && loaded.applications == settings.applications, "maximum application rule count round-trips");
    settings.applications.push_back(L"overflow.exe");
    Check(!guard::SaveSettings(path, settings, error), "too many application rules are rejected before writing");
    Check(guard::LoadSettings(path, loaded) && loaded.applications.size() == guard::MaxRules, "rejected save preserves previous rules");
    settings.applications.clear();
    settings.blockedDisplays.resize(guard::MaxRules + 1, L"display");
    Check(!guard::SaveSettings(path, settings, error), "too many saved display rules are rejected");
    DeleteFileW(path.c_str());
    Check(!guard::LoadSettings(path, loaded) && loaded.blockedDisplays.empty() && loaded.intervalSeconds == 2, "missing config restores safe defaults");
    Check(!guard::SaveSettings(L"", settings, error), "unavailable configuration path reports failure");
}

void Pump(unsigned milliseconds = 180) {
    const auto deadline = GetTickCount64() + milliseconds;
    do {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(10);
    } while (GetTickCount64() < deadline);
    DwmFlush();
}

HWND TestWindow(const RECT& rect, DWORD style = WS_OVERLAPPEDWINDOW) {
    const auto window = CreateWindowExW(WS_EX_APPWINDOW, L"WindowGuard.Test", L"Window Guard - temporary test",
        style, rect.left, rect.top, guard::Width(rect), guard::Height(rect), nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (window) {
        const BOOL disableTransitions = TRUE;
        DwmSetWindowAttribute(window, DWMWA_TRANSITIONS_FORCEDISABLED, &disableTransitions, sizeof(disableTransitions));
        ShowWindow(window, SW_SHOWNOACTIVATE);
        Pump();
    }
    return window;
}

RECT Visible(HWND window) {
    RECT outer{};
    RECT visible{};
    guard::WindowBounds(window, outer, visible);
    return visible;
}

bool SelectTestDisplay(guard::WindowGuard& protector, const std::wstring& device) {
    protector.Refresh();
    for (const auto& monitor : protector.monitors) {
        if (_wcsicmp(monitor.device.c_str(), device.c_str()) == 0) protector.settings.blockedDisplays = monitor.identities;
    }
    protector.Refresh();
    return protector.Ready();
}

void IntegrationTests(const std::wstring& device) {
    guard::WindowGuard protector;
    SelectTestDisplay(protector, device);
    for (const auto& monitor : protector.monitors) {
        std::wprintf(L"MONITOR %ls %ls %ld,%ld,%ld,%ld blocked=%d\n", monitor.device.c_str(), monitor.name.c_str(),
            monitor.bounds.left, monitor.bounds.top, monitor.bounds.right, monitor.bounds.bottom, monitor.blocked);
    }
    Check(protector.Ready(), "selected test display and an allowed destination detected");
    if (!protector.Ready()) return;
    const guard::Monitor* blocked = nullptr;
    for (const auto& monitor : protector.monitors) if (monitor.blocked) blocked = &monitor;
    if (!blocked) return;
    const auto allowed = guard::PickDestination(blocked->bounds, protector.monitors);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"WindowGuard.Test";
    RegisterClassW(&windowClass);
    const RECT stranded = {blocked->bounds.left + 80, blocked->bounds.top + 80, blocked->bounds.left + 580, blocked->bounds.top + 400};
    auto window = TestWindow(stranded);
    Check(window != nullptr, "create isolated test window");
    if (!window) return;
    Check(guard::TouchesBlocked(Visible(window), protector.monitors), "test window starts on audio display");
    const auto foreground = GetForegroundWindow();
    protector.settings.applications = {guard::ProcessName(GetCurrentProcessId())};
    Check(!protector.Repair(window), "executable exclusion prevents native relocation");
    protector.settings.onlyListed = true;
    Check(protector.Repair(window), "repair fully stranded native window");
    protector.settings.onlyListed = false;
    protector.settings.applications.clear();
    Pump();
    Check(!guard::TouchesBlocked(Visible(window), protector.monitors), "native window is fully off audio display");
    Check(GetForegroundWindow() == foreground, "normal repair does not steal keyboard focus");
    const auto repaired = Visible(window);
    Check(!protector.Repair(window) && Same(repaired, Visible(window)), "repaired window is stable on repeated scan");
    DestroyWindow(window);

    RECT partial = stranded;
    partial.left = blocked->bounds.right - 100;
    partial.right = blocked->bounds.right + 400;
    window = TestWindow(partial);
    Check(guard::TouchesBlocked(Visible(window), protector.monitors), "partial test window intersects audio display");
    Check(protector.Repair(window), "repair partially stranded native window");
    Pump();
    Check(!guard::TouchesBlocked(Visible(window), protector.monitors), "partial window fully returned");
    DestroyWindow(window);

    if (blocked->bounds.right == allowed->bounds.left && blocked->bounds.top < allowed->bounds.bottom) {
        const RECT safeRect = {allowed->work.left + 30, blocked->bounds.top + 30, allowed->work.left + 530, blocked->bounds.top + 350};
        window = TestWindow(safeRect);
        auto frame = Visible(window);
        RECT outer{};
        GetWindowRect(window, &outer);
        const auto frameOffset = frame.left - outer.left;
        SetWindowPos(window, nullptr, allowed->work.left - frameOffset, safeRect.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        Pump();
        Check(!guard::TouchesBlocked(Visible(window), protector.monitors), "invisible resize border is not counted as overlap");
        Check(!protector.Repair(window), "border-only overlap is not moved");
        SetWindowPos(window, nullptr, allowed->work.left - frameOffset - 1, safeRect.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        Pump();
        Check(guard::TouchesBlocked(Visible(window), protector.monitors), "one-pixel native overlap detected");
        Check(protector.Repair(window), "one-pixel native overlap repaired");
        Pump();
        Check(!guard::TouchesBlocked(Visible(window), protector.monitors), "one-pixel window fully returned");
        DestroyWindow(window);
    }

    window = TestWindow(stranded);
    protector.paused = true;
    const auto beforePause = Visible(window);
    Check(!protector.Repair(window) && Same(beforePause, Visible(window)), "pause prevents relocation");
    protector.paused = false;
    ShowWindow(window, SW_MINIMIZE);
    Pump();
    Check(!protector.Repair(window) && IsIconic(window), "minimized windows are not restored");
    ShowWindow(window, SW_HIDE);
    Check(!protector.Repair(window), "hidden windows are ignored");
    DestroyWindow(window);

    window = TestWindow(stranded);
    SetWindowPos(window, nullptr, stranded.left, stranded.top, guard::Width(allowed->work) + 300,
        guard::Height(allowed->work) + 300, SWP_NOACTIVATE | SWP_NOZORDER);
    Pump();
    Check(protector.Repair(window), "oversized native window is repaired");
    Pump();
    Check(!guard::TouchesBlocked(Visible(window), protector.monitors), "oversized window no longer intersects audio display");
    Check(guard::Width(Visible(window)) <= guard::Width(allowed->work), "oversized window fits destination width");
    DestroyWindow(window);

    window = TestWindow(stranded);
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    GetWindowPlacement(window, &placement);
    placement.showCmd = SW_SHOWMAXIMIZED;
    SetWindowPlacement(window, &placement);
    Pump();
    Check(IsZoomed(window) && guard::TouchesBlocked(Visible(window), protector.monitors), "maximized test starts on audio display");
    const auto maxForeground = GetForegroundWindow();
    Check(protector.Repair(window), "repair maximized native window");
    Pump();
    Check(IsZoomed(window), "maximized window stays maximized");
    Check(!guard::TouchesBlocked(Visible(window), protector.monitors), "maximized window moves to real display");
    Check(GetForegroundWindow() == maxForeground, "maximized repair preserves foreground window");
    GetWindowPlacement(window, &placement);
    placement.showCmd = SW_SHOWNOACTIVATE;
    SetWindowPlacement(window, &placement);
    Pump();
    Check(!guard::TouchesBlocked(Visible(window), protector.monitors), "restoring a rescued maximized window stays on a real screen");
    DestroyWindow(window);
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
}

void BackgroundTests(const std::wstring& path) {
    Check(FindWindowW(L"WindowGuard.Control.2", nullptr) != nullptr, "background guard is running");
    auto monitors = guard::ReadMonitors();
    guard::Settings settings;
    guard::LoadSettings(path, settings);
    guard::ApplyDisplayRules(monitors, settings);
    const guard::Monitor* blocked = nullptr;
    for (const auto& monitor : monitors) if (monitor.blocked) blocked = &monitor;
    Check(blocked != nullptr, "background test identifies audio-only display");
    if (!blocked) return;
    const auto allowed = guard::PickDestination(blocked->bounds, monitors);
    if (!allowed) return;
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"WindowGuard.Test";
    RegisterClassW(&windowClass);
    const RECT stranded = {blocked->bounds.left + 100, blocked->bounds.top + 100, blocked->bounds.left + 600, blocked->bounds.top + 400};
    const RECT safe = {allowed->work.left + 300, allowed->work.top + 200, allowed->work.left + 800, allowed->work.top + 500};
    auto waitForRescue = [&](HWND window) {
        const auto deadline = GetTickCount64() + settings.intervalSeconds * 2000 + 2500;
        while (guard::TouchesBlocked(Visible(window), monitors) && GetTickCount64() < deadline) Pump(50);
        return !guard::TouchesBlocked(Visible(window), monitors);
    };
    auto window = TestWindow(stranded);
    const auto foreground = GetForegroundWindow();
    Check(waitForRescue(window), "configured timer rescues a different process's window");
    Check(GetForegroundWindow() == foreground, "background timer does not steal foreground for normal windows");
    DestroyWindow(window);

    const RECT partial = {blocked->bounds.right - 100, blocked->bounds.top + 100,
                          blocked->bounds.right + 400, blocked->bounds.top + 400};
    window = TestWindow(partial);
    Check(waitForRescue(window), "background timer rescues partial overlap across processes");
    DestroyWindow(window);

    window = TestWindow(stranded);
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    GetWindowPlacement(window, &placement);
    placement.showCmd = SW_SHOWMAXIMIZED;
    SetWindowPlacement(window, &placement);
    const auto sentinel = TestWindow(safe);
    SetForegroundWindow(sentinel);
    Pump(50);
    const auto expectedForeground = GetForegroundWindow();
    Check(expectedForeground != nullptr, "foreground window recorded before maximized rescue");
    Check(waitForRescue(window), "background timer rescues maximized window across processes");
    Check(IsZoomed(window), "cross-process rescue retains maximized state");
    Check(GetForegroundWindow() == expectedForeground, "background maximized rescue does not steal foreground");
    DestroyWindow(window);
    DestroyWindow(sentinel);
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
}

int Benchmark() {
    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) || (GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
        std::printf("Release mouse buttons before running the benchmark.\n");
        return EXIT_FAILURE;
    }
    guard::WindowGuard protector;
    protector.monitors = guard::ReadMonitors();
    if (protector.monitors.empty()) return EXIT_FAILURE;
    auto unused = MakeMonitor({1000000, 1000000, 1001000, 1001000}, true);
    protector.monitors.push_back(unused);
    LARGE_INTEGER frequency{};
    LARGE_INTEGER started{};
    LARGE_INTEGER finished{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&started);
    constexpr unsigned scans = 1000;
    for (unsigned index = 0; index < scans; ++index) protector.Scan();
    QueryPerformanceCounter(&finished);
    const double milliseconds = 1000.0 * (finished.QuadPart - started.QuadPart) / frequency.QuadPart;
    std::printf("%u desktop scans: %.3f ms total, %.4f ms per scan\n", scans, milliseconds, milliseconds / scans);
    return EXIT_SUCCESS;
}

}

int wmain(int argumentCount, wchar_t** arguments) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (argumentCount == 2 && wcscmp(arguments[1], L"--benchmark") == 0) return Benchmark();
    if (argumentCount == 4 && wcscmp(arguments[1], L"--make-config") == 0) {
        guard::WindowGuard protector;
        std::wstring error;
        return SelectTestDisplay(protector, arguments[2]) && guard::SaveSettings(arguments[3], protector.settings, error) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    GeometryTests();
    PolicyTests();
    if (argumentCount == 3 && wcscmp(arguments[1], L"--integration") == 0) IntegrationTests(arguments[2]);
    if (argumentCount > 1 && wcscmp(arguments[1], L"--background") == 0) BackgroundTests(argumentCount > 2 ? arguments[2] : guard::SettingsPath());
    std::printf("\n%u checks, %u failures\n", checks, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
