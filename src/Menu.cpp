#include "Menu.h"

#include "Config.h"
#include "Event.h"
#include "HudManager.h"
#include "Input.h"
#include "Renderer.h"
#include "WindowManager.h"


SKSEMenuFrameworkMenu::SKSEMenuFrameworkMenu() {
    menuFlags.set(RE::UI_MENU_FLAGS::kRendersUnderPauseMenu);
    menuFlags.set(RE::UI_MENU_FLAGS::kAdvancesUnderPauseMenu);
    menuFlags.set(RE::UI_MENU_FLAGS::kAlwaysOpen);
    menuFlags.set(RE::UI_MENU_FLAGS::kAllowSaving);
    menuFlags.set(RE::UI_MENU_FLAGS::kUsesCursor);

    inputContext = RE::UserEvents::INPUT_CONTEXT_ID::kNone;  // We handle our own input
    depthPriority = 11;                                      // Console - 1

    RE::BSInputDeviceManager::GetSingleton()->AddEventSink(this);
}

SKSEMenuFrameworkMenu::~SKSEMenuFrameworkMenu() { RE::BSInputDeviceManager::GetSingleton()->RemoveEventSink(this); }

RE::BSEventNotifyControl SKSEMenuFrameworkMenu::ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) {
    if (!a_events) {
        return RE::BSEventNotifyControl::kContinue;
    }

    if (WindowManager::IsAnyWindowOpen())
    {
        UI::TranslateInputEvent(a_events);
    }

    for (RE::InputEvent* event = *a_events; event; event = event->next) {
        if (!event) {
			continue;
		}

        for (auto& [id, callback] : inputCallbacks) {
            callback(event);
        }

        // Handle the toggle key for the menu
        if (event->eventType.get() == RE::INPUT_EVENT_TYPE::kButton) {
            RE::INPUT_DEVICE device = event->device.get();
            RE::ButtonEvent* btn = static_cast<RE::ButtonEvent*>(event);
            const bool isKeyboard = device == RE::INPUT_DEVICE::kKeyboard;

            if (isKeyboard && btn->GetIDCode() == REX::W32::DIK_ESCAPE && btn->IsDown()) {
                if (WindowManager::IsAnyWindowOpen()) { WindowManager::Close(); }
                continue;
            }

            const uint32_t toggleKey = isKeyboard ? Config::ToggleKey : Config::ToggleKeyGamePad;
            if (btn->GetIDCode() != toggleKey) {
                continue;
            }

            if (WindowManager::MainInterface->IsOpen.load() && btn->IsDown()) {
                WindowManager::Close();
                return RE::BSEventNotifyControl::kContinue;
            }

            const uint8_t mode = isKeyboard ? Config::ToggleMode : Config::ToggleModeGamePad;
            DoublePressDetector& doublePressDetector =
                isKeyboard ? DoublePressDetectorKeyboard : DoublePressDetectorGamepad;

            if (mode == 0 && btn->IsDown() || mode == 1 && btn->HeldDuration() > 0.4f) {
                WindowManager::Open();
            } else if (mode == 2 && btn->IsDown()) {
                doublePressDetector.press();
                if (doublePressDetector) {
                    WindowManager::Open();
                }
            }
        }
    }
    return RE::BSEventNotifyControl::kContinue;
}

RE::UI_MESSAGE_RESULTS SKSEMenuFrameworkMenu::ProcessMessage(RE::UIMessage& a_message) {
    // Consume Scaleform events if we have a window open, so they don't get sent to lower menus
    if (a_message.type == RE::UI_MESSAGE_TYPE::kScaleformEvent && WindowManager::IsAnyWindowOpen()) {
        return RE::UI_MESSAGE_RESULTS::kHandled;
    }

    // Do not handle messages for other menus
    if (a_message.menu != MENU_NAME) {
        return RE::UI_MESSAGE_RESULTS::kPassOn;
    }

    // Never close this menu
    if (a_message.type == RE::UI_MESSAGE_TYPE::kHide) {
        return RE::UI_MESSAGE_RESULTS::kIgnore;
    }

    // Handle all other messages that were addressed to this menu
    return RE::UI_MESSAGE_RESULTS::kHandled;
}

void SKSEMenuFrameworkMenu::PostDisplay() {
    if (!UI::Renderer::initialized.load()) {
        return;
    }

    UI::Renderer::ImGuiBegin();
    HudManager::Render();
    if (WindowManager::IsAnyWindowOpen()) {
        UI::Renderer::RenderWindows();
    }
    UI::Renderer::ImGuiEnd();
}

void SKSEMenuFrameworkMenu::AdvanceMovie(float, std::uint32_t) {
    bool anyBlocking = WindowManager::IsAnyWindowOpen();
    bool hasCursor = menuFlags.all(RE::UI_MENU_FLAGS::kUsesCursor);

    if (anyBlocking != hasCursor) {
        RE::UI* ui = RE::UI::GetSingleton();
        RE::UIMessageQueue* msgQueue = RE::UIMessageQueue::GetSingleton();

        bool gamepad = RE::BSInputDeviceManager::GetSingleton()->IsGamepadEnabled();

        if (anyBlocking && !gamepad) {
            menuFlags.set(RE::UI_MENU_FLAGS::kUsesCursor);
            RE::UI_MESSAGE_TYPE msgType = ui->IsMenuOpen(RE::CursorMenu::MENU_NAME) ? RE::UI_MESSAGE_TYPE::kUpdate : RE::UI_MESSAGE_TYPE::kShow;
            msgQueue->AddMessage(RE::CursorMenu::MENU_NAME, msgType, nullptr);
        }
        else
        {
            menuFlags.reset(RE::UI_MENU_FLAGS::kUsesCursor);
            int32_t otherCursorMenus = 0;
            for (const RE::GPtr<RE::IMenu>& menu : ui->menuStack) {
                if (menu.get() != this && menu->menuFlags.all(RE::UI_MENU_FLAGS::kUsesCursor)) {
                    otherCursorMenus++;
                }
            }
            if (otherCursorMenus == 0) {
                msgQueue->AddMessage(RE::CursorMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
            }
        }
    }

    bool wantBlur = anyBlocking && Config::BlurBackgroundOnMenu;
    if (wantBlur != isBlurActive) {
        if (wantBlur) {
            RE::UIBlurManager::GetSingleton()->IncrementBlurCount();
        } else {
            RE::UIBlurManager::GetSingleton()->DecrementBlurCount();
        }
        isBlurActive = wantBlur;
    }

    bool wantPause = anyBlocking && Config::FreezeTimeOnMenu;
    bool hasPauseFlag = menuFlags.all(RE::UI_MENU_FLAGS::kPausesGame);
    if (wantPause != hasPauseFlag) {
        if (wantPause) {
            menuFlags.set(RE::UI_MENU_FLAGS::kPausesGame);
            RE::UI::GetSingleton()->numPausesGame++;
        } else {
            menuFlags.reset(RE::UI_MENU_FLAGS::kPausesGame);
            RE::UI::GetSingleton()->numPausesGame--;
        }
    }
}

void SKSEMenuFrameworkMenu::Register() { RE::UI::GetSingleton()->Register(MENU_NAME, Create); }

int64_t SKSEMenuFrameworkMenu::RegisterInputEvent(InputEventCallback callback) {
    uint64_t id = nextCallbackId++;
    inputCallbacks[id] = std::move(callback);
    return static_cast<int64_t>(id);
}

void SKSEMenuFrameworkMenu::UnregisterInputEvent(uint64_t id) { inputCallbacks.erase(id); }

void SKSEMenuFrameworkMenu::Open() {
    RE::UIMessageQueue::GetSingleton()->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
}
