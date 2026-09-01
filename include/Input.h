#pragma once

struct DoublePressDetector {
    
    void press();
    operator bool() const;
    DoublePressDetector() = default;

private:
    using Timestamp = std::chrono::steady_clock::time_point;
    using TimePair = std::pair<Timestamp, Timestamp>;

    bool last_pressed_index = 0;
    TimePair last_pressed_times = {Timestamp::min(), Timestamp::min()};

    [[maybe_unused]] void reset();
    void increment();

};

// inline, not static: a `static` at namespace scope in a header gives every
// translation unit its own copy, so state written by one would be invisible to
// the rest.
inline DoublePressDetector DoublePressDetectorKeyboard;
inline DoublePressDetector DoublePressDetectorGamepad;

enum SupportedDevices { kKeyboard = RE::INPUT_DEVICE::kKeyboard, kGamepad = RE::INPUT_DEVICE::kGamepad};

bool IsSupportedDevice(RE::INPUT_DEVICE device);


namespace UI {
    void TranslateInputEvent(RE::InputEvent* const* a_event);
}
