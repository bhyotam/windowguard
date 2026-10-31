# Window Guard

Some projector and AV receiver setups lose HDMI audio when their display is disabled in Windows. To keep the audio output available, the display has to stay enabled, even if it isn't being used for video. Applications can then open on that unused display, leaving their windows out of sight.

Window Guard works around this by moving those windows back to another screen. It checks for windows on the selected displays every two seconds by default, including windows that only partly overlap them. The HDMI display stays enabled, and audio settings are left unchanged.

The extra display will still appear in Windows Settings, and the mouse pointer can still enter it. Window Guard moves application windows; it doesn't remove the display from the desktop.

## Setup

1. Download the Windows x64 ZIP from [Releases](https://github.com/bhyotam/windowguard/releases/latest), extract it to a permanent folder, and run `WindowGuard.exe`.
2. Under **Displays**, check the display you want to keep windows off. Leave at least one display you use unchecked.
3. Choose a destination screen, or leave **Automatic** selected, then click **Apply**.

Closing settings leaves the program running in the system tray. Startup is off by default. To start it at sign-in, enable the startup option under **General** and click **Apply**.

When using the projector for video, choose **Pause protection** from the tray menu.

## Requirements

Requires an x64 version of Windows 10 (version 1703 or later) or Windows 11. No additional runtime is required. The executable is unsigned, so Windows may show a reputation warning.

See the [usage guide](docs/usage.md) for application rules, configuration, and limitations, or the [development notes](docs/design.md) to build from source.

[MIT license](LICENSE).
