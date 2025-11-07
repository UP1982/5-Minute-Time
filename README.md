# 5-Minute Time

A transparent, always-on-top countdown timer that immediately starts from five
minutes, plays an alarm, and fires a celebratory confetti animation when it
reaches zero. The project is built with [PySide6](https://www.qt.io/qt-for-python)
and can be packaged into a standalone Windows executable with PyInstaller.

## Features

- Large, easy-to-read timer overlay with drop shadow for visibility on any
  background.
- Optional play/pause/reset controls tucked in the lower-right corner.
- Semi-transparent rounded background that keeps the desktop visible.
- Confetti celebration overlay and looping alarm tone when time expires.
- Resizable window with a built-in size grip and drag-to-move support.

The alarm tone is synthesized at runtime, so no external audio files are
required when packaging or distributing the application.

## Prerequisites

- Python 3.9+
- `pip` for installing dependencies

Install the runtime dependencies:

```bash
pip install -r requirements.txt
```

## Running the timer

```bash
python -m app.main
```

The countdown starts immediately, keeps the window on top of other
applications, and exits automatically after the confetti animation finishes.

## Building a standalone Windows executable

1. Install PyInstaller (only needs to be done once):

   ```bash
   pip install pyinstaller
   ```

2. Generate the executable. Run this from the project root on Windows. The
   ``--onefile`` flag bundles everything into a single ``.exe`` and
   ``--collect-all PySide6`` ensures the required Qt multimedia plugins ship
   inside that executable so the alarm tone plays correctly:

   ```bash
   pyinstaller app/main.py \
     --name "5MinuteTimer" \
     --noconsole \
     --onefile \
     --collect-all PySide6
   ```

3. The compiled `5MinuteTimer.exe` will appear in the `dist/` directory. Copy
   that file anywhere to launch the timer on another Windows machine—no
   additional dependencies required.

