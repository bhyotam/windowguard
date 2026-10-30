# Window Guard

Keep HDMI audio without losing windows on a screen you cannot see.

An HDMI receiver or projector can appear in Windows as another monitor, even when you only use it for sound. On some setups, disconnecting that display in Windows also removes the HDMI audio output. Leaving it connected keeps the sound working, but gives applications somewhere invisible to open.

Window Guard leaves that connection alone. It runs in the system tray and moves windows off the displays you mark as audio-only, back onto a screen you actually use. It also catches windows that are only partly on the unwanted display.

This is a workaround for the extra desktop, not a driver fix: the display stays enabled, audio routing stays unchanged, and your mouse can still enter that desktop area.

## Set it up

1. Download the Windows x64 ZIP from [Releases](../../releases/latest), extract it into a folder you will keep, and run `WindowGuard.exe`.
2. Under **Displays**, check the HDMI display you want to keep clear. Leave your normal screen unchecked.
3. Choose where windows should return, or leave **Automatic** selected. Click **Apply**.
4. If you want it running after a restart, enable **Start Window Guard when I sign in to Windows** under **General**.

Nothing is blocked until you choose a display. The default check interval is two seconds. Closing settings leaves the guard running; right-click its tray icon to pause it or exit.

Windows 10 version 1703 or later, or Windows 11, x64. No installer, administrator account, or extra runtime is needed for ordinary applications. The executable is unsigned, so Windows may show a reputation warning. Do not turn off security protections to run it.

## When you actually use the projector

Pause the guard from the tray before putting windows on that display. Alternatively, use **Applications** to exclude your media player, such as `vlc.exe`. You can also manage only a specific list of applications instead of all of them. Rules apply to executable names, not individual documents or window titles.

The **General** tab controls the check interval, partial overlaps, and whether oversized windows may be resized. Maximized windows stay maximized. The guard waits while you drag a window and leaves minimized windows alone until you restore them.

## Things to know

Some elevated applications, exclusive-fullscreen games, and windows that enforce their own size or position may resist being moved. Display identities can change after a graphics-driver update or a cable-port change; check your selection if protection stops working.

There are no network requests, accounts, or telemetry. Settings stay on your PC. To remove the program, turn off its startup option, exit through the tray, and delete its folder.

See [settings and troubleshooting](docs/usage.md) for storage locations, display rules, command-line options, and limitations.

## Build and development

Install Visual Studio 2022 or newer with **Desktop development with C++** and a Windows SDK. From the source directory, run:

```bat
build.cmd
build\GuardTests.exe
```

The program is C++17 using Win32 controls and a statically linked runtime. The build treats warnings as errors. [Implementation notes](docs/design.md) explain window detection, placement, resource use, and the test commands.

Licensed under [MIT](LICENSE).
