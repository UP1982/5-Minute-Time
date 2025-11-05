# 5-Minute-Time

A minimal WPF countdown timer for Windows. Press **Start** and the app will count down from five minutes to zero. When the timer finishes an alarm tone plays so you know time is up. Press **Start** again to restart the countdown and stop the alarm.

## Building a single-file executable

The project file is configured to publish a Windows 10/11 ready, self-contained single executable.
You can produce it with:

```bash
dotnet publish FiveMinuteTimer/FiveMinuteTimer.csproj \
  -c Release \
  -r win-x64 \
  --self-contained true \
  -p:PublishSingleFile=true \
  -p:IncludeNativeLibrariesForSelfExtract=true
```

The resulting `.exe` can be found in `FiveMinuteTimer/bin/Release/net8.0-windows/win-x64/publish/`.
