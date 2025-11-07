# 5-Minute Time

A native Windows countdown overlay written in modern C++ and Win32 APIs. The
application immediately begins counting down from five minutes, stays on top of
other windows with a translucent background, and fills the screen with confetti
and an audible alert when the timer expires. Optional controls allow the timer
to be paused or restarted without interrupting its always-on-top behaviour.

## Features

- Large, high-contrast countdown text with a subtle drop shadow for readability.
- Semi-transparent, resizable window that defaults to the center of the screen
  and remains topmost while running.
- Optional pause/resume and reset buttons tucked into the lower-right corner.
- Animated, full-screen confetti celebration and audible system chime when the
  timer hits zero.
- Automatically closes a few seconds after the celebration finishes.
- Implemented entirely with Win32/GDI so the compiled executable has no runtime
  dependencies beyond the Windows system libraries.

## Build requirements

- Windows 10 or later
- [Microsoft Visual Studio](https://visualstudio.microsoft.com/) 2019 or newer
  with the "Desktop development with C++" workload
- CMake 3.20+

All required libraries (User32, GDI32, WinMM, Shcore) ship with the Windows SDK
included in Visual Studio.

## Configure and build

Open a "x64 Native Tools Command Prompt for VS" and run the following commands
from the repository root:

```bat
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The resulting standalone executable will be available at:

```
build\Release\FiveMinuteTimer.exe
```

Copy that file to any Windows computer to run the timer. No installers or
additional assets are required.

## Running the timer

Double-click the built `FiveMinuteTimer.exe`. The countdown starts immediately.
When the timer reaches zero, a system alert sound plays and the confetti overlay
spreads across the entire desktop for a few seconds before the application
exits on its own.

### Optional controls

- **Pause/Resume** – Toggles the countdown without resetting the remaining time.
- **Reset** – Returns the countdown to five minutes and restarts it.
- **Replay** – Appears after the celebration finishes and restarts the timer and
  celebration.

## Packaging tips

If you prefer to distribute a single file, you can simply share the
`FiveMinuteTimer.exe` built in Release mode. Because the project only relies on
system libraries, no additional DLLs or assets are needed.
