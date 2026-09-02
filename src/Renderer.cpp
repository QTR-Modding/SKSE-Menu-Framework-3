#include "Renderer.h"
#include "WindowManager.h"
#include "Config.h"
#include "UI.h"
#include "imgui.h"
#include "Input.h"
#include "imgui_impl_dx11.h"
#include "Application.h"
#include "Input.h"
#include "Model.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "dxgi.h"

namespace {
    // Matches GetToggleMode() in Application.cpp: SinglePress/Hold/DoublePress/OFF.
    constexpr std::uint8_t kToggleModeOff = 3;
}


bool UI::Renderer::ProcessOpenClose(RE::InputEvent* const* evns) {
    if (!*evns) return false;

    for (RE::InputEvent* e = *evns; e; e = e->next) {
        if (e->eventType.get() != RE::INPUT_EVENT_TYPE::kButton) continue;
        const RE::ButtonEvent* a_event = e->AsButtonEvent();
        const auto temp_device = a_event->GetDevice();
        if (!IsSupportedDevice(temp_device)) continue;
        const auto temp_toggleKey = temp_device == RE::INPUT_DEVICE::kKeyboard ? Config::ToggleKey : Config::ToggleKeyGamePad;
        const auto temp_toggleMode =
            temp_device == RE::INPUT_DEVICE::kKeyboard ? Config::ToggleMode : Config::ToggleModeGamePad;
        // Mode OFF has to disable the whole binding. Closing used to be checked
        // before the mode was consulted, so an OFF toggle key could still close
        // the menu it was no longer allowed to open.
        if (hotkeyEnabled.load() && temp_toggleMode != kToggleModeOff &&
            a_event->GetIDCode() == temp_toggleKey) {

            if (WindowManager::MainInterface->IsOpen.load() && a_event->IsDown()) {
                WindowManager::Close();
                return true;
            } else {

                if (temp_device == RE::INPUT_DEVICE::kKeyboard) {
                    if (a_event->IsDown()) DoublePressDetectorKeyboard.press();

                    if (Config::ToggleMode == 0 && a_event->IsDown() ||
                        Config::ToggleMode == 1 && a_event->HeldDuration() > 0.4f||
                        Config::ToggleMode == 2 && DoublePressDetectorKeyboard && a_event->IsDown()) {
                        WindowManager::Open();
                        return true;
                    };
                } else {
                    if (a_event->IsDown()) DoublePressDetectorGamepad.press();
                    if (Config::ToggleModeGamePad == 0 && a_event->IsDown() ||
                        Config::ToggleModeGamePad == 1 && a_event->HeldDuration() > 0.4f ||
                        Config::ToggleModeGamePad == 2 && DoublePressDetectorGamepad && a_event->IsDown()) {
                        WindowManager::Open();
                        return true;
                    };
                }

            }

            if (Config::ConsumeToggleKey && a_event->IsDown()) {
                return true;
            }
        }

        // Escape closes on keyboard; B is the equivalent on a gamepad, and
        // without it a menu opened from somewhere other than the toggle key
        // (a tween menu entry, a mod event) has no controller exit at all.
        // Back unwinds one level at a time - edit, then popup, then page, then
        // the window - rather than dropping straight out of a submenu.
        if (temp_device == RE::INPUT_DEVICE::kGamepad &&
            a_event->GetIDCode() ==
                static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kB) &&
            a_event->IsDown() && WindowManager::MainInterface->IsOpen.load()) {
            switch (UI::ResolveBack()) {
                case UI::BackAction::PassToImGui:
                    break;  // do not consume: ImGui needs to see the press
                case UI::BackAction::PoppedPage:
                    return true;
                case UI::BackAction::CloseMenu:
                    WindowManager::Close();
                    return true;
            }
        }

        if (a_event->GetIDCode() == REX::W32::DIK_ESCAPE && temp_device == RE::INPUT_DEVICE::kKeyboard) {
            bool hasChanged = WindowManager::MainInterface->IsOpen.load();
            WindowManager::Close();
            return hasChanged;
        }
    }
    return false;
}



void UI::Renderer::RenderWindows() {
    for (const auto window : WindowManager::Windows) {
        if (window->Interface->IsOpen) {
            window->Render();
        }
    }
}

void UI::Renderer::install() {}

