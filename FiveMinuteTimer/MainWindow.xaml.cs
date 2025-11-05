using System;
using System.IO;
using System.Media;
using System.Text;
using System.Windows;
using System.Windows.Threading;

namespace FiveMinuteTimer;

public partial class MainWindow : Window
{
    private readonly DispatcherTimer _dispatcherTimer;
    private readonly TimeSpan _duration = TimeSpan.FromMinutes(5);
    private DateTime? _targetTimeUtc;
    private bool _isRunning;
    private bool _alarmPlaying;
    private MemoryStream? _alarmStream;
    private SoundPlayer? _soundPlayer;

    public MainWindow()
    {
        InitializeComponent();

        TimerText.Text = FormatTime(_duration);

        _dispatcherTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(200)
        };
        _dispatcherTimer.Tick += OnTick;

        Loaded += (_, _) => LoadAlarmSound();
        Closed += (_, _) => DisposeAlarmSound();
    }

    private void OnStartButtonClick(object sender, RoutedEventArgs e)
    {
        if (_isRunning)
        {
            return;
        }

        StopAlarmIfNeeded();

        _targetTimeUtc = DateTime.UtcNow + _duration;
        _dispatcherTimer.Start();
        _isRunning = true;
        StartButton.IsEnabled = false;
        UpdateRemaining(_duration);
    }

    private void OnTick(object? sender, EventArgs e)
    {
        if (_targetTimeUtc is null)
        {
            return;
        }

        var remaining = _targetTimeUtc.Value - DateTime.UtcNow;
        if (remaining <= TimeSpan.Zero)
        {
            TimerText.Text = "00:00";
            _dispatcherTimer.Stop();
            _isRunning = false;
            _targetTimeUtc = null;
            StartButton.IsEnabled = true;
            PlayAlarm();
            return;
        }

        UpdateRemaining(remaining);
    }

    private void UpdateRemaining(TimeSpan remaining)
    {
        if (remaining < TimeSpan.Zero)
        {
            remaining = TimeSpan.Zero;
        }

        TimerText.Text = FormatTime(remaining);
    }

    private static string FormatTime(TimeSpan timeSpan)
    {
        if (timeSpan < TimeSpan.Zero)
        {
            timeSpan = TimeSpan.Zero;
        }

        int totalSeconds = (int)Math.Floor(timeSpan.TotalSeconds);
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;
        return $"{minutes:00}:{seconds:00}";
    }

    private void PlayAlarm()
    {
        if (_alarmPlaying)
        {
            return;
        }

        if (_soundPlayer is not null)
        {
            try
            {
                _soundPlayer.PlayLooping();
                _alarmPlaying = true;
                return;
            }
            catch
            {
                // Fall through to system sound if playback fails.
            }
        }

        SystemSounds.Exclamation.Play();
    }

    private void StopAlarmIfNeeded()
    {
        if (!_alarmPlaying || _soundPlayer is null)
        {
            return;
        }

        try
        {
            _soundPlayer.Stop();
        }
        catch
        {
            // Ignore playback stop errors.
        }

        _alarmPlaying = false;
    }

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
            _soundPlayer = null;
            _alarmStream?.Dispose();
            _alarmStream = null;
        }
    }

    private void DisposeAlarmSound()
    {
        try
        {
            _soundPlayer?.Stop();
        }
        catch
        {
        }

        _soundPlayer?.Dispose();
        _alarmStream?.Dispose();
        _soundPlayer = null;
        _alarmStream = null;
        _alarmPlaying = false;
    }

    private static MemoryStream CreateAlarmSoundStream()
    {
        const int sampleRate = 44100;
        const short bitsPerSample = 16;
        const short channels = 1;
        const double durationSeconds = 3.0;
        const double frequency = 880.0;

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
                double envelope = Math.Min(1.0, time * 4.0); // quick fade in
                double sample = Math.Sin(2 * Math.PI * frequency * time) * 0.6 * envelope;
                short value = (short)(sample * short.MaxValue);
                writer.Write(value);
            }
        }

        stream.Position = 0;
        return stream;
    }
}
