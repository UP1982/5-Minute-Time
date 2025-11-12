#pragma once

#include <windows.h>
#include <vector>

struct ConfettiPiece {
    float x;
    float y;
    float vx;
    float vy;
    float size;
    COLORREF color;
};

class ConfettiWindow {
public:
    explicit ConfettiWindow(HINSTANCE instance);
    ~ConfettiWindow();

    bool Create();
    void Show();
    void Hide();
    bool IsVisible() const { return m_hwnd != nullptr && IsWindowVisible(m_hwnd); }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCreate(HWND hwnd);
    void OnDestroy();
    void OnPaint();
    void OnTimer(UINT_PTR timerId);
    void AdvancePieces(float deltaSeconds);
    void InitializePieces();

    HINSTANCE m_instance;
    HWND m_hwnd{nullptr};
    std::vector<ConfettiPiece> m_pieces;
    ULONGLONG m_lastTick{0};
    RECT m_screenRect{0, 0, 0, 0};
};
