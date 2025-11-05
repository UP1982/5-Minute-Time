# 5-Minute-Time

Five Minute Timer is a lightweight WPF countdown utility that instantly launches a five-minute timer in an always-on-top, semi-transparent window. It includes start/stop controls, a synthesized alarm tone (generated at runtime to avoid shipping binary assets), and a celebratory confetti animation when time runs out.

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
