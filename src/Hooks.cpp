#include "Hooks.h"
#include <format>
#include "Renderer.h"
#include "FontManager.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "WindowManager.h"
#include "InputEventHandler.h"
#include "HudManager.h"
#include "GameLock.h"
#include "TextureLoader.h"
#include "Event.h"

#include "ImGuiVRHelperClientSDK.h"  // vendored ImGuiVRHelper SDK (VR overlay/input)

namespace {
    // VR overlay-helper client. In SkyrimVR with the helper installed, the menu
    // is mirrored into the helper's in-scene panel and driven by the wand; on
    // desktop (no helper) this stays unconnected and the normal flat path runs.
    ImGuiVRHelperPluginAPI::Client g_vrHelper;
}

void Hooks::Install() {
    D3DInitHook::install();
    RenderUIHook::install();
    ProcessInputQueueHook::install();
}

void Hooks::ConnectVRHelper() {
    // Call from our kPostPostLoad handler: by then the helper has registered its
    // handshake listener (at kPostLoad), so the connect reaches it regardless of
    // load order. On flat screen / SE-AE there's no helper, so Connect simply
    // fails and the normal flat path runs.
    const auto decl = SKSE::PluginDeclaration::GetSingleton();
    const auto version = decl->GetVersion();
    const auto versionStr = std::format("{}.{}.{}", version.major(), version.minor(), version.patch());
    // kClientFlag_OwnCursor: draw our own cursor from io.MousePos (the same
    // value ImGui hit-tests against) instead of the helper's own marker.
    if (g_vrHelper.Connect(BEAUTIFUL_NAME, versionStr.c_str(),
            ImGuiVRHelperPluginAPI::kClientFlag_RendersOnFocus |
                ImGuiVRHelperPluginAPI::kClientFlag_OwnCursor)) {
        logger::info("ImGuiVRHelper: connected as VR overlay client");
    } else {
        logger::info("ImGuiVRHelper not present; menu stays on the flat mirror");
    }
}

void Hooks::D3DInitHook::install() {
    SKSE::AllocTrampoline(14);
    auto& trampoline = SKSE::GetTrampoline();
    originalFunction = trampoline.write_call<5>(
        REL::RelocationID(75595, 77226, 75595).address() + REL::Relocate(0x9, 0x275, 0x9), thunk);
}

void Hooks::RenderUIHook::install() {
    SKSE::AllocTrampoline(14);
    auto& trampoline = SKSE::GetTrampoline();
    originalFunction1 = trampoline.write_call<5>(REL::RelocationID(35556, 36555, 35556).address() + REL::Relocate(0x3ab, 0x371, 0x355), thunk1);
    SKSE::AllocTrampoline(14);
    originalFunction2 = trampoline.write_call<5>(REL::RelocationID(38085, 39039).address() + REL::Relocate(0x19a, 0x19a, 0x3FC), thunk2);
}

void Hooks::ProcessInputQueueHook::install() {
    SKSE::AllocTrampoline(14);
    auto& trampoline = SKSE::GetTrampoline();
    originalFunction = trampoline.write_call<5>(
        REL::RelocationID(67315, 68617, 67315).address() + REL::Relocate(0x7B, 0x7B, 0x81), thunk);
}
void DisableImGuiInput() {
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
}
void EnableImGuiInput() {
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavNoCaptureKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
}

RE::InputEvent** RemoveNonPrintScreenInputs(RE::InputEvent** a_event) {
    auto first = *a_event;
    auto last = *a_event;
    size_t length = 0;

    for (auto current = *a_event; current; current = current->next) {

        if (auto button = current->AsButtonEvent()) {
            if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard && button->GetIDCode() == RE::BSWin32KeyboardDevice::Keys::kPrintScreen) {
                last = current;
                ++length;
                continue;
            }
        }

        if (current != last) {
            last->next = current->next;
        } else {
            last = current->next;
            first = current->next;
        }
    }
    a_event[0] = first;
    return a_event;
}

void Hooks::ProcessInputQueueHook::thunk(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher,
                                      RE::InputEvent* const* a_event) {
    bool isInputCapturedByOpenClose = UI::Renderer::ProcessOpenClose(a_event);

    if (!ImGui::IsAnyItemActive()) {
        a_event = InputEventHandler::Process(const_cast<RE::InputEvent**>(a_event));
    }

    if (isInputCapturedByOpenClose) {
        constexpr RE::InputEvent* const dummy[] = {nullptr};
        originalFunction(a_dispatcher, dummy);
    } else {
        if (WindowManager::ShouldTheGameBePaused()) {
            UI::TranslateInputEvent(a_event);
            originalFunction(a_dispatcher, RemoveNonPrintScreenInputs(const_cast<RE::InputEvent**>(a_event)));
        } else {
            originalFunction(a_dispatcher, a_event);
        }
    }
}

LRESULT Hooks::WndProcHook::thunk(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KILLFOCUS) {
        auto& io = ImGui::GetIO();
        io.ClearInputKeys();
    }
    return func(hWnd, uMsg, wParam, lParam);
}

void Hooks::D3DInitHook::thunk() {
    logger::debug("[D3DInitHook] START");

    originalFunction();

    const auto renderer = RE::BSGraphics::Renderer::GetSingleton();
    if (!renderer) {
        SKSE::log::error("couldn't find renderer");
        return;
    }
    auto data = renderer->GetRuntimeData();
    const auto swapChain = reinterpret_cast<IDXGISwapChain*>(data.renderWindows[0].swapChain);
    if (!swapChain) {
        SKSE::log::error("couldn't find swapChain");
        return;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapChain->GetDesc(std::addressof(desc)))) {
        SKSE::log::error("IDXGISwapChain::GetDesc failed.");
        return;
    }
    const auto device = reinterpret_cast<ID3D11Device*>(data.forwarder);
    const auto context = reinterpret_cast<ID3D11DeviceContext*>(data.context);

    TextureLoader::Init(device, context);

    SKSE::log::info("Initializing ImGui...");

    ImGui::CreateContext();

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.IniFilename = nullptr;
    io.MouseDrawCursor = true;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    if (!ImGui_ImplWin32_Init(desc.OutputWindow)) {
        SKSE::log::error("ImGui initialization failed (Win32)");
        return;
    }

    if (!ImGui_ImplDX11_Init(device, context)) {
        SKSE::log::error("ImGui initialization failed (DX11)");
        return;
    }

    UI::Renderer::initialized.store(true);

    SKSE::log::info("ImGui initialized.");

    WndProcHook::func = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrA(desc.OutputWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook::thunk)));
    if (!WndProcHook::func) {
        SKSE::log::error("SetWindowLongPtrA failed!");
    }

    Config::LoadStyle();

    auto regular = FontManager::LoadFonts(io, Config::FontSizeMedium);
    io.FontDefault = regular.defaultFont;

    FontManager::fontSizes["Default"] = regular;

    io.Fonts->Build();

    logger::debug("[D3DInitHook] FINISH");
}

void Render() {
    if (!UI::Renderer::initialized.load()) {
        return;
    }

    Event::DispatchEvent(Event::EventType::kBeforeRender);

    // VR overlay helper. When connected, Update() reconciles our menu-open state
    // with the helper's focus (so its open/cycle combo opens this menu) and pumps
    // the wand into ImGui before NewFrame consumes the input.
    if (g_vrHelper.IsConnected()) {
        bool menuOpen = WindowManager::IsAnyWindowOpen();
        g_vrHelper.Update(menuOpen);
        if (menuOpen != WindowManager::IsAnyWindowOpen())
            menuOpen ? WindowManager::Open() : WindowManager::Close();
        g_vrHelper.PumpKeyboard();
    }

    // Decided once per frame, after the VR reconciliation above (which can
    // flip menu-open this same frame), not per RE::InputEvent: the VR wand is
    // pumped straight into ImGui's IO above, bypassing RE::InputEvent
    // entirely, so a wand-only session needs this cleared without a native
    // keyboard/mouse event ever firing.
    if (WindowManager::ShouldTheGameBePaused()) {
        EnableImGuiInput();
    } else {
        DisableImGuiInput();
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    // When connected, size to the helper's panel canvas so wand UV and our own
    // layout share one coordinate space, instead of the flat mirror's resolution.
    if (!g_vrHelper.IsConnected() || !g_vrHelper.ApplyPanelDisplaySize()) {
        // trick imgui into rendering at game's real resolution (ie. if upscaled with Display Tweaks)
        static const auto screenSize = RE::BSGraphics::Renderer::GetScreenSize();

        auto& io = ImGui::GetIO();
        io.DisplaySize.x = static_cast<float>(screenSize.width);
        io.DisplaySize.y = static_cast<float>(screenSize.height);
    }
    ImGui::NewFrame();
    HudManager::Render();
    if (WindowManager::IsAnyWindowOpen()) {
        auto& io = ImGui::GetIO();
        if (!WindowManager::ShouldTheGameBePaused()) {
            io.MouseDrawCursor = false;
            GameLock::SetState(GameLock::State::Resume);
        } else {
            GameLock::SetState(GameLock::State::Locked);
            io.MouseDrawCursor = true;
        }

        UI::Renderer::RenderWindows();
    } else {
        auto& io = ImGui::GetIO();
        io.MouseDrawCursor = false;
        GameLock::SetState(GameLock::State::Unlocked);
    }
    FontManager::CleanFont();
    ImGui::EndFrame();
    ImGui::Render();
    // One output call: VR + helper → flat panel only (the helper composites it
    // as a world-space quad); flat screen / no helper → our normal in-game draw.
    // RenderFrame hides that branch, so we never paint a second copy into the
    // game's kHUDMENU (which Skyrim VR would wrap onto its curved HUD).
    g_vrHelper.RenderFrame();

    if (FontManager::ConsumeAtlasRebuildRequest()) {
        auto& io = ImGui::GetIO();
        ImGui_ImplDX11_InvalidateDeviceObjects();
        io.Fonts->Clear();
        FontManager::fontSizes.clear();

        auto regular = FontManager::LoadFonts(io, Config::FontSizeMedium);
        io.FontDefault = regular.defaultFont;
        FontManager::fontSizes["Default"] = std::move(regular);

        if (!io.Fonts->Build()) {
            logger::error("Failed to rebuild the font atlas for font size {}.", Config::FontSizeMedium);
        } else if (!ImGui_ImplDX11_CreateDeviceObjects()) {
            logger::error("Failed to recreate ImGui device objects after rebuilding the font atlas.");
        }
    }

    Event::DispatchEvent(Event::EventType::kAfterRender);
}

int64_t Hooks::RenderUIHook::thunk1(int64_t gMenuManager) {
    auto result = originalFunction1(gMenuManager);
    Render();
    return result;
}

int64_t Hooks::RenderUIHook::thunk2(int64_t gMenuManager) { 
    auto result = originalFunction2(gMenuManager);
    Render();
    return result;
}
