#include "CountdownWindow.h"

#include "ConfettiWindow.h"

#include <cwchar>
#include <mmsystem.h>
#include <string>
#include <utility>

#pragma comment(lib, "winmm.lib")

namespace {
constexpr LPCWSTR kWindowClassName = L"FiveMinuteTimerWindow";
constexpr UINT_PTR kCountdownTimerId = 1;
constexpr UINT_PTR kShutdownTimerId = 2;
constexpr UINT kCountdownIntervalMs = 1000;
constexpr int kDefaultSeconds = 300;
constexpr int kButtonPadding = 12;
constexpr int kButtonWidth = 90;
constexpr int kButtonHeight = 28;

ATOM EnsureClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.hInstance = instance;
    wc.lpfnWndProc = &CountdownWindow::WindowProc;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_HAND);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.style = CS_HREDRAW | CS_VREDRAW;

    atom = RegisterClassExW(&wc);
    return atom;
}
}  // namespace

CountdownWindow::CountdownWindow(HINSTANCE instance) : m_instance(instance) {}

CountdownWindow::~CountdownWindow() {
    if (m_timerFont) {
        DeleteObject(m_timerFont);
        m_timerFont = nullptr;
    }
}

bool CountdownWindow::Create() {
    if (!EnsureClassRegistered(m_instance)) {
        return false;
    }

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kWindowClassName,
        L"5-Minute Timer",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        420,
        240,
        nullptr,
        nullptr,
        m_instance,
        this);

    if (!m_hwnd) {
        return false;
    }

    return true;
}

void CountdownWindow::Show(int cmdShow) {
    if (!m_hwnd) {
        return;
    }
    ShowWindow(m_hwnd, cmdShow);
    UpdateWindow(m_hwnd);
}

LRESULT CALLBACK CountdownWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CountdownWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto createStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        self = reinterpret_cast<CountdownWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<CountdownWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!self) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
    case WM_CREATE:
        self->OnCreate(hwnd);
        return 0;
    case WM_DESTROY:
        self->OnDestroy();
        PostQuitMessage(0);
        return 0;
    case WM_PAINT:
        self->OnPaint();
        return 0;
    case WM_TIMER:
        self->OnTimer(static_cast<UINT_PTR>(wParam));
        return 0;
    case WM_COMMAND:
        self->OnCommand(LOWORD(wParam));
        return 0;
    case WM_SIZE:
        self->OnSize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_ERASEBKGND:
        // Prevent flicker by handling background painting in WM_PAINT.
        return 1;
    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void CountdownWindow::OnCreate(HWND hwnd) {
    SetLayeredWindowAttributes(hwnd, 0, static_cast<BYTE>(255 * 0.78f), LWA_ALPHA);

    RECT windowRect;
    GetClientRect(hwnd, &windowRect);

    RECT monitorRect;
    monitorRect.left = 0;
    monitorRect.top = 0;
    monitorRect.right = GetSystemMetrics(SM_CXSCREEN);
    monitorRect.bottom = GetSystemMetrics(SM_CYSCREEN);

    RECT currentRect;
    GetWindowRect(hwnd, &currentRect);
    int windowWidth = currentRect.right - currentRect.left;
    int windowHeight = currentRect.bottom - currentRect.top;
    int targetX = monitorRect.left + (monitorRect.right - windowWidth) / 2;
    int targetY = monitorRect.top + (monitorRect.bottom - windowHeight) / 2;
    SetWindowPos(hwnd, nullptr, targetX, targetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    HDC hdc = GetDC(hwnd);
    int logicalHeight = -MulDiv(120, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(hwnd, hdc);

    m_timerFont = CreateFontW(
        logicalHeight,
        0,
        0,
        0,
        FW_SEMIBOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        VARIABLE_PITCH,
        L"Segoe UI");

    const int clientWidth = windowRect.right - windowRect.left;
    const int clientHeight = windowRect.bottom - windowRect.top;

    m_startPauseButton = CreateWindowExW(
        0,
        L"BUTTON",
        L"Pause",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        clientWidth - (kButtonWidth + kButtonPadding),
        clientHeight - (kButtonHeight + kButtonPadding),
        kButtonWidth,
        kButtonHeight,
        hwnd,
        reinterpret_cast<HMENU>(1),
        m_instance,
        nullptr);

    m_resetButton = CreateWindowExW(
        0,
        L"BUTTON",
        L"Reset",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        clientWidth - (2 * kButtonWidth + 2 * kButtonPadding),
        clientHeight - (kButtonHeight + kButtonPadding),
        kButtonWidth,
        kButtonHeight,
        hwnd,
        reinterpret_cast<HMENU>(2),
        m_instance,
        nullptr);

    Start();
    UpdateButtonStates();
    UpdateWindowTitle();
}

void CountdownWindow::OnDestroy() {
    KillTimer(m_hwnd, kCountdownTimerId);
    KillTimer(m_hwnd, kShutdownTimerId);
    if (m_confetti) {
        m_confetti->Hide();
        m_confetti.reset();
    }
}

void CountdownWindow::OnSize(int width, int height) {
    if (!m_startPauseButton || !m_resetButton) {
        return;
    }

    SetWindowPos(
        m_startPauseButton,
        nullptr,
        width - (kButtonWidth + kButtonPadding),
        height - (kButtonHeight + kButtonPadding),
        0,
        0,
        SWP_NOSIZE | SWP_NOZORDER);

    SetWindowPos(
        m_resetButton,
        nullptr,
        width - (2 * kButtonWidth + 2 * kButtonPadding),
        height - (kButtonHeight + kButtonPadding),
        0,
        0,
        SWP_NOSIZE | SWP_NOZORDER);

    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void CountdownWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rect;
    GetClientRect(m_hwnd, &rect);

    HBRUSH background = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdc, &rect, background);
    DeleteObject(background);

    SetBkMode(hdc, TRANSPARENT);

    auto text = FormatRemainingTime();
    HFONT oldFont = nullptr;
    if (m_timerFont) {
        oldFont = static_cast<HFONT>(SelectObject(hdc, m_timerFont));
    }

    RECT shadowRect = rect;
    OffsetRect(&shadowRect, 3, 3);
    SetTextColor(hdc, RGB(0, 0, 0));
    DrawTextW(hdc, text.c_str(), static_cast<int>(text.length()), &shadowRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, RGB(240, 255, 255));
    DrawTextW(hdc, text.c_str(), static_cast<int>(text.length()), &rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (oldFont) {
        SelectObject(hdc, oldFont);
    }

    EndPaint(m_hwnd, &ps);
}

void CountdownWindow::OnTimer(UINT_PTR timerId) {
    if (timerId == kCountdownTimerId) {
        if (m_running && !m_completed) {
            if (m_remainingSeconds > 0) {
                --m_remainingSeconds;
                UpdateWindowTitle();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                if (m_remainingSeconds == 0) {
                    TriggerCompletion();
                }
            }
        }
    } else if (timerId == kShutdownTimerId) {
        KillTimer(m_hwnd, kShutdownTimerId);
        PostMessage(m_hwnd, WM_CLOSE, 0, 0);
    }
}

void CountdownWindow::OnCommand(int commandId) {
    switch (commandId) {
    case 1:
        ToggleRunning();
        break;
    case 2:
        Reset();
        break;
    default:
        break;
    }
}

void CountdownWindow::Start() {
    if (m_running || m_completed) {
        return;
    }
    m_running = true;
    SetTimer(m_hwnd, kCountdownTimerId, kCountdownIntervalMs, nullptr);
    UpdateButtonStates();
}

void CountdownWindow::Pause() {
    if (!m_running) {
        return;
    }
    m_running = false;
    KillTimer(m_hwnd, kCountdownTimerId);
    UpdateButtonStates();
}

void CountdownWindow::Reset() {
    Pause();
    m_remainingSeconds = kDefaultSeconds;
    m_completed = false;
    if (m_confetti) {
        m_confetti->Hide();
        m_confetti.reset();
    }
    InvalidateRect(m_hwnd, nullptr, TRUE);
    UpdateWindowTitle();
    Start();
}

void CountdownWindow::ToggleRunning() {
    if (m_completed) {
        Reset();
        return;
    }

    if (m_running) {
        Pause();
    } else {
        Start();
    }
}

void CountdownWindow::TriggerCompletion() {
    if (m_completed) {
        return;
    }

    m_completed = true;
    Pause();
    PlaySoundW(L"SystemAsterisk", nullptr, SND_ALIAS | SND_ASYNC);

    if (!m_confetti) {
        m_confetti = std::make_unique<ConfettiWindow>(m_instance);
        if (m_confetti->Create()) {
            m_confetti->Show();
        }
    }

    SetTimer(m_hwnd, kShutdownTimerId, 5000, nullptr);
}

void CountdownWindow::UpdateButtonStates() {
    if (!m_startPauseButton || !m_resetButton) {
        return;
    }

    if (m_completed) {
        SetWindowTextW(m_startPauseButton, L"Replay");
        EnableWindow(m_startPauseButton, TRUE);
        EnableWindow(m_resetButton, FALSE);
        return;
    }

    if (m_running) {
        SetWindowTextW(m_startPauseButton, L"Pause");
        EnableWindow(m_resetButton, FALSE);
    } else {
        SetWindowTextW(m_startPauseButton, L"Resume");
        EnableWindow(m_resetButton, TRUE);
    }
}

void CountdownWindow::UpdateWindowTitle() {
    auto text = L"5-Minute Timer - " + FormatRemainingTime();
    SetWindowTextW(m_hwnd, text.c_str());
}

std::wstring CountdownWindow::FormatRemainingTime() const {
    int minutes = m_remainingSeconds / 60;
    int seconds = m_remainingSeconds % 60;
    wchar_t buffer[16];
    swprintf_s(buffer, L"%02d:%02d", minutes, seconds);
    return buffer;
}
