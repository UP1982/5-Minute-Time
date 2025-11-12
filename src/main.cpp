#include "CountdownWindow.h"

#include <shellscalingapi.h>
#include <windows.h>

using SetDpiAwareContextFunc = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);

void EnablePerMonitorDpiAwareness() {
    HMODULE user32 = LoadLibraryW(L"User32.dll");
    if (!user32) {
        return;
    }

    auto setProcessDpiAwarenessContext =
        reinterpret_cast<SetDpiAwareContextFunc>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (setProcessDpiAwarenessContext) {
        setProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    FreeLibrary(user32);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    EnablePerMonitorDpiAwareness();

    CountdownWindow window(hInstance);
    if (!window.Create()) {
        MessageBoxW(nullptr, L"Unable to initialize the timer window.", L"5-Minute Timer", MB_ICONERROR | MB_OK);
        return -1;
    }

    window.Show(nCmdShow == 0 ? SW_SHOWDEFAULT : nCmdShow);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}
