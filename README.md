# Window Guard

If you send audio over HDMI to a receiver or projector, Windows may treat it as another monitor. Disable that display and the audio output can disappear too. Leave it enabled and applications can open where you can't see them.

Window Guard moves those windows back to your normal screens. The HDMI connection stays active, so you can keep using it for sound.

## Setup

Requires Windows 10 version 1703 or later, or Windows 11, x64. There is no installer or extra runtime to install.

1. Download the Windows x64 ZIP from [Releases](https://github.com/bhyotam/windowguard/releases/latest). Extract it to a permanent folder and run `WindowGuard.exe`.
2. In **Displays**, check the screen you want to keep clear. Leave your normal screens unchecked.
3. Pick a destination, or leave **Automatic** selected, then click **Apply**.

The default check interval is two seconds. Closing settings leaves the program in the tray. To start it at sign-in, enable that option under **General**.

When you want to use the projector, right-click the tray icon and pause protection. You can also exclude your media player under **Applications**, so the guard leaves it alone.

## Limitations

This doesn't remove the extra desktop. Your pointer can still enter it, and a window may sit there until the next check.

Minimized windows are left alone until you restore them. Some administrator windows, fullscreen games, and applications that control their own position may not move. If a driver update or cable change alters a display's identity, select it again in settings.

The executable is unsigned and may trigger a Windows reputation warning. Don't disable Windows security protections to run it.

See [Settings](docs/usage.md) for application rules, startup, configuration files, and troubleshooting.

## Build

Install Visual Studio 2022 or newer with **Desktop development with C++** and a Windows SDK, then run:

```bat
build.cmd
build\GuardTests.exe
```

The code is C++17 and Win32, with a statically linked runtime. [Development notes](docs/design.md) cover the placement logic and tests.

[MIT license](LICENSE).
