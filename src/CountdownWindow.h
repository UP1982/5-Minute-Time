#pragma once

#include <windows.h>
#include <memory>

class ConfettiWindow;

class CountdownWindow {
public:
    explicit CountdownWindow(HINSTANCE instance);
    ~CountdownWindow();

    bool Create();
    void Show(int cmdShow);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCreate(HWND hwnd);
    void OnDestroy();
    void OnPaint();
    void OnTimer(UINT_PTR timerId);
    void OnCommand(int commandId);
    void OnSize(int width, int height);

    void Start();
    void Pause();
    void Reset();
    void ToggleRunning();
    void TriggerCompletion();
    void UpdateButtonStates();
    void UpdateWindowTitle();

    std::wstring FormatRemainingTime() const;

    HINSTANCE m_instance;
    HWND m_hwnd{nullptr};
    HWND m_startPauseButton{nullptr};
    HWND m_resetButton{nullptr};
    HFONT m_timerFont{nullptr};
    int m_remainingSeconds{300};
    bool m_running{false};
    bool m_completed{false};
    std::unique_ptr<ConfettiWindow> m_confetti;
};
