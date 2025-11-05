using System;
using System.IO;
using System.Media;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Shapes;
using System.Windows.Threading;

namespace FiveMinuteTimer;

public partial class MainWindow : Window
{
    private readonly DispatcherTimer _tickTimer;
    private readonly Random _random = new();
    private readonly TimeSpan _defaultDuration = TimeSpan.FromMinutes(5);
    private TimeSpan _initialDuration;
    private DateTime? _targetEndTimeUtc;
    private TimeSpan _remaining;
    private bool _isRunning;
    private bool _completionHandled;
    private MemoryStream? _alarmStream;
    private SoundPlayer? _soundPlayer;

    public MainWindow()
    {
        InitializeComponent();

        _initialDuration = ParseDurationFromArgs() ?? _defaultDuration;
        _remaining = _initialDuration;

        TimerText.Text = FormatTime(_remaining);

        // Dispatcher timer drives UI updates while the absolute end time keeps the countdown accurate.
        _tickTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(200)
        };
        _tickTimer.Tick += OnTick;

        Loaded += OnLoaded;
        SizeChanged += (_, _) => SynchronizeConfettiCanvasSize();

        LoadAlarmSound();
        SynchronizeConfettiCanvasSize();
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        ResetCountdown();
    }

    private TimeSpan? ParseDurationFromArgs()
    {
        try
        {
            var args = Environment.GetCommandLineArgs();
            for (int i = 0; i < args.Length; i++)
            {
                var arg = args[i];
                if (arg.Equals("--minutes", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
                {
                    if (double.TryParse(args[i + 1], out double minutes) && minutes > 0)
                    {
                        return TimeSpan.FromMinutes(minutes);
                    }
                }
                else if (arg.StartsWith("--minutes=", StringComparison.OrdinalIgnoreCase))
                {
                    var value = arg[("--minutes=".Length)..];
                    if (double.TryParse(value, out double minutes) && minutes > 0)
                    {
                        return TimeSpan.FromMinutes(minutes);
                    }
                }
            }
        }
        catch
        {
            // Ignore parsing errors and fall back to the default duration.
        }

        return null;
    }

    // Preload a generated alarm tone so playback can start instantly without touching the file system.
    private void LoadAlarmSound()
    {
        try
        {
            _alarmStream = CreateAlarmSoundStream();
            _soundPlayer = new SoundPlayer(_alarmStream);
            _soundPlayer.Load();
        }
        catch
        {
            _alarmStream = null;
            _soundPlayer = null;
        }
    }

    // Build a simple PCM WAV tone on the fly so the project has no binary assets while
    // still delivering an uncompressed alarm sound for the celebration sequence.
    private static MemoryStream CreateAlarmSoundStream()
    {
        const int sampleRate = 44100;
        const short bitsPerSample = 16;
        const short channels = 1;
        const double durationSeconds = 2.5;
        const double baseFrequency = 880.0;

        int sampleCount = (int)(sampleRate * durationSeconds);
        short blockAlign = (short)(channels * (bitsPerSample / 8));
        int byteRate = sampleRate * blockAlign;
        int dataSize = sampleCount * blockAlign;

        var stream = new MemoryStream(44 + dataSize);
        using (var writer = new BinaryWriter(stream, Encoding.ASCII, leaveOpen: true))
        {
            writer.Write(Encoding.ASCII.GetBytes("RIFF"));
            writer.Write(36 + dataSize);
            writer.Write(Encoding.ASCII.GetBytes("WAVE"));
            writer.Write(Encoding.ASCII.GetBytes("fmt "));
            writer.Write(16);
            writer.Write((short)1);
            writer.Write(channels);
            writer.Write(sampleRate);
            writer.Write(byteRate);
            writer.Write(blockAlign);
            writer.Write(bitsPerSample);
            writer.Write(Encoding.ASCII.GetBytes("data"));
            writer.Write(dataSize);

            for (int i = 0; i < sampleCount; i++)
            {
                double time = i / (double)sampleRate;
                double envelope = Math.Max(0.0, 1.0 - (time / durationSeconds));
                double modulation = 1.0 + 0.1 * Math.Sin(2 * Math.PI * 4 * time);
                double sample = Math.Sin(2 * Math.PI * baseFrequency * modulation * time) * 0.85 * envelope;
                short value = (short)(sample * short.MaxValue);
                writer.Write(value);
            }
        }

        stream.Position = 0;
        return stream;
    }

    private void StartCountdown(TimeSpan? from = null)
    {
        if (_isRunning)
        {
            return;
        }

        if (_completionHandled)
        {
            _completionHandled = false;
        }

        if (from.HasValue)
        {
            _remaining = from.Value;
        }

        if (_remaining <= TimeSpan.Zero)
        {
            _remaining = _initialDuration;
        }

        _targetEndTimeUtc = DateTime.UtcNow + _remaining;
        _tickTimer.Start();
        _isRunning = true;
        UpdateTimerDisplay(_remaining);
    }

    private void StopCountdown()
    {
        if (!_isRunning)
        {
            return;
        }

        _remaining = GetRemainingTime();
        _tickTimer.Stop();
        _targetEndTimeUtc = null;
        _isRunning = false;
    }

    private void ResetCountdown()
    {
        _tickTimer.Stop();
        _targetEndTimeUtc = null;
        _completionHandled = false;
        _isRunning = false;
        _remaining = _initialDuration;
        UpdateTimerDisplay(_remaining);
        StartCountdown(_remaining);
    }

    private async void OnTick(object? sender, EventArgs e)
    {
        var remaining = GetRemainingTime();
        if (remaining <= TimeSpan.Zero)
        {
            TimerText.Text = "00:00";
            await HandleCountdownCompletedAsync();
            return;
        }

        _remaining = remaining;
        UpdateTimerDisplay(_remaining);
    }

    private TimeSpan GetRemainingTime()
    {
        if (_targetEndTimeUtc.HasValue)
        {
            var remaining = _targetEndTimeUtc.Value - DateTime.UtcNow;
            if (remaining < TimeSpan.Zero)
            {
                return TimeSpan.Zero;
            }

            return remaining;
        }

        return _remaining;
    }

    private void UpdateTimerDisplay(TimeSpan remaining)
    {
        var totalSeconds = (int)Math.Ceiling(remaining.TotalSeconds);
        if (totalSeconds < 0)
        {
            totalSeconds = 0;
        }

        var minutes = totalSeconds / 60;
        var seconds = totalSeconds % 60;
        TimerText.Text = $"{minutes:00}:{seconds:00}";
    }

    // When the timer hits zero we fire the alarm, launch confetti, then exit after a short celebration.
    private async System.Threading.Tasks.Task HandleCountdownCompletedAsync()
    {
        if (_completionHandled)
        {
            return;
        }

        _completionHandled = true;
        _tickTimer.Stop();
        _isRunning = false;
        _targetEndTimeUtc = null;
        PlayAlarmSound();
        StartConfettiAnimation();

        await System.Threading.Tasks.Task.Delay(TimeSpan.FromSeconds(4));

        Application.Current.Dispatcher.Invoke(Application.Current.Shutdown);
    }

    // Use the generated WAV alarm, falling back to a system sound if the synthesis fails.
    private void PlayAlarmSound()
    {
        try
        {
            if (_soundPlayer is not null && _alarmStream is not null)
            {
                _alarmStream.Position = 0;
                _soundPlayer.Play();
                return;
            }
        }
        catch
        {
            // Fall back to a system sound if playback fails.
        }

        SystemSounds.Exclamation.Play();
    }

    // Generates a burst of colourful confetti pieces that fall across the window using WPF animations.
    private void StartConfettiAnimation()
    {
        ConfettiCanvas.Children.Clear();
        SynchronizeConfettiCanvasSize();

        double width = ConfettiCanvas.ActualWidth;
        double height = ConfettiCanvas.ActualHeight;
        if (width <= 0 || height <= 0)
        {
            width = ActualWidth;
            height = ActualHeight;
        }

        int pieceCount = Math.Max(120, (int)(width / 3));

        for (int i = 0; i < pieceCount; i++)
        {
            var size = _random.Next(8, 20);
            Shape shape = _random.NextDouble() > 0.5
                ? new Rectangle { Width = size, Height = size * 0.6 }
                : CreateTriangle(size);

            shape.Fill = new SolidColorBrush(GetRandomConfettiColor());
            shape.RenderTransformOrigin = new Point(0.5, 0.5);

            var transforms = new TransformGroup();
            var rotate = new RotateTransform();
            var translate = new TranslateTransform();
            transforms.Children.Add(rotate);
            transforms.Children.Add(translate);
            shape.RenderTransform = transforms;

            double startX = _random.NextDouble() * width;
            double startYOffset = _random.NextDouble() * 200 + 40;
            double drift = (_random.NextDouble() - 0.5) * width * 0.25;
            double durationSeconds = 3.0 + _random.NextDouble() * 2.0;

            Canvas.SetLeft(shape, startX);
            Canvas.SetTop(shape, -startYOffset);
            ConfettiCanvas.Children.Add(shape);

            var fallAnimation = new DoubleAnimation
            {
                From = 0,
                To = height + startYOffset,
                Duration = TimeSpan.FromSeconds(durationSeconds),
                AccelerationRatio = 0.1,
                DecelerationRatio = 0.2,
            };

            var driftAnimation = new DoubleAnimation
            {
                From = 0,
                To = drift,
                Duration = TimeSpan.FromSeconds(durationSeconds),
            };

            var rotationAnimation = new DoubleAnimation
            {
                From = 0,
                To = (_random.NextDouble() > 0.5 ? 720 : -720),
                Duration = TimeSpan.FromSeconds(durationSeconds / 2),
                RepeatBehavior = RepeatBehavior.Forever
            };

            translate.BeginAnimation(TranslateTransform.YProperty, fallAnimation);
            translate.BeginAnimation(TranslateTransform.XProperty, driftAnimation);
            rotate.BeginAnimation(RotateTransform.AngleProperty, rotationAnimation);
        }
    }

    private static Polygon CreateTriangle(double size)
    {
        var triangle = new Polygon
        {
            Points = new PointCollection
            {
                new Point(0, size),
                new Point(size / 2, 0),
                new Point(size, size)
            }
        };

        return triangle;
    }

    private Color GetRandomConfettiColor()
    {
        Color[] palette =
        {
            Color.FromRgb(255, 99, 132),
            Color.FromRgb(54, 162, 235),
            Color.FromRgb(255, 206, 86),
            Color.FromRgb(75, 192, 192),
            Color.FromRgb(153, 102, 255),
            Color.FromRgb(255, 159, 64)
        };

        return palette[_random.Next(palette.Length)];
    }

    private void SynchronizeConfettiCanvasSize()
    {
        ConfettiCanvas.Width = ActualWidth;
        ConfettiCanvas.Height = ActualHeight;
    }

    private void MainPanel_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ButtonState == MouseButtonState.Pressed)
        {
            try
            {
                DragMove();
            }
            catch
            {
                // DragMove throws when initiated during window closing; ignore.
            }
        }
    }

    private void StartButton_Click(object sender, RoutedEventArgs e) => StartCountdown();

    private void StopButton_Click(object sender, RoutedEventArgs e) => StopCountdown();

    private void ResetButton_Click(object sender, RoutedEventArgs e) => ResetCountdown();

    private void CloseButton_Click(object sender, RoutedEventArgs e) => Close();
}
