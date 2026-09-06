#pragma once

#include <mutex>

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
    inline std::atomic<RE::INPUT_DEVICE> activeInputDevice{RE::INPUT_DEVICE::kKeyboard};
    void UpdateActiveInputDevice(RE::InputEvent* const* events);

    class KeyBindingCapture {
    public:
        enum class State { Idle, Waiting, Pressed, Complete, Cancelled, Confirming };
        static constexpr unsigned int UnboundKey = 0;

        void Begin(RE::INPUT_DEVICE device);
        void BeginConfirmation();
        bool IsConfirming() const;
        void Reset();
        bool Process(RE::InputEvent* const* events);
        State Poll(unsigned int& key, RE::INPUT_DEVICE displayedDevice);

    private:
        std::mutex mutex;
        std::atomic<State> state{State::Idle};
        RE::INPUT_DEVICE captureDevice = RE::INPUT_DEVICE::kNone;
        RE::INPUT_DEVICE device = RE::INPUT_DEVICE::kNone;
        unsigned int key = UnboundKey;
    };

    inline KeyBindingCapture keyBindingCapture;

    void TranslateInputEvent(RE::InputEvent* const* a_event, bool capturingBinding = false);
}
