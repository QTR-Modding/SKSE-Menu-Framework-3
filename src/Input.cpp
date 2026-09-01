#include "Input.h"
#include "Config.h"
#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>


ImGuiKey ParseKeyFromKeyboard(RE::BSKeyboardDevice::Key a_key) {
    switch (a_key) {
        // 512
        case RE::BSKeyboardDevice::Key::kTab:
            return ImGuiKey_Tab;
        case RE::BSKeyboardDevice::Key::kLeft:
            return ImGuiKey_LeftArrow;
        case RE::BSKeyboardDevice::Key::kRight:
            return ImGuiKey_RightArrow;
        case RE::BSKeyboardDevice::Key::kUp:
            return ImGuiKey_UpArrow;
        case RE::BSKeyboardDevice::Key::kDown:
            return ImGuiKey_DownArrow;
        case RE::BSKeyboardDevice::Key::kPageUp:
            return ImGuiKey_PageUp;
        case RE::BSKeyboardDevice::Key::kPageDown:
            return ImGuiKey_PageDown;
        case RE::BSKeyboardDevice::Key::kHome:
            return ImGuiKey_Home;
        case RE::BSKeyboardDevice::Key::kEnd:
            return ImGuiKey_End;
        case RE::BSKeyboardDevice::Key::kInsert:
            return ImGuiKey_Insert;
        case RE::BSKeyboardDevice::Key::kDelete:
            return ImGuiKey_Delete;
        case RE::BSKeyboardDevice::Key::kBackspace:
            return ImGuiKey_Backspace;
        case RE::BSKeyboardDevice::Key::kSpacebar:
            return ImGuiKey_Space;
        case RE::BSKeyboardDevice::Key::kEnter:
            return ImGuiKey_Enter;
        case RE::BSKeyboardDevice::Key::kEscape:
            return ImGuiKey_Escape;
        case RE::BSKeyboardDevice::Key::kLeftControl:
            return ImGuiKey_ModCtrl;
        case RE::BSKeyboardDevice::Key::kLeftShift:
            return ImGuiKey_ModShift;
        case RE::BSKeyboardDevice::Key::kLeftAlt:
            return ImGuiKey_ModAlt;
        case RE::BSKeyboardDevice::Key::kLeftWin:
            return ImGuiKey_ModSuper;
        case RE::BSKeyboardDevice::Key::kRightControl:
            return ImGuiKey_ModCtrl;
        case RE::BSKeyboardDevice::Key::kRightShift:
            return ImGuiKey_ModShift;
        case RE::BSKeyboardDevice::Key::kRightAlt:
            return ImGuiKey_ModAlt;
        case RE::BSKeyboardDevice::Key::kRightWin:
            return ImGuiKey_ModSuper;
        // Skip 535
        case RE::BSKeyboardDevice::Key::kNum0:
            return ImGuiKey_0;
        case RE::BSKeyboardDevice::Key::kNum1:
            return ImGuiKey_1;
        case RE::BSKeyboardDevice::Key::kNum2:
            return ImGuiKey_2;
        case RE::BSKeyboardDevice::Key::kNum3:
            return ImGuiKey_3;
        case RE::BSKeyboardDevice::Key::kNum4:
            return ImGuiKey_4;
        case RE::BSKeyboardDevice::Key::kNum5:
            return ImGuiKey_5;
        case RE::BSKeyboardDevice::Key::kNum6:
            return ImGuiKey_6;
        case RE::BSKeyboardDevice::Key::kNum7:
            return ImGuiKey_7;
        case RE::BSKeyboardDevice::Key::kNum8:
            return ImGuiKey_8;
        case RE::BSKeyboardDevice::Key::kNum9:
            return ImGuiKey_9;
        case RE::BSKeyboardDevice::Key::kA:
            return ImGuiKey_A;
        case RE::BSKeyboardDevice::Key::kB:
            return ImGuiKey_B;
        case RE::BSKeyboardDevice::Key::kC:
            return ImGuiKey_C;
        case RE::BSKeyboardDevice::Key::kD:
            return ImGuiKey_D;
        case RE::BSKeyboardDevice::Key::kE:
            return ImGuiKey_E;
        case RE::BSKeyboardDevice::Key::kF:
            return ImGuiKey_F;
        case RE::BSKeyboardDevice::Key::kG:
            return ImGuiKey_G;
        case RE::BSKeyboardDevice::Key::kH:
            return ImGuiKey_H;
        case RE::BSKeyboardDevice::Key::kI:
            return ImGuiKey_I;
        case RE::BSKeyboardDevice::Key::kJ:
            return ImGuiKey_J;
        case RE::BSKeyboardDevice::Key::kK:
            return ImGuiKey_K;
        case RE::BSKeyboardDevice::Key::kL:
            return ImGuiKey_L;
        case RE::BSKeyboardDevice::Key::kM:
            return ImGuiKey_M;
        case RE::BSKeyboardDevice::Key::kN:
            return ImGuiKey_N;
        case RE::BSKeyboardDevice::Key::kO:
            return ImGuiKey_O;
        case RE::BSKeyboardDevice::Key::kP:
            return ImGuiKey_P;
        case RE::BSKeyboardDevice::Key::kQ:
            return ImGuiKey_Q;
        case RE::BSKeyboardDevice::Key::kR:
            return ImGuiKey_R;
        case RE::BSKeyboardDevice::Key::kS:
            return ImGuiKey_S;
        case RE::BSKeyboardDevice::Key::kT:
            return ImGuiKey_T;
        case RE::BSKeyboardDevice::Key::kU:
            return ImGuiKey_U;
        case RE::BSKeyboardDevice::Key::kV:
            return ImGuiKey_V;
        case RE::BSKeyboardDevice::Key::kW:
            return ImGuiKey_W;
        case RE::BSKeyboardDevice::Key::kX:
            return ImGuiKey_X;
        case RE::BSKeyboardDevice::Key::kY:
            return ImGuiKey_Y;
        case RE::BSKeyboardDevice::Key::kZ:
            return ImGuiKey_Z;
        case RE::BSKeyboardDevice::Key::kF1:
            return ImGuiKey_F1;
        case RE::BSKeyboardDevice::Key::kF2:
            return ImGuiKey_F2;
        case RE::BSKeyboardDevice::Key::kF3:
            return ImGuiKey_F3;
        case RE::BSKeyboardDevice::Key::kF4:
            return ImGuiKey_F4;
        case RE::BSKeyboardDevice::Key::kF5:
            return ImGuiKey_F5;
        case RE::BSKeyboardDevice::Key::kF6:
            return ImGuiKey_F6;
        case RE::BSKeyboardDevice::Key::kF7:
            return ImGuiKey_F7;
        case RE::BSKeyboardDevice::Key::kF8:
            return ImGuiKey_F8;
        case RE::BSKeyboardDevice::Key::kF9:
            return ImGuiKey_F9;
        case RE::BSKeyboardDevice::Key::kF10:
            return ImGuiKey_F10;
        case RE::BSKeyboardDevice::Key::kF11:
            return ImGuiKey_F11;
        case RE::BSKeyboardDevice::Key::kF12:
            return ImGuiKey_F12;
        // Skip 584 ~ 595
        case RE::BSKeyboardDevice::Key::kApostrophe:
            return ImGuiKey_Apostrophe;
        case RE::BSKeyboardDevice::Key::kComma:
            return ImGuiKey_Comma;
        case RE::BSKeyboardDevice::Key::kMinus:
            return ImGuiKey_Minus;
        case RE::BSKeyboardDevice::Key::kPeriod:
            return ImGuiKey_Period;
        case RE::BSKeyboardDevice::Key::kSlash:
            return ImGuiKey_Slash;
        case RE::BSKeyboardDevice::Key::kSemicolon:
            return ImGuiKey_Semicolon;
        case RE::BSKeyboardDevice::Key::kEquals:
            return ImGuiKey_Equal;
        case RE::BSKeyboardDevice::Key::kBracketLeft:
            return ImGuiKey_LeftBracket;
        case RE::BSKeyboardDevice::Key::kBackslash:
            return ImGuiKey_Backslash;
        case RE::BSKeyboardDevice::Key::kBracketRight:
            return ImGuiKey_RightBracket;
        case RE::BSKeyboardDevice::Key::kTilde:
            return ImGuiKey_GraveAccent;
        case RE::BSKeyboardDevice::Key::kCapsLock:
            return ImGuiKey_CapsLock;
        case RE::BSKeyboardDevice::Key::kScrollLock:
            return ImGuiKey_ScrollLock;
        case RE::BSKeyboardDevice::Key::kNumLock:
            return ImGuiKey_NumLock;
        case RE::BSKeyboardDevice::Key::kPrintScreen:
            return ImGuiKey_PrintScreen;
        case RE::BSKeyboardDevice::Key::kPause:
            return ImGuiKey_Pause;
        case RE::BSKeyboardDevice::Key::kKP_0:
            return ImGuiKey_Keypad0;
        case RE::BSKeyboardDevice::Key::kKP_1:
            return ImGuiKey_Keypad1;
        case RE::BSKeyboardDevice::Key::kKP_2:
            return ImGuiKey_Keypad2;
        case RE::BSKeyboardDevice::Key::kKP_3:
            return ImGuiKey_Keypad3;
        case RE::BSKeyboardDevice::Key::kKP_4:
            return ImGuiKey_Keypad4;
        case RE::BSKeyboardDevice::Key::kKP_5:
            return ImGuiKey_Keypad5;
        case RE::BSKeyboardDevice::Key::kKP_6:
            return ImGuiKey_Keypad6;
        case RE::BSKeyboardDevice::Key::kKP_7:
            return ImGuiKey_Keypad7;
        case RE::BSKeyboardDevice::Key::kKP_8:
            return ImGuiKey_Keypad8;
        case RE::BSKeyboardDevice::Key::kKP_9:
            return ImGuiKey_Keypad9;
        case RE::BSKeyboardDevice::Key::kKP_Decimal:
            return ImGuiKey_KeypadDecimal;
        case RE::BSKeyboardDevice::Key::kKP_Divide:
            return ImGuiKey_KeypadDivide;
        case RE::BSKeyboardDevice::Key::kKP_Multiply:
            return ImGuiKey_KeypadMultiply;
        case RE::BSKeyboardDevice::Key::kKP_Subtract:
            return ImGuiKey_KeypadSubtract;
        case RE::BSKeyboardDevice::Key::kKP_Plus:
            return ImGuiKey_KeypadAdd;
        case RE::BSKeyboardDevice::Key::kKP_Enter:
            return ImGuiKey_KeypadEnter;
        // Skip 628 ~ 630
        default:
            return ImGuiKey_None;
    }
}

inline ImGuiKey ParseKeyFromKeyboard(std::uint32_t a_key) {
    return ParseKeyFromKeyboard(static_cast<RE::BSKeyboardDevice::Key>(a_key));
}

inline ImGuiKey ParseKeyFromGamepad(RE::BSWin32GamepadDevice::Key a_key) {
    switch (a_key) {
        case RE::BSWin32GamepadDevice::Key::kUp:
            return ImGuiKey_GamepadDpadUp;
        case RE::BSWin32GamepadDevice::Key::kDown:
            return ImGuiKey_GamepadDpadDown;
        case RE::BSWin32GamepadDevice::Key::kLeft:
            return ImGuiKey_GamepadDpadLeft;
        case RE::BSWin32GamepadDevice::Key::kRight:
            return ImGuiKey_GamepadDpadRight;
        case RE::BSWin32GamepadDevice::Key::kStart:
            return ImGuiKey_GamepadStart;
        case RE::BSWin32GamepadDevice::Key::kBack:
            return ImGuiKey_GamepadBack;
        case RE::BSWin32GamepadDevice::Key::kLeftThumb:
            return ImGuiKey_GamepadL3;
        case RE::BSWin32GamepadDevice::Key::kRightThumb:
            return ImGuiKey_GamepadR3;
        case RE::BSWin32GamepadDevice::Key::kLeftShoulder:
            return ImGuiKey_GamepadL1;
        case RE::BSWin32GamepadDevice::Key::kRightShoulder:
            return ImGuiKey_GamepadR1;
        case RE::BSWin32GamepadDevice::Key::kLeftTrigger:
            return ImGuiKey_GamepadL2;
        case RE::BSWin32GamepadDevice::Key::kRightTrigger:
            return ImGuiKey_GamepadR2;
        case RE::BSWin32GamepadDevice::Key::kA:
            return ImGuiKey_GamepadFaceDown;
        case RE::BSWin32GamepadDevice::Key::kB:
            return ImGuiKey_GamepadFaceRight;
        case RE::BSWin32GamepadDevice::Key::kX:
            return ImGuiKey_GamepadFaceLeft;
        case RE::BSWin32GamepadDevice::Key::kY:
            return ImGuiKey_GamepadFaceUp;
        default:
            return ImGuiKey_None;
    }
}

inline ImGuiKey ParseKeyFromGamepad(std::uint32_t a_key) {
    return ParseKeyFromGamepad(static_cast<RE::BSWin32GamepadDevice::Key>(a_key));
}

namespace {
    // Below this the stick is treated as centred; roughly the XInput default.
    constexpr float kStickDeadzone = 0.35f;
    // Cursor travel at full tilt, in virtual pixels per second.
    constexpr float kCursorSpeed = 1400.0f;

    float g_cursorX = -1.0f;
    float g_cursorY = -1.0f;
}

// The left stick mirrors the d-pad for ImGui navigation; the right stick drives
// the cursor. Without the latter there is no controller route to any
// pointer-driven widget (sliders, drags, window title bars), while
// io.MouseDrawCursor is on.
inline void TranslateThumbstickEvent(ImGuiIO& io, const RE::ThumbstickEvent* stick) {
    if (stick->IsLeft()) {
        io.AddKeyEvent(ImGuiKey_GamepadLStickLeft, stick->xValue < -kStickDeadzone);
        io.AddKeyEvent(ImGuiKey_GamepadLStickRight, stick->xValue > kStickDeadzone);
        io.AddKeyEvent(ImGuiKey_GamepadLStickUp, stick->yValue > kStickDeadzone);
        io.AddKeyEvent(ImGuiKey_GamepadLStickDown, stick->yValue < -kStickDeadzone);
        return;
    }

    if (!stick->IsRight()) {
        return;
    }

    const float x = std::abs(stick->xValue) > kStickDeadzone ? stick->xValue : 0.0f;
    const float y = std::abs(stick->yValue) > kStickDeadzone ? stick->yValue : 0.0f;
    if (x == 0.0f && y == 0.0f) {
        return;
    }

    if (g_cursorX < 0.0f || g_cursorY < 0.0f) {
        const bool hasCursor = io.MousePos.x > -FLT_MAX && io.MousePos.y > -FLT_MAX;
        g_cursorX = hasCursor ? io.MousePos.x : io.DisplaySize.x * 0.5f;
        g_cursorY = hasCursor ? io.MousePos.y : io.DisplaySize.y * 0.5f;
    }

    const float dt = io.DeltaTime > 0.0f ? io.DeltaTime : 1.0f / 60.0f;
    g_cursorX = std::clamp(g_cursorX + x * kCursorSpeed * dt, 0.0f, io.DisplaySize.x);
    g_cursorY = std::clamp(g_cursorY - y * kCursorSpeed * dt, 0.0f, io.DisplaySize.y);
    io.AddMousePosEvent(g_cursorX, g_cursorY);
}

inline void TranslateButtonEvent(ImGuiIO& io, const RE::ButtonEvent* button) {
    if (!button->HasIDCode()) {
        return;
    }

    switch (button->GetDevice()) {
        case RE::INPUT_DEVICE::kKeyboard:{
            auto imKey = ParseKeyFromKeyboard(button->GetIDCode());
            if (imKey == ImGuiKey_None) {
                break;
            }
            auto pressed = button->IsPressed();
            io.AddKeyEvent(imKey, pressed);
        } break;
        case RE::INPUT_DEVICE::kMouse: {
            switch (auto key = static_cast<RE::BSWin32MouseDevice::Key>(button->GetIDCode())) {
                case RE::BSWin32MouseDevice::Key::kWheelUp:
                    io.AddMouseWheelEvent(0, button->Value());
                    break;
                case RE::BSWin32MouseDevice::Key::kWheelDown:
                    io.AddMouseWheelEvent(0, button->Value() * -1);
                    break;
                default:
                    io.AddMouseButtonEvent(key, button->IsPressed());
                    break;
            }
        } break;
        case RE::INPUT_DEVICE::kGamepad: {
            auto imKey = ParseKeyFromGamepad(button->GetIDCode());
            if (imKey == ImGuiKey_None) {
                break;
            }
            io.AddKeyEvent(imKey, button->IsPressed());
        } break;
        default:
            break;
    }
}

void UI::TranslateInputEvent(RE::InputEvent* const* a_event) {
    auto& io = ImGui::GetIO();

    for (auto event = *a_event; event; event = event->next) {
        if (auto button = event->AsButtonEvent()) {
            TranslateButtonEvent(io, button);
        } else if (auto thumbstick = event->AsThumbstickEvent()) {
            TranslateThumbstickEvent(io, thumbstick);
        } else if (auto charEvent = event->AsCharEvent()) {
            io.AddInputCharacter(charEvent->keyCode);
        }
    }
}

void DoublePressDetector::press(){
    auto now = std::chrono::steady_clock::now();
    if (last_pressed_index) {
        last_pressed_times.second = now;
    } else {
        last_pressed_times.first = now;
    }
    increment();
};

DoublePressDetector::operator bool() const {
    const auto [first, second] = last_pressed_times;
    const int diff = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(second - first).count());
    return std::abs(diff) < Config::DoublePressThreshold;
    
}
void DoublePressDetector::reset(){ last_pressed_times = {Timestamp::min(), Timestamp::min()};};
void DoublePressDetector::increment(){ last_pressed_index = !last_pressed_index; };

bool IsSupportedDevice(RE::INPUT_DEVICE device) {
    switch (device) {
        case kKeyboard:
        case kGamepad:
            return true;
        default:
            return false;
    }
}
