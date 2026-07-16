#pragma once


typedef void(__stdcall* RenderFunction)();

class WindowInterface {
public:
    std::atomic<bool> IsOpen{false};
    std::atomic<bool> BlockUserInput{true};
};


class Window {
public:
    Window();
    WindowInterface* Interface;
    RenderFunction Render;
    bool WasBlockingOpen{false};
};


class WindowManager {
public:
    static inline std::vector<Window*> Windows;
    static inline WindowInterface* MainInterface;
    static inline WindowInterface* ConfigInterface;
    static bool IsAnyWindowOpen();
    static bool ShouldTheGameBePaused();
    static bool ConsumeBlockingWindowOpened();
    static void Close();
    static void Open();
};
