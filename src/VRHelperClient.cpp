#include "VRHelperClient.h"
#include "WindowManager.h"
#include "ImGuiVRHelperClientSDK.h"  // vendored ImGuiVRHelper SDK (VR overlay/input)

namespace {
    // VR overlay-helper client. In SkyrimVR with the helper installed, the menu
    // is mirrored into the helper's in-scene panel and driven by the wand; on
    // desktop (no helper) this stays unconnected and the normal flat path runs.
    ImGuiVRHelperPluginAPI::Client g_vrHelper;
}

void VRHelperClient::ConnectVRHelper() {
    // Call from our kPostPostLoad handler: by then the helper has registered its
    // handshake listener (at kPostLoad), so the connect reaches it regardless of
    // load order. On flat screen / SE-AE there's no helper, so Connect simply
    // fails and the normal flat path runs.
    const auto decl = SKSE::PluginDeclaration::GetSingleton();
    const auto version = decl->GetVersion();
    const auto versionStr = std::format("{}.{}.{}", version.major(), version.minor(), version.patch());
    if (g_vrHelper.Connect(BEAUTIFUL_NAME, versionStr.c_str(), ImGuiVRHelperPluginAPI::kClientFlag_RendersOnFocus)) {
        logger::info("ImGuiVRHelper: connected as VR overlay client");
    } else {
        logger::info("ImGuiVRHelper not present; menu stays on the flat mirror");
    }
}

void VRHelperClient::Update() {
    // VR overlay helper. When connected, Update() reconciles our menu-open state
    // with the helper's focus (so its open/cycle combo opens this menu) and pumps
    // the wand into ImGui before NewFrame consumes the input.
    if (g_vrHelper.IsConnected()) {
        bool menuOpen = WindowManager::IsAnyWindowOpen();
        g_vrHelper.Update(menuOpen);
        if (menuOpen != WindowManager::IsAnyWindowOpen()) menuOpen ? WindowManager::Open() : WindowManager::Close();
    }
}
