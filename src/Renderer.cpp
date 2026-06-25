#include "Renderer.h"
#include "Event.h"
#include "FontManager.h"
#include "WindowManager.h"
#include "imgui_impl_dx11.h"


void UI::Renderer::ImGuiBegin() {
    ImGui_ImplDX11_NewFrame();

    const auto screenSize = RE::BSGraphics::Renderer::GetScreenSize();
    static LARGE_INTEGER frequency = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f;
    }();
    static LARGE_INTEGER lastTime = [] {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        return t;
    }();

    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);

    auto& io = ImGui::GetIO();
    io.DeltaTime = static_cast<float>(currentTime.QuadPart - lastTime.QuadPart) / static_cast<float>(frequency.QuadPart);
    lastTime = currentTime;
    io.DisplaySize.x = static_cast<float>(screenSize.width);
    io.DisplaySize.y = static_cast<float>(screenSize.height);

    RE::MenuCursor* cursor = RE::MenuCursor::GetSingleton();
    if (cursor) {
        io.AddMousePosEvent(cursor->GetRuntimeData().cursorPosX, cursor->GetRuntimeData().cursorPosY);
    }

    Event::DispatchEvent(Event::EventType::kBeforeRender);
    ImGui::NewFrame();
}

void UI::Renderer::ImGuiEnd() {
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    FontManager::CleanFont();
    Event::DispatchEvent(Event::EventType::kAfterRender);
}

void UI::Renderer::RenderWindows() {
    for (const auto window : WindowManager::Windows) {
        if (window->Interface->IsOpen) {
            window->Render();
        }
    }
}
