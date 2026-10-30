# Settings and troubleshooting

## Displays

Checked displays are kept clear; unchecked displays remain available. At least one connected display must remain allowed.

**Automatic** returns a window to the allowed display containing the largest visible portion of it. If none contains any part, the nearest allowed display is used. A selected destination takes priority, with automatic placement as the fallback if that display is disconnected or blocked.

Rules use Windows monitor device identities, not model names or display numbers. Two monitors of the same model can have different rules. Disconnected selections are retained; **Clear display rules** removes those selections too. Mirrored displays share one desktop surface, so the group is blocked only when every identified target is blocked. An unidentified target is left allowed.

If the monitor arrangement changes while settings are open, click **Refresh displays**. A different cable port or graphics driver can change the device identity; reselect the display if necessary.

## Applications

In **All applications, except those listed below**, the list contains exceptions. In **Only applications listed below**, it contains the applications to manage; an empty list moves nothing.

Type an executable name, browse to an `.exe`, or choose a running application. Matching ignores letter case and uses the filename only: `vlc.exe` covers every instance of that filename, regardless of installation folder. Rules do not match window titles, full paths, or wildcards. There is a limit of 256 application rules.

An application that cannot be identified is left alone when filtering is enabled. Older packaged applications may run under a shared host executable, making separate rules impractical.

## Timing and window sizes

The check interval is 1-30 seconds; the default is 2. This is periodic checking, not an immediate barrier. Windows may coalesce timer wakeups by up to 100 ms, and a busy system can delay them further.

With **Also move windows that only partly overlap a blocked display** enabled, even one visible pixel of overlap qualifies. Invisible resize borders do not count. With it disabled, a window is left alone if any visible portion is on an allowed display.

**Resize oversized windows to fit the destination** applies to resizable windows. Fixed-size windows or applications with a minimum size larger than the destination cannot always fit completely.

The guard skips hidden and minimized windows, other virtual desktops, menus, tooltips, nonactivating overlays, the desktop, and taskbars. It waits while a mouse button is held or a window is being dragged or resized. Normal moves preserve focus and stacking order. Maximized windows keep their maximized state and receive a new restore position.

## If a window is not moving

Check that protection is not paused, the unwanted display is checked, and an allowed destination is connected. Check the application rules next. Use **Move stranded windows now** in the tray menu to request an immediate check.

Administrator windows may need Window Guard to run with matching privileges; do not elevate it unless you need that access. UAC prompts and secure desktops are not supported. Exclusive-fullscreen applications and programs that immediately reposition themselves may resist relocation. Repeated unchanged-position attempts slow to once every 30 seconds rather than running indefinitely at the normal interval.

Other window-management tools can conflict with the guard. It does not disable displays, change audio settings, or keep the pointer out of an unused desktop. It also does not restore windows that are entirely outside all connected displays unless they overlap a selected blocked display.

## Configuration and startup

The default settings file is `%APPDATA%\WindowGuard\settings.ini`. It contains display identities, executable-name rules, and preferences. Review it before sharing it in a bug report. Saves replace the file atomically.

Startup is optional and off on a fresh installation. Enabling it creates the `WindowGuard` value under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`. It records the current executable and configuration paths. If you move the program later, open it from the new location and enable startup there again.

Only one copy runs per Windows session. A second launch opens the existing settings window; it does not replace the running copy's configuration.

```text
WindowGuard.exe                         Open settings and run in the tray
WindowGuard.exe --background            Run without opening settings
WindowGuard.exe --stop                  Stop this session's running copy
WindowGuard.exe --config FULL-PATH      Use a separate settings file
WindowGuard.exe --report FULL-PATH      Write display diagnostics and exit
```

For a portable setup, pass an absolute `--config` path beside the executable. Startup remembers that path when enabled. Diagnostic reports contain monitor identities and should be reviewed before sharing.

To uninstall, disable startup, exit from the tray, and delete the program folder. Delete the configuration file separately if you want to remove saved rules as well. No service or driver is installed.
