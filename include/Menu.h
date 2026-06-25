#pragma once
#include "Input.h"

class SKSEMenuFrameworkMenu : public RE::IMenu, public RE::BSTEventSink<RE::InputEvent*> {
public:
    static constexpr const char* MENU_NAME = "SKSEMenuFrameworkMenu";

    SKSEMenuFrameworkMenu();
    ~SKSEMenuFrameworkMenu();

    // override (RE::IMenu)
    RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override;
    void PostDisplay() override;
    void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override;

	// override (RE::BSTEventSink)
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>* a_source) override;

    static RE::IMenu* Create() { return new SKSEMenuFrameworkMenu(); }
    static void Register();
    static void Open();

    static int64_t RegisterInputEvent(InputEventCallback callback);
    static void UnregisterInputEvent(uint64_t id);

private:
    bool isBlurActive = false;

    static inline std::map<uint64_t, InputEventCallback> inputCallbacks;
    static inline uint64_t nextCallbackId = 0;
};
