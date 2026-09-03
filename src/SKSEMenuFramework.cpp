#include "SKSEMenuFramework.h"
#include "FontManager.h"
#include <imgui.h>
#include "Application.h"
#include "Renderer.h"
#include "UI.h"
#include "TextureLoader.h"
#include "WindowManager.h"

#define MENU_FRAMEWORK_VERSION 3.8f

void AddSectionItem(const char* path, RenderFunction rendererFunction) { 
    if (!path || !rendererFunction) {
        return;
    }

    auto pathSplit = SplitString(path, '/');
    if (pathSplit.empty()) {
        return;
    }

    AddToTree(UI::RootMenu, pathSplit, rendererFunction, pathSplit.back());
}

bool RenameSection(const char* sectionPath, const char* newName) {
    return UI::QueueMenuMutation({UI::MenuMutationType::Rename, sectionPath ? sectionPath : "", newName ? newName : ""});
}

bool DeleteSection(const char* sectionPath) {
    return UI::QueueMenuMutation({UI::MenuMutationType::Delete, sectionPath ? sectionPath : "", ""});
}

bool RenameSectionItem(const char* fullPagePath, const char* newName) {
    return UI::QueueMenuMutation({UI::MenuMutationType::Rename, fullPagePath ? fullPagePath : "", newName ? newName : ""});
}

bool DeleteSectionItem(const char* fullPagePath) {
    return UI::QueueMenuMutation({UI::MenuMutationType::Delete, fullPagePath ? fullPagePath : "", ""});
}

WindowInterface* AddWindow(RenderFunction rendererFunction) { 

    auto newWindow = new Window();

    newWindow->Render = rendererFunction;

    WindowManager::Windows.push_back(newWindow);

    return newWindow->Interface;

}

void PushDefault() 
{
    FontManager::currentFont = (Font)(FontManager::currentFont | Font::fontSizeDefault);
    FontManager::ProcessFont();
}
void PushBig()
{
}
void PushSmall()
{
}

void PushSolid() 
{
}

void PushRegular() 
{

}

void PushBrands() 
{

}

void PushFont(const char* name)
{
    if (name) {
        FontManager::SetFont(name);
    }
}

void Pop() { FontManager::CleanFont(); }

int64_t RegisterInpoutEvent(InputEventCallback callback) { return InputEventHandler::Register(callback); }

void UnregisterInputEvent(uint64_t id) { InputEventHandler::Unregister(id); }

int64_t RegisterHudElement(HudElementCallback callback) { return HudManager::Register(callback); }

void UnregisterHudElement(uint64_t id) { HudManager::Unregister(id); }

bool IsAnyBlockingWindowOpened() { return WindowManager::ShouldTheGameBePaused(); }

ImTextureID LoadTexture(const char* texturePath, ImVec2* size) {
    return TextureLoader::GetTexture(texturePath, size ? *size : ImVec2{0,0});
}

void DisposeTexture(const char* texturePath) { 
    TextureLoader::DisposeTexture(texturePath);
}

int64_t RegisterEvent(Event::EventCallback callback) { return Event::AddEventListener(callback, 0); }

int64_t RegisterEventPriority(Event::EventCallback callback, float priority) {
    return Event::AddEventListener(callback, priority);
}

void UnregisterEvent(int64_t id) { Event::RemoveEventListener(id); }

float GetMenuFrameworkVersion() { return MENU_FRAMEWORK_VERSION; }

WindowInterface* GetMainWindow() { return WindowManager::MainInterface; }

void SetHotkeyEnabled(bool enabled) { UI::Renderer::hotkeyEnabled.store(enabled); }

bool IsHotkeyEnabled() { return UI::Renderer::hotkeyEnabled.load(); }
