#include "Hooks.h"
#include "Config.h"
#include "Logger.h"
#include "UI.h"
#include "SKSEMenuFramework.h"
#include "Licence.h"
#include "Translations.h"
#include "Menu.h"
#include "VRHelperClient.h"

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SetupLog();
    #if VALIDATE_LICENSE
    if (!Licence::Validate()) {
        return false;
    }
    #endif
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    Config::Init();
    WindowManager::MainInterface = AddWindow(UI::RenderMenuWindow);
    WindowManager::ConfigInterface = AddWindow(UI::RenderConfigWindow);
    WindowManager::MainInterface->BlockUserInput = true;
    WindowManager::ConfigInterface->BlockUserInput = true;
    Translations::Install();
    Hooks::Install();
    Hooks::InstallInputHooks();

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
        if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
            SKSEMenuFrameworkMenu::Register();
            SKSEMenuFrameworkMenu::Open();
        } else if (a_msg->type == SKSE::MessagingInterface::kPostPostLoad) {
            // Connect to ImGuiVRHelper at kPostPostLoad — by then the helper has
            // registered its handshake listener (at kPostLoad), so this reaches it
            // regardless of load order.
            VRHelperClient::ConnectVRHelper();
        }
    });

    return true;
}