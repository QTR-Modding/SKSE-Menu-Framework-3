#include "Hooks.h"
#include "Config.h"
#include "Logger.h"
#include "UI.h"
#include "SKSEMenuFramework.h"
#include "Licence.h"
#include "RootMenuConfig.h"
#include "Translations.h"

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SetupLog();
    #if VALIDATE_LICENSE
    if (!Licence::Validate()) {
        return false;
    }
    #endif
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    // Connect to ImGuiVRHelper at kPostPostLoad — by then the helper has
    // registered its handshake listener (at kPostLoad), so this reaches it
    // regardless of load order.
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* m) {
        if (m->type == SKSE::MessagingInterface::kPostPostLoad) {
            Hooks::ConnectVRHelper();
        }
    });
    Config::Init();
    RootMenuConfig::Load();
    WindowManager::MainInterface = AddWindow(UI::RenderMenuWindow);
    WindowManager::ConfigInterface = AddWindow(UI::RenderConfigWindow);
    WindowManager::MainInterface->BlockUserInput = true;
    WindowManager::ConfigInterface->BlockUserInput = true;
    Translations::Install();
    Hooks::Install();
    return true;
}
