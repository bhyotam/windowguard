# Settings

## Displays

Check the displays you want to keep clear. You must leave at least one connected display unchecked.

**Automatic** picks the allowed screen containing the largest visible part of the window. If the window is entirely off your allowed screens, it picks the nearest one. A destination you select takes priority; if it's disconnected or blocked, the guard falls back to Automatic.

Selections are saved by Windows device identity, not model name or display number. Unplugging a monitor doesn't erase its rule. **Clear display rules** also clears saved selections for disconnected monitors. A driver update or different cable port can change an identity, so you may need to select that display again.

Mirrored displays share one desktop surface. The group is blocked only when every target is identified and selected. If the arrangement changes while settings are open, click **Refresh displays**.

## Applications

The list has two modes:

- **All applications, except those listed below** leaves the listed applications alone.
- **Only applications listed below** moves only the listed applications. An empty list moves nothing.

Type an executable name, use **Browse**, or choose a running application. For example, `vlc.exe` matches every VLC process with that filename, regardless of its folder. Matching ignores letter case. Window titles and wildcards aren't supported. You can save up to 256 application rules.

If the guard can't identify a process while filtering is enabled, it leaves that window alone. Some older packaged apps share a host executable, so they can't be separated by these rules.

## General

The check interval ranges from 1 to 30 seconds and defaults to 2. Checks wait while a mouse button is held or a window is being dragged or resized.

**Also move windows that only partly overlap a blocked display** counts even one visible pixel. Invisible resize borders don't count. Turn it off to leave a window alone whenever any visible part is on an allowed screen.

**Resize oversized windows to fit the destination** affects resizable windows only. An application's minimum size may still prevent it from fitting.

Minimized windows stay minimized. Maximized windows stay maximized, with their restore position moved to the destination too. Hidden windows, other virtual desktops, menus, taskbars, and nonactivating overlays are skipped.

## A window isn't moving

First check the tray menu: is protection paused? Then check the display selection, destination, and application rules. **Move stranded windows now** requests a check without waiting for the timer.

Administrator windows may require Window Guard to run with matching privileges. Only elevate it if you need to manage those windows. It cannot move UAC prompts or windows on secure desktops.

Fullscreen applications and programs that reset their own position can resist a move. After three attempts at an unchanged position, the guard waits 30 seconds between attempts. Another window manager can also interfere.

The guard only acts on windows overlapping a selected blocked display. It doesn't recover windows that are outside every connected display, and it doesn't restrict the mouse pointer.

## Settings files and startup

Settings normally live in `%APPDATA%\WindowGuard\settings.ini`. The file contains display identities, application names, and preferences. Review it before attaching it to a bug report. The program makes no network requests and doesn't read window contents.

Startup is off by default. Enabling it creates a `WindowGuard` value in `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, containing the executable and configuration paths. If you move the program, exit the old copy, open the new one, and enable startup there.

Only one copy runs per Windows session. Launching it again opens the existing settings window rather than loading a different configuration. A second launch with `--background` doesn't open settings.

```text
WindowGuard.exe                         Open settings and run in the tray
WindowGuard.exe --background            Run without opening settings
WindowGuard.exe --stop                  Stop this session's running copy
WindowGuard.exe --config FULL-PATH      Use a separate settings file
WindowGuard.exe --report FULL-PATH      Write display diagnostics and exit
```

Use an absolute `--config` path for a portable setup. The startup option remembers it. Diagnostic reports include monitor identities; review them before sharing.

To uninstall, turn off startup, exit from the tray, and delete the program folder. Remove the settings file separately if you no longer want the saved rules.
