# Changelog

## 2.0.0

- Add settings for blocked displays, a destination screen, and application exceptions or inclusion lists.
- Make the check interval, partial-overlap handling, resizing, and startup at sign-in configurable.
- Save rules per user and keep them when a display is unplugged. Use device identities to distinguish monitors of the same model.
- Leave unidentified displays allowed and avoid blocking only part of a mirrored display group.
- Retry display detection and tray registration when they are unavailable at startup or after resume.
- Add geometry, configuration, and window-movement tests, plus a Windows CI build.

## 1.0.0

- Return windows from a fixed audio-only display.
