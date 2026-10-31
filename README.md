# Window Guard

I wanted to use HDMI for sound without having an extra screen in Windows.

With some projectors and AV receivers, disabling the display in Windows also removes the HDMI audio output. Keeping it enabled gets the sound working, but leaves another desktop where applications can open out of sight.

Window Guard is a workaround for that. It leaves the HDMI display connected and moves windows back to your other screens, including windows that are only partly on it. By default, it checks every two seconds.

It doesn't give Windows a true audio-only HDMI mode. The extra display will still appear in Settings, and your mouse can still enter it. The point is to stop losing application windows there.

## Using it

1. Download the Windows x64 ZIP from [Releases](https://github.com/bhyotam/windowguard/releases/latest), extract it, and run `WindowGuard.exe`.
2. Under **Displays**, check the screen you don't want windows on. Leave your normal screens unchecked.
3. Choose where the windows should go, or leave **Automatic** selected, and click **Apply**.

Closing settings leaves it running in the tray. If you want it to start with Windows, keep the executable in a permanent folder and enable startup under **General**.

If you also use the projector for video, pause the guard from the tray menu or exclude your player under **Applications**.

Windows 10 (1703 or later) or Windows 11, x64. The executable isn't code-signed, so Windows may show a warning.

[Settings and troubleshooting](docs/usage.md) · [Building and development](docs/design.md) · [MIT license](LICENSE)
