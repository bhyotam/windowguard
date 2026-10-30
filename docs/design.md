# Development notes

`main.cpp` owns the message loop, tray menu, startup entry, and command line. Window discovery and movement are in `guard.cpp`; the rectangle calculations are in `geometry.h`.

`settings.cpp` handles rules and INI files. `settings_ui.cpp` edits a draft of the settings until Apply succeeds. Dialog layouts are in `resources.rc`. Tests are in `tests.cpp`.

## Scanning

The guard runs on the main thread, using a Win32 timer rather than a worker loop. It skips enumeration while paused, while a mouse button is held, or without both a blocked display and an allowed destination.

Each scan enumerates top-level windows. Style, visibility, shell-class, DWM-cloaking, and move/resize checks filter out windows it shouldn't touch. The remaining windows are checked for visible overlap with a blocked display. A process-name lookup is only needed after finding an overlapping window, and only when application rules are in use.

The monitor layout is cached. Display, settings, and power notifications clear it and schedule a refresh. Incomplete identity detection is retried every ten seconds while display rules are configured. Tray registration has its own retry timer for an unavailable or restarting Explorer shell.

Rules use monitor device paths from `QueryDisplayConfig`. A mirrored surface is blocked only if all target identities are known and selected. Unknown displays are left allowed.

## Moving windows

`GetWindowRect` includes invisible resize borders. Using it for overlap checks would move windows whose visible edges are already aligned with a monitor. Instead, the guard uses DWM's extended frame bounds where available, then adds the border offsets back when calculating the move.

A configured destination takes priority. Otherwise, the screen with the greatest visible overlap wins, followed by the nearest screen. The primary screen breaks ties. Windows are fitted to the work area, excluding the taskbar.

Normal windows are moved with asynchronous `SetWindowPos`, without activation or z-order changes. Maximized windows need two steps: move the maximized frame to the destination, then update `WINDOWPLACEMENT` so restoring the window doesn't send it back.

Most `WINDOWPLACEMENT` rectangles use workspace coordinates rather than screen coordinates. `ToWorkspace` accounts for top and left taskbars; tool windows keep screen coordinates.

A successful asynchronous call only means Windows accepted the request. The next scan checks the actual position. If the position doesn't change, the third and later attempts are spaced 30 seconds apart. A changed process ID or rectangle resets this tracking. Entries expire after a minute without seeing the window or when its handle is destroyed.

## Settings and overhead

Settings are written to a temporary file in the same directory, flushed, then moved over the previous file. Both rule lists are limited to 256 entries on read and write.

There is no service or background worker. The default two-second timer permits Windows to coalesce wakeups by up to 100 ms. Scan cost depends on the number of windows and on whether process lookups or moves are needed.

For a quick enumeration benchmark:

```bat
build\GuardTests.exe --benchmark
```

This times 1,000 scans with a synthetic blocked rectangle far outside a normal desktop. It doesn't measure monitor discovery or actual window moves, so don't treat the result as a worst-case cost.

## Tests

Run `build.cmd`, then `build\GuardTests.exe`. The default tests cover geometry, display identities, mirrored displays, application rules, destination fallback, and configuration round-trips. They don't move real application windows or require a particular monitor layout. GitHub Actions runs this build and test suite on Windows.

For native-window tests, exit the guard first. Use a report to find a non-primary display, then substitute its device name:

```bat
build\WindowGuard.exe --report "%TEMP%\window-guard-displays.txt"
build\GuardTests.exe --integration \\.\DISPLAY2
```

These tests create and close their own windows. Shared-edge checks only run when the monitors have the required adjacency; geometry tests cover the same calculations independently.

To test the background timer across processes, use a separate configuration:

```bat
build\GuardTests.exe --make-config \\.\DISPLAY2 "%TEMP%\window-guard-test.ini"
build\WindowGuard.exe --background --config "%TEMP%\window-guard-test.ini"
build\GuardTests.exe --background "%TEMP%\window-guard-test.ini"
build\WindowGuard.exe --stop
```

Run this on a test desktop: the guard will also move other eligible windows from the selected display. Reopen your usual copy afterward.

Before shipping changes to placement, also test the affected applications on real hardware, including mixed DPI and sleep/resume where relevant. For bug reports, include the Windows version, reproduction steps, and whether the affected application is elevated. Check reports and screenshots for personal information before posting.
