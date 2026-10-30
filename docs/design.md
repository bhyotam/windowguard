# Implementation notes

Window Guard does one job: return application windows from a deliberately unused display without disconnecting its HDMI audio. It is a per-user tray application, not a display driver or a window manager.

## Source layout

| File | Responsibility |
| --- | --- |
| `main.cpp` | Message loop, tray menu, startup registration, command line, and display-change notifications. |
| `guard.cpp` | Monitor discovery, window eligibility, process lookup, relocation, and retry tracking. |
| `geometry.h` | Rectangle intersection, destination selection, fitting, and coordinate conversion. |
| `settings.cpp` | Display/application policy and UTF-16 INI persistence. |
| `settings_ui.cpp` | Native settings pages and their unsaved draft state. |
| `resources.rc` | Dialog layouts and executable version information. |
| `tests.cpp` | Geometry, policy, persistence, native-window tests, and a scan benchmark. |

## One scan

The guard uses a Win32 message loop and a coalescable timer. It does no window enumeration while paused, while a mouse button is held, or without both a blocked display and an allowed destination. Display topology is cached rather than queried on every scan.

For each top-level window, it checks visibility, window styles, shell classes, DWM cloaking, and whether the owning thread is moving or resizing it. It then reads the visible frame and tests for overlap with a blocked display. Process-name lookup happens only for an eligible overlapping window when application filtering needs it.

Display selections are matched against monitor device paths returned by `QueryDisplayConfig`. A cloned surface is blocked only if every target identity is known and explicitly selected. Missing identities therefore cannot accidentally block an unidentified screen.

Display, settings, and power notifications invalidate the topology and schedule a delayed refresh. Incomplete identity discovery is retried every ten seconds while display rules are configured. The tray icon has a separate retry timer for an unavailable or restarting Explorer shell.

## Placement details

`GetWindowRect` includes invisible resize borders. Overlap checks use DWM's extended frame bounds when available, while moves include the difference between the visible and outer rectangles. This prevents a window aligned with a monitor edge from being repeatedly rescued because of an invisible border.

The destination is the configured allowed monitor, or the monitor with the greatest visible overlap, then the shortest rectangle-to-rectangle distance. The primary monitor breaks ties. Fitting uses the destination's work area so the taskbar remains accessible.

Normal windows use `SetWindowPos` with asynchronous, no-activation, and no-z-order-change flags. Maximized windows first move to the destination work area, then update their `WINDOWPLACEMENT` restore rectangle while retaining the maximized state. Most placement rectangles use workspace coordinates, unlike ordinary window rectangles; `ToWorkspace` accounts for taskbars on the top or left. Tool windows retain screen coordinates.

An accepted asynchronous request does not prove the target has moved. Subsequent scans observe the actual bounds. If a window stays at the same position, its third and later attempts are spaced 30 seconds apart. Tracking expires after a minute without seeing the window or when its handle is destroyed. A changed process ID or changed bounds resets the attempt state.

## State and resource use

All guard state lives on the main thread. There is no service, worker pool, network client, global input hook, or busy-wait loop. Settings use a separate draft until Apply succeeds. A save writes a same-directory temporary file, flushes it, and replaces the previous file; list sizes are bounded when reading and writing.

The default two-second timer can be coalesced by up to 100 ms. This trades a small amount of response latency for fewer independent wakeups. The scan cost depends on the desktop's window count and target applications; it is not zero and cannot be guaranteed by an idle measurement.

To measure enumeration and eligibility checks without moving normal desktop windows:

```bat
build\GuardTests.exe --benchmark
```

This runs 1,000 scans with a synthetic blocked rectangle far outside an ordinary desktop. It excludes monitor discovery and actual relocation. It is a baseline measurement, not a worst-case performance test.

## Tests

Build with `build.cmd`, then run `build\GuardTests.exe`. This default suite does not move real application windows or depend on a particular monitor arrangement. It covers geometry, display identities, mirrored displays, application rules, destination fallback, and configuration round-trips. GitHub Actions runs the same build and tests on Windows.

For native-window integration tests, exit the guard first. Find a non-primary test display with a diagnostic report, then substitute its device name:

```bat
build\WindowGuard.exe --report "%TEMP%\window-guard-displays.txt"
build\GuardTests.exe --integration \\.\DISPLAY2
```

The suite creates, moves, and closes its own temporary windows. Shared-edge checks run only when the selected displays have the required adjacency; the default geometry tests cover those cases independently.

To check the actual background timer across processes, create an isolated configuration and launch the guard with it:

```bat
build\GuardTests.exe --make-config \\.\DISPLAY2 "%TEMP%\window-guard-test.ini"
build\WindowGuard.exe --background --config "%TEMP%\window-guard-test.ini"
build\GuardTests.exe --background "%TEMP%\window-guard-test.ini"
build\WindowGuard.exe --stop
```

Do this on a test desktop: the running guard will also manage other eligible windows on the selected display. Reopen your usual copy afterward. These tests are not substitutes for testing a real media player, mixed DPI, driver changes, sleep/resume, or elevated applications on the target hardware.

Keep contributions focused, build with warnings enabled, and run the relevant tests. For a bug report, include Windows version, reproduction steps, and whether the affected application is elevated. Review configuration, reports, and screenshots for personal information before posting them.
