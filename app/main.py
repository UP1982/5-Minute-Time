"""Main entry point for the 5-minute countdown timer application.

This module defines a transparent, always-on-top window that immediately
begins counting down from five minutes. When the countdown finishes it
plays an alarm and launches a confetti overlay before closing the
application.

The application is written with PySide6 (Qt for Python) so it can be
packaged into a standalone Windows executable with PyInstaller.
"""

from __future__ import annotations

import io
import math
import random
import sys
import wave
from dataclasses import dataclass
from typing import List

from pathlib import Path

from PySide6.QtCore import (QCoreApplication, QElapsedTimer, QPointF, Qt,
                            QTemporaryFile, QTimer, QUrl)
from PySide6.QtGui import QColor, QFont, QPainter, QPaintEvent
from PySide6.QtMultimedia import QSoundEffect
from PySide6.QtWidgets import (QApplication, QGraphicsDropShadowEffect,
                               QHBoxLayout, QLabel, QPushButton, QSizeGrip,
                               QVBoxLayout, QWidget)

# Five minutes expressed in milliseconds. Using milliseconds gives us a
# smooth, accurate timer with no noticeable drift.
FIVE_MINUTES_MS = 5 * 60 * 1000


@dataclass
class ConfettiPiece:
    """Simple data container that tracks a single confetti particle."""

    position: QPointF
    velocity: QPointF
    color: QColor
    size: float
    rotation: float
    rotation_speed: float


class ConfettiOverlay(QWidget):
    """Full-screen celebratory overlay that shows animated confetti."""

    def __init__(self, duration_ms: int = 4000) -> None:
        super().__init__()

        # Make the window cover the entire desktop and stay above other
        # windows. ``Qt.WindowTransparentForInput`` keeps the overlay from
        # grabbing mouse/keyboard input so the user can continue interacting
        # with whatever is beneath it.
        flags = Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
        if hasattr(Qt, "WindowTransparentForInput"):
            flags |= Qt.WindowTransparentForInput
        self.setWindowFlags(flags)
        self.setAttribute(Qt.WA_TranslucentBackground)

        self._duration_ms = duration_ms
        self._elapsed = 0
        self._update_timer = QTimer(self)
        self._update_timer.timeout.connect(self._update_frame)
        self._update_timer.start(16)  # ~60 FPS animation.

        self._pieces: List[ConfettiPiece] = []
        self._spawn_confetti(350)

    def _spawn_confetti(self, amount: int) -> None:
        """Create a batch of confetti particles with random attributes."""

        screen_geometry = QApplication.primaryScreen().geometry()
        for _ in range(amount):
            x = random.uniform(0, screen_geometry.width())
            y = random.uniform(-screen_geometry.height(), 0)
            size = random.uniform(6, 14)
            velocity = QPointF(
                random.uniform(-30, 30) / 100.0,  # Gentle horizontal drift
                random.uniform(120, 260) / 100.0,  # Vertical fall speed
            )
            rotation = random.uniform(0, 360)
            rotation_speed = random.uniform(-180, 180) / 100.0
            color = QColor(
                random.randint(50, 255),
                random.randint(50, 255),
                random.randint(50, 255),
                220,
            )
            self._pieces.append(
                ConfettiPiece(
                    position=QPointF(x, y),
                    velocity=velocity,
                    color=color,
                    size=size,
                    rotation=rotation,
                    rotation_speed=rotation_speed,
                )
            )

    def _update_frame(self) -> None:
        """Advance the animation and close after the duration elapses."""

        self._elapsed += self._update_timer.interval()
        if self._elapsed >= self._duration_ms:
            self._update_timer.stop()
            self.close()
            return

        screen_geometry = QApplication.primaryScreen().geometry()
        for piece in self._pieces:
            piece.position.setX(piece.position.x() + piece.velocity.x())
            piece.position.setY(piece.position.y() + piece.velocity.y())
            piece.rotation += piece.rotation_speed

            # Wrap the piece back to the top if it falls past the bottom of
            # the screen so the animation feels dense.
            if piece.position.y() > screen_geometry.height():
                piece.position.setY(-random.uniform(10, screen_geometry.height() / 3))
                piece.position.setX(random.uniform(0, screen_geometry.width()))

        self.update()

    def paintEvent(self, event: QPaintEvent) -> None:  # noqa: N802 - Qt API
        """Draw semi-transparent confetti rectangles."""

        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)

        for piece in self._pieces:
            painter.save()
            painter.translate(piece.position)
            painter.rotate(piece.rotation)
            painter.setBrush(piece.color)
            painter.setPen(Qt.NoPen)
            painter.drawRect(-piece.size / 2, -piece.size / 2, piece.size, piece.size)
            painter.restore()

        painter.end()


class CountdownWindow(QWidget):
    """Primary floating timer window that drives the countdown."""

    def __init__(self) -> None:
        super().__init__()

        self.setWindowTitle("5 Minute Timer")
        self.setWindowFlags(
            Qt.Window
            | Qt.WindowStaysOnTopHint
            | Qt.FramelessWindowHint
            | Qt.WindowMinMaxButtonsHint
        )
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setMinimumSize(320, 180)

        # Center the window on launch. Qt will honour this size and allow
        # the user to resize afterwards.
        screen_geometry = QApplication.primaryScreen().geometry()
        self.resize(400, 220)
        self.move(
            screen_geometry.center().x() - self.width() // 2,
            screen_geometry.center().y() - self.height() // 2,
        )

        self._build_ui()
        self._configure_timers()
        self._configure_audio()

        # Immediately start the countdown without user interaction.
        self.reset_timer()
        self.start_timer()

    def _build_ui(self) -> None:
        """Create the visual components for the timer."""

        layout = QVBoxLayout(self)
        layout.setContentsMargins(24, 24, 24, 24)
        layout.setSpacing(12)

        self.timer_label = QLabel("05:00", self)
        font = QFont("Segoe UI", 72, QFont.Bold)
        self.timer_label.setFont(font)
        self.timer_label.setAlignment(Qt.AlignCenter)
        self.timer_label.setStyleSheet("color: white;")

        drop_shadow = QGraphicsDropShadowEffect(self)
        drop_shadow.setBlurRadius(25)
        drop_shadow.setOffset(0, 0)
        drop_shadow.setColor(QColor(0, 0, 0, 220))
        self.timer_label.setGraphicsEffect(drop_shadow)

        controls_layout = QHBoxLayout()
        controls_layout.setContentsMargins(0, 0, 0, 0)
        controls_layout.setSpacing(8)

        self.start_button = QPushButton("▶", self)
        self.start_button.setFixedSize(36, 36)
        self.start_button.clicked.connect(self.start_timer)
        self.start_button.setToolTip("Start / resume countdown")

        self.stop_button = QPushButton("⏸", self)
        self.stop_button.setFixedSize(36, 36)
        self.stop_button.clicked.connect(self.pause_timer)
        self.stop_button.setToolTip("Pause countdown")

        self.reset_button = QPushButton("⟲", self)
        self.reset_button.setFixedSize(36, 36)
        self.reset_button.clicked.connect(self.reset_timer)
        self.reset_button.setToolTip("Reset countdown to 5 minutes")

        for button in (self.start_button, self.stop_button, self.reset_button):
            button.setStyleSheet(
                "background-color: rgba(0, 0, 0, 120);"
                "border: 1px solid rgba(255, 255, 255, 120);"
                "color: white;"
                "border-radius: 6px;"
            )

        controls_layout.addWidget(self.start_button)
        controls_layout.addWidget(self.stop_button)
        controls_layout.addWidget(self.reset_button)

        layout.addWidget(self.timer_label, stretch=1, alignment=Qt.AlignCenter)

        footer_layout = QHBoxLayout()
        footer_layout.setContentsMargins(0, 0, 0, 0)
        footer_layout.setSpacing(8)
        footer_layout.addStretch(1)
        footer_layout.addLayout(controls_layout)

        self._size_grip = QSizeGrip(self)
        self._size_grip.setFixedSize(18, 18)
        footer_layout.addWidget(self._size_grip, 0, Qt.AlignBottom | Qt.AlignRight)

        layout.addLayout(footer_layout)

        # Allow the mouse to drag the window from anywhere in the empty
        # space so the user can reposition the overlay quickly.
        self._drag_position = None

    def paintEvent(self, event: QPaintEvent) -> None:  # noqa: N802 - Qt API
        """Render a semi-transparent rounded rectangle behind the content."""

        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setBrush(QColor(15, 15, 30, 180))
        painter.setPen(Qt.NoPen)
        painter.drawRoundedRect(self.rect().adjusted(0, 0, -1, -1), 24, 24)
        painter.end()

        super().paintEvent(event)

    def _configure_timers(self) -> None:
        """Set up timers for both the countdown and screen refresh."""

        self._countdown_timer = QTimer(self)
        self._countdown_timer.setInterval(100)
        self._countdown_timer.timeout.connect(self._update_countdown)

        self._elapsed_timer = QElapsedTimer()
        self._remaining_ms = FIVE_MINUTES_MS

    def _configure_audio(self) -> None:
        """Load the alarm sound that will play when the timer finishes."""

        self._alarm = QSoundEffect(self)
        self._alarm_file = QTemporaryFile(self)
        self._alarm_file.setAutoRemove(True)
        if not self._alarm_file.open():
            return

        alarm_wave = self._generate_alarm_wave()
        self._alarm_file.write(alarm_wave)
        self._alarm_file.flush()
        self._alarm_file.seek(0)

        self._alarm.setSource(QUrl.fromLocalFile(self._alarm_file.fileName()))
        self._alarm.setLoopCount(2)
        self._alarm.setVolume(0.75)

    def _generate_alarm_wave(self) -> bytes:
        """Create a short celebratory alarm tone as a WAV byte sequence."""

        sample_rate = 44100
        duration_seconds = 1.6
        frequency = 880
        amplitude = 0.65
        ramp_duration = 0.05  # Fade in/out to prevent clicks.

        total_samples = int(sample_rate * duration_seconds)
        ramp_samples = int(sample_rate * ramp_duration)

        samples = bytearray()
        for i in range(total_samples):
            # Linear fade in/out envelope for a cleaner sound.
            if i < ramp_samples:
                envelope = i / ramp_samples
            elif i > total_samples - ramp_samples:
                envelope = (total_samples - i) / ramp_samples
            else:
                envelope = 1.0

            value = math.sin(2 * math.pi * frequency * (i / sample_rate))
            sample = int(amplitude * envelope * value * 32767)
            samples.extend(sample.to_bytes(2, byteorder="little", signed=True))

        # Assemble the WAV container.
        with io.BytesIO() as buffer:
            with wave.open(buffer, "wb") as wav_file:
                wav_file.setnchannels(1)
                wav_file.setsampwidth(2)
                wav_file.setframerate(sample_rate)
                wav_file.writeframes(samples)
            return buffer.getvalue()

    # ------------------------------------------------------------------
    # Timer control methods
    # ------------------------------------------------------------------
    def start_timer(self) -> None:
        """Start the countdown if it is not already running."""

        if not self._countdown_timer.isActive():
            self._elapsed_timer.start()
            self._countdown_timer.start()

    def pause_timer(self) -> None:
        """Pause the countdown and record the remaining time."""

        if self._countdown_timer.isActive():
            self._countdown_timer.stop()
            self._remaining_ms = max(
                0, self._remaining_ms - self._elapsed_timer.elapsed()
            )
            self._elapsed_timer.invalidate()

    def reset_timer(self) -> None:
        """Reset the timer to exactly five minutes."""

        self._countdown_timer.stop()
        self._remaining_ms = FIVE_MINUTES_MS
        self._elapsed_timer.invalidate()
        self._update_label(self._remaining_ms)

    def _update_countdown(self) -> None:
        """Update the label with the remaining time and finish at zero."""

        elapsed = self._elapsed_timer.elapsed()
        remaining = max(0, self._remaining_ms - elapsed)
        self._update_label(remaining)

        if remaining <= 0:
            self._countdown_timer.stop()
            self._handle_completion()

    def _update_label(self, remaining_ms: int) -> None:
        """Format the remaining milliseconds as MM:SS for display."""

        seconds = math.ceil(remaining_ms / 1000)
        minutes_part = seconds // 60
        seconds_part = seconds % 60
        self.timer_label.setText(f"{minutes_part:02d}:{seconds_part:02d}")

    def _handle_completion(self) -> None:
        """Play the alarm, show confetti, and exit when finished."""

        self._update_label(0)
        self._alarm.play()

        self._confetti = ConfettiOverlay()
        self._confetti.showFullScreen()
        self._confetti.show()

        # Give the confetti enough time to finish before the app quits.
        QTimer.singleShot(4500, QApplication.instance().quit)

    # ------------------------------------------------------------------
    # Window interaction helpers
    # ------------------------------------------------------------------
    def mousePressEvent(self, event):  # noqa: D401, N802 - Qt API
        """Capture the offset when the user starts dragging the window."""

        if event.button() == Qt.LeftButton:
            self._drag_position = event.globalPosition().toPoint() - self.frameGeometry().topLeft()
            event.accept()

    def mouseMoveEvent(self, event):  # noqa: D401, N802 - Qt API
        """Move the window while the left mouse button is held down."""

        if self._drag_position is not None and event.buttons() & Qt.LeftButton:
            self.move(event.globalPosition().toPoint() - self._drag_position)
            event.accept()

    def mouseReleaseEvent(self, event):  # noqa: D401, N802 - Qt API
        """Stop dragging when the mouse button is released."""

        if event.button() == Qt.LeftButton:
            self._drag_position = None
            event.accept()


def main() -> int:
    """Launch the Qt application and display the countdown window."""

    # Enable high-DPI scaling so the timer looks sharp on all monitors
    # before the QApplication instance is constructed.
    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)

    # When packaged as a PyInstaller ``--onefile`` executable the Qt plugin
    # folder lives inside the temporary extraction directory (``_MEIPASS``).
    # Explicitly add it to the library path so multimedia backends required
    # by ``QSoundEffect`` can be located without shipping external files.
    if getattr(sys, "frozen", False):
        base_dir = Path(getattr(sys, "_MEIPASS", Path.cwd()))
        plugin_dir = base_dir / "PySide6" / "plugins"
        if plugin_dir.exists():
            QCoreApplication.addLibraryPath(str(plugin_dir))

    app = QApplication(sys.argv)

    window = CountdownWindow()
    window.show()

    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
