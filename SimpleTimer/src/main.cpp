#include <windows.h>
#include <mmsystem.h>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <limits>
#include <numbers>
#include <string>
#include <vector>

#pragma comment(lib, "winmm.lib")

namespace {
constexpr int kCountdownSeconds = 5 * 60;
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerIntervalMs = 100;

HWND g_timeLabel = nullptr;
HWND g_startButton = nullptr;
bool g_running = false;
bool g_alarmPlaying = false;
std::chrono::steady_clock::time_point g_endTime;
HFONT g_timeFont = nullptr;
HFONT g_buttonFont = nullptr;
std::vector<BYTE> g_alarmBuffer;

#pragma pack(push, 1)
struct WavHeader {
    char riff[4];
    std::uint32_t overallSize;
    char wave[4];
    char fmtChunkMarker[4];
    std::uint32_t fmtLength;
    std::uint16_t audioFormat;
    std::uint16_t numChannels;
    std::uint32_t sampleRate;
    std::uint32_t byteRate;
    std::uint16_t blockAlign;
    std::uint16_t bitsPerSample;
    char dataChunkHeader[4];
    std::uint32_t dataSize;
};
#pragma pack(pop)

void EnsureAlarmBuffer()
{
    if (!g_alarmBuffer.empty()) {
        return;
    }

    constexpr int sampleRate = 44100;
    constexpr double frequency = 880.0;
    constexpr double amplitude = 0.5;
    constexpr int durationSeconds = 1;
    const int totalSamples = sampleRate * durationSeconds;
    const std::uint32_t dataSize = totalSamples * sizeof(std::int16_t);

    WavHeader header{};
    std::memcpy(header.riff, "RIFF", 4);
    header.overallSize = 36 + dataSize;
    std::memcpy(header.wave, "WAVE", 4);
    std::memcpy(header.fmtChunkMarker, "fmt ", 4);
    header.fmtLength = 16;
    header.audioFormat = 1;
    header.numChannels = 1;
    header.sampleRate = sampleRate;
    header.bitsPerSample = 16;
    header.byteRate = sampleRate * header.numChannels * header.bitsPerSample / 8;
    header.blockAlign = header.numChannels * header.bitsPerSample / 8;
    std::memcpy(header.dataChunkHeader, "data", 4);
    header.dataSize = dataSize;

    g_alarmBuffer.resize(sizeof(WavHeader) + dataSize);
    std::memcpy(g_alarmBuffer.data(), &header, sizeof(WavHeader));
    auto* sampleData = reinterpret_cast<std::int16_t*>(g_alarmBuffer.data() + sizeof(WavHeader));

    for (int i = 0; i < totalSamples; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double sample = std::sin(2.0 * std::numbers::pi * frequency * t);
        sampleData[i] = static_cast<std::int16_t>(sample * amplitude * std::numeric_limits<std::int16_t>::max());
    }
}

void StopAlarm()
{
    if (g_alarmPlaying) {
        PlaySoundW(nullptr, nullptr, 0);
        g_alarmPlaying = false;
    }
}

void StartAlarm()
{
    EnsureAlarmBuffer();
    if (!g_alarmBuffer.empty()) {
        PlaySoundW(reinterpret_cast<LPCWSTR>(g_alarmBuffer.data()), nullptr,
                   SND_ASYNC | SND_MEMORY | SND_LOOP);
        g_alarmPlaying = true;
    } else {
        MessageBeep(MB_ICONEXCLAMATION);
    }
}

std::wstring FormatRemaining(int totalSeconds)
{
    int clamped = totalSeconds < 0 ? 0 : totalSeconds;
    int minutes = clamped / 60;
    int seconds = clamped % 60;

    wchar_t buffer[16];
    std::swprintf(buffer, 16, L"%02d:%02d", minutes, seconds);
    return buffer;
}

void UpdateTimeLabel(int totalSeconds)
{
    SetWindowTextW(g_timeLabel, FormatRemaining(totalSeconds).c_str());
}

void StartCountdown(HWND hwnd)
{
    StopAlarm();
    g_endTime = std::chrono::steady_clock::now() + std::chrono::seconds(kCountdownSeconds);
    g_running = true;
    UpdateTimeLabel(kCountdownSeconds);
    KillTimer(hwnd, kTimerId);
    SetTimer(hwnd, kTimerId, kTimerIntervalMs, nullptr);
}

void HandleTimerTick(HWND hwnd)
{
    if (!g_running) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(g_endTime - now);

    if (remaining <= std::chrono::milliseconds::zero()) {
        UpdateTimeLabel(0);
        KillTimer(hwnd, kTimerId);
        g_running = false;
        StartAlarm();
        return;
    }

    auto rounded = std::chrono::duration_cast<std::chrono::seconds>(remaining + std::chrono::milliseconds(999));
    int seconds = static_cast<int>(rounded.count());
    UpdateTimeLabel(seconds);
}

void LayoutControls(HWND hwnd)
{
    RECT client{};
    GetClientRect(hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;

    int buttonHeight = 40;
    int buttonWidth = 100;
    int padding = 16;

    int timeHeight = height - buttonHeight - padding * 3;
    if (timeHeight < 80) {
        timeHeight = 80;
    }

    int labelTop = padding;
    int labelHeight = timeHeight;
    int labelWidth = width - padding * 2;
    if (labelWidth < 120) {
        labelWidth = 120;
    }

    MoveWindow(g_timeLabel,
               (width - labelWidth) / 2,
               labelTop,
               labelWidth,
               labelHeight,
               TRUE);

    MoveWindow(g_startButton,
               (width - buttonWidth) / 2,
               labelTop + labelHeight + padding,
               buttonWidth,
               buttonHeight,
               TRUE);
}

void Cleanup()
{
    StopAlarm();
    if (g_timeFont) {
        DeleteObject(g_timeFont);
        g_timeFont = nullptr;
    }
    if (g_buttonFont) {
        DeleteObject(g_buttonFont);
        g_buttonFont = nullptr;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE: {
        g_timeLabel = CreateWindowExW(0, L"STATIC", L"05:00",
                                      WS_CHILD | WS_VISIBLE | SS_CENTER,
                                      0, 0, 0, 0,
                                      hwnd, nullptr, nullptr, nullptr);

        g_startButton = CreateWindowExW(0, L"BUTTON", L"Start",
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        0, 0, 0, 0,
                                        hwnd, reinterpret_cast<HMENU>(1), nullptr, nullptr);

        LOGFONTW lf{};
        lf.lfHeight = -72;
        lf.lfWeight = FW_BOLD;
        lstrcpynW(lf.lfFaceName, L"Segoe UI", LF_FACESIZE);
        g_timeFont = CreateFontIndirectW(&lf);
        SendMessageW(g_timeLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g_timeFont), TRUE);

        LOGFONTW blf{};
        blf.lfHeight = -20;
        lstrcpynW(blf.lfFaceName, L"Segoe UI", LF_FACESIZE);
        g_buttonFont = CreateFontIndirectW(&blf);
        SendMessageW(g_startButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_buttonFont), TRUE);

        LayoutControls(hwnd);
        return 0;
    }
    case WM_SIZE:
        LayoutControls(hwnd);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == 1 && HIWORD(wParam) == BN_CLICKED) {
            StartCountdown(hwnd);
        }
        return 0;
    case WM_TIMER:
        if (wParam == kTimerId) {
            HandleTimerTick(hwnd);
        }
        return 0;
    case WM_DESTROY:
        Cleanup();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SimpleTimerWindow";

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"5-Minute Timer",
                                WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 360, 240,
                                nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) {
        MessageBoxW(nullptr, L"Failed to create window.", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
