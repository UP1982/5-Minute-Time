#include "ConfettiWindow.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
constexpr LPCWSTR kClassName = L"ConfettiWindowClass";
constexpr UINT_PTR kAnimationTimerId = 1;
constexpr UINT kAnimationIntervalMs = 16;

ATOM EnsureClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.hInstance = instance;
    wc.lpfnWndProc = &ConfettiWindow::WindowProc;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.style = CS_HREDRAW | CS_VREDRAW;

    atom = RegisterClassExW(&wc);
    return atom;
}

COLORREF RandomColor(std::mt19937 &rng) {
    std::uniform_int_distribution<int> dist(0, 255);
    BYTE r = static_cast<BYTE>(dist(rng));
    BYTE g = static_cast<BYTE>(dist(rng));
    BYTE b = static_cast<BYTE>(dist(rng));
    return RGB(r, g, b);
}

float RandomFloat(std::mt19937 &rng, float minValue, float maxValue) {
    std::uniform_real_distribution<float> dist(minValue, maxValue);
    return dist(rng);
}
}  // namespace

ConfettiWindow::ConfettiWindow(HINSTANCE instance) : m_instance(instance) {}

ConfettiWindow::~ConfettiWindow() {
    Hide();
}

bool ConfettiWindow::Create() {
    if (!EnsureClassRegistered(m_instance)) {
        return false;
    }

    m_screenRect.left = 0;
    m_screenRect.top = 0;
    m_screenRect.right = GetSystemMetrics(SM_CXSCREEN);
    m_screenRect.bottom = GetSystemMetrics(SM_CYSCREEN);

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        kClassName,
        L"Confetti",
        WS_POPUP,
        m_screenRect.left,
        m_screenRect.top,
        m_screenRect.right - m_screenRect.left,
        m_screenRect.bottom - m_screenRect.top,
        nullptr,
        nullptr,
        m_instance,
        this);

    if (!m_hwnd) {
        return false;
    }

    return true;
}

void ConfettiWindow::Show() {
    if (!m_hwnd) {
        return;
    }

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
}

void ConfettiWindow::Hide() {
    if (m_hwnd) {
        KillTimer(m_hwnd, kAnimationTimerId);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

LRESULT CALLBACK ConfettiWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ConfettiWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto createStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        self = reinterpret_cast<ConfettiWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<ConfettiWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
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
        return 0;
    case WM_PAINT:
        self->OnPaint();
        return 0;
    case WM_TIMER:
        self->OnTimer(static_cast<UINT_PTR>(wParam));
        return 0;
    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ConfettiWindow::OnCreate(HWND hwnd) {
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    InitializePieces();
    m_lastTick = GetTickCount64();
    SetTimer(hwnd, kAnimationTimerId, kAnimationIntervalMs, nullptr);
}

void ConfettiWindow::OnDestroy() {
    m_pieces.clear();
}

void ConfettiWindow::OnPaint() {
    if (!m_hwnd) {
        return;
    }

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rect = m_screenRect;
    FillRect(hdc, &rect, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    for (const auto& piece : m_pieces) {
        RECT r;
        r.left = static_cast<LONG>(piece.x);
        r.top = static_cast<LONG>(piece.y);
        r.right = static_cast<LONG>(piece.x + piece.size);
        r.bottom = static_cast<LONG>(piece.y + piece.size * 1.5f);

        HBRUSH brush = CreateSolidBrush(piece.color);
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
        RoundRect(hdc, r.left, r.top, r.right, r.bottom, 6, 6);
        SelectObject(hdc, oldBrush);
        DeleteObject(brush);
    }

    EndPaint(m_hwnd, &ps);
}

void ConfettiWindow::OnTimer(UINT_PTR timerId) {
    if (timerId != kAnimationTimerId) {
        return;
    }

    ULONGLONG now = GetTickCount64();
    float delta = static_cast<float>(now - m_lastTick) / 1000.0f;
    m_lastTick = now;
    AdvancePieces(delta);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ConfettiWindow::AdvancePieces(float deltaSeconds) {
    const float width = static_cast<float>(m_screenRect.right - m_screenRect.left);
    const float height = static_cast<float>(m_screenRect.bottom - m_screenRect.top);

    for (auto& piece : m_pieces) {
        piece.x += piece.vx * deltaSeconds;
        piece.y += piece.vy * deltaSeconds;

        if (piece.x < 0.0f) {
            piece.x += width;
        } else if (piece.x > width) {
            piece.x -= width;
        }

        if (piece.y > height) {
            piece.y = -piece.size;
            piece.x = std::fmod(piece.x + width, width);
        }
    }
}

void ConfettiWindow::InitializePieces() {
    std::mt19937 rng(static_cast<unsigned int>(GetTickCount()));
    const float width = static_cast<float>(m_screenRect.right - m_screenRect.left);
    const float height = static_cast<float>(m_screenRect.bottom - m_screenRect.top);

    std::uniform_real_distribution<float> xDist(0.0f, width);
    std::uniform_real_distribution<float> yDist(0.0f, height);

    constexpr size_t kPieceCount = 220;
    m_pieces.clear();
    m_pieces.reserve(kPieceCount);

    for (size_t i = 0; i < kPieceCount; ++i) {
        ConfettiPiece piece{};
        piece.x = xDist(rng);
        piece.y = yDist(rng);
        piece.vx = RandomFloat(rng, -40.0f, 40.0f);
        piece.vy = RandomFloat(rng, 120.0f, 260.0f);
        piece.size = RandomFloat(rng, 6.0f, 16.0f);
        piece.color = RandomColor(rng);
        m_pieces.push_back(piece);
    }
}
