#include "Hooks.h"
#include "Renderer.h"
#include "FontManager.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "WindowManager.h"
#include "HudManager.h"
#include "TextureLoader.h"
#include "Event.h"

template <class T>
bool Hooks::InputHandlerHook<T>::CanProcess_Hook(RE::InputEvent* a_event) {
    if (WindowManager::IsAnyWindowOpen()) {
        return false;
    }
    return originalCanProcess(this, a_event);
}

void Hooks::InstallInputHooks() {
    Hooks::InputHandlerHook<RE::ActivateHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_ActivateHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::AttackBlockHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_AttackBlockHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::AutoMoveHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_AutoMoveHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::FirstPersonState>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_FirstPersonState[0]), 0xB);
    Hooks::InputHandlerHook<RE::JumpHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_JumpHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::LookHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_LookHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::MovementHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_MovementHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::ReadyWeaponHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_ReadyWeaponHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::RunHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_RunHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::ShoutHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_ShoutHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::SneakHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_SneakHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::SprintHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_SprintHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::ThirdPersonState>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_ThirdPersonState[0]), 0x12);
    Hooks::InputHandlerHook<RE::TogglePOVHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_TogglePOVHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::ToggleRunHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_ToggleRunHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::FavoritesHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_FavoritesHandler[0]), 0x1);
    Hooks::InputHandlerHook<RE::MenuOpenHandler>::Install(REL::Relocation<std::uintptr_t>(RE::VTABLE_MenuOpenHandler[0]), 0x1);
}

void Hooks::Install() { D3DInitHook::install(); }

void Hooks::D3DInitHook::install() {
    SKSE::AllocTrampoline(14);
    auto& trampoline = SKSE::GetTrampoline();
    originalFunction = trampoline.write_call<5>(
        REL::RelocationID(75595, 77226, 75595).address() + REL::Relocate(0x9, 0x275, 0x9), thunk);
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

    ID3D11DeviceContext* context = nullptr;
    device->GetImmediateContext(&context);
    if (!context) {
        SKSE::log::error("couldn't get immediate context");
        return;
    }
    TextureLoader::Init(device, context);

    SKSE::log::info("Initializing ImGui...");

    ImGui::CreateContext();

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.IniFilename = nullptr;
    io.MouseDrawCursor = false;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    if (!ImGui_ImplWin32_Init(desc.OutputWindow)) {
        SKSE::log::error("ImGui initialization failed (Win32)");
        return;
    }

    if (!ImGui_ImplDX11_Init(device, context)) {
        SKSE::log::error("ImGui initialization failed (DX11)");
        return;
    }

    WndProcHook::func = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrA(desc.OutputWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook::thunk)));
    if (!WndProcHook::func) {
        SKSE::log::error("SetWindowLongPtrA failed!");
    }

    Config::LoadStyle();

    auto regular = FontManager::LoadFonts(io, Config::FontSizeMedium);
    io.FontDefault = regular.defaultFont;

    FontManager::fontSizes["Big"] = FontManager::LoadFonts(io, Config::FontSizeBig);
    FontManager::fontSizes["Small"] = FontManager::LoadFonts(io, Config::FontSizeSmall);
    FontManager::fontSizes["Default"] = regular;

    io.Fonts->Build();

    if (!ImGui_ImplDX11_CreateDeviceObjects()) {
        SKSE::log::error("ImGui DX11 device object creation failed");
        return;
    }

    UI::Renderer::initialized.store(true);

    SKSE::log::info("ImGui initialized.");

    logger::debug("[D3DInitHook] FINISH");
}
