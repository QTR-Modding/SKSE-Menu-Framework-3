#include "WindowManager.h"
#include "Event.h"

Window::Window() {
	Interface = new WindowInterface();
    Render = nullptr;
}


bool WindowManager::IsAnyWindowOpen() {
    auto it = std::find_if(WindowManager::Windows.begin(), WindowManager::Windows.end(),
                           [](Window* x) { return x->Interface->IsOpen.load(); });
    return it != WindowManager::Windows.end();
}


bool WindowManager::ShouldTheGameBePaused() {
    auto it = std::find_if(WindowManager::Windows.begin(), WindowManager::Windows.end(),
                           [](Window* x) { return x->Interface->IsOpen.load() && x->Interface->BlockUserInput.load();  });
    return it != WindowManager::Windows.end();
}

bool WindowManager::ConsumeBlockingWindowOpened() {
    bool blockingWindowOpened = false;

    for (const auto window : Windows) {
        const bool isBlockingOpen =
            window->Interface->IsOpen.load() && window->Interface->BlockUserInput.load();
        blockingWindowOpened |= isBlockingOpen && !window->WasBlockingOpen;
        window->WasBlockingOpen = isBlockingOpen;
    }

    return blockingWindowOpened;
}

void WindowManager::Close() {
    WindowManager::MainInterface->BlockUserInput = true;
    WindowManager::ConfigInterface->BlockUserInput = true;
    ConfigInterface->IsOpen = false;
    MainInterface->IsOpen = false;
    Event::DispatchEvent(Event::EventType::kCloseMenu);
}

void WindowManager::Open() {
    MainInterface->IsOpen = true;
    Event::DispatchEvent(Event::EventType::kOpenMenu);
}
