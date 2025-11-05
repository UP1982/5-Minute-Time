# 5-Minute-Time

A lightweight native Win32 countdown timer. Press **Start** and the window counts down from five minutes to zero. When time expires an alarm tone loops until you close the window or start a fresh countdown.

## Build (Visual Studio or MSVC)

1. Install the "Desktop development with C++" workload in Visual Studio 2022 (or the MSVC Build Tools).
2. Open a *x64 Native Tools* command prompt.
3. Configure and build with CMake:

```batch
cmake -S SimpleTimer -B build -A x64
cmake --build build --config Release
```

The resulting standalone executable will be located at `build/Release/SimpleTimer.exe`. You can copy this single file to another Windows 10/11 machine and run it without additional dependencies.
