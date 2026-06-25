#pragma once

typedef bool(__stdcall* InputEventCallback)(RE::InputEvent*);

struct DoublePressDetector {
    
    void press();
    operator bool() const;
    DoublePressDetector() = default;

private:
    using Timestamp = std::chrono::steady_clock::time_point;
    using TimePair = std::pair<Timestamp, Timestamp>;

    bool last_pressed_index = 0;
    TimePair last_pressed_times = {Timestamp::min(), Timestamp::min()};
    static constexpr int double_press_threshold = 300;

    [[maybe_unused]] void reset();
    void increment();

};

static DoublePressDetector DoublePressDetectorKeyboard;
static DoublePressDetector DoublePressDetectorGamepad;

enum SupportedDevices { kKeyboard = RE::INPUT_DEVICE::kKeyboard, kGamepad = RE::INPUT_DEVICE::kGamepad};

bool IsSupportedDevice(RE::INPUT_DEVICE device);


namespace UI {
    void TranslateInputEvent(RE::InputEvent* const* a_event);
}
