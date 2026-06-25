#include "SKSEMenuFramework.h"
#include "FontManager.h"
#include "Menu.h"
#include <imgui.h>
#include "Application.h"
#include "Renderer.h"
#include "UI.h"
#include "TextureLoader.h"
#include "WindowManager.h"

#define MENU_FRAMEWORK_VERSION 3.7f

void AddSectionItem(const char* path, RenderFunction rendererFunction) { 
    auto pathSplit = SplitString(path, '/');
    AddToTree(UI::RootMenu, pathSplit, rendererFunction, pathSplit.back());
}

WindowInterface* AddWindow(RenderFunction rendererFunction) { 

    auto newWindow = new Window();

    newWindow->Render = rendererFunction;

    WindowManager::Windows.push_back(newWindow);

    return newWindow->Interface;

}

void PushDefault() 
{
    FontManager::currentFont = (Font)(FontManager::currentFont & ~Font::fontSizeBig);
    FontManager::currentFont = (Font)(FontManager::currentFont & ~Font::fontSizeSmall);
    FontManager::currentFont = (Font)(FontManager::currentFont | Font::fontSizeDefault);
    FontManager::ProcessFont();
}
void PushBig() 
{
    FontManager::currentFont = (Font)(FontManager::currentFont & ~Font::fontSizeDefault);
    FontManager::currentFont = (Font)(FontManager::currentFont & ~Font::fontSizeSmall);
    FontManager::currentFont = (Font)(FontManager::currentFont | Font::fontSizeBig);
    FontManager::ProcessFont();
}
void PushSmall() 
{
    FontManager::currentFont = (Font)(FontManager::currentFont & ~Font::fontSizeDefault);
    FontManager::currentFont = (Font)(FontManager::currentFont & ~Font::fontSizeBig);
    FontManager::currentFont = (Font)(FontManager::currentFont | Font::fontSizeSmall);
    FontManager::ProcessFont();
}

void PushSolid() 
{
    FontManager::currentFont = (Font)(FontManager::currentFont & ~Font::faBrands);
    FontManager::currentFont = (Font)(FontManager::currentFont & ~Font::faRegular);
    FontManager::currentFont = (Font)(FontManager::currentFont | Font::faSolid);
    FontManager::ProcessFont();
}

void PushRegular() 
{
    FontManager::currentFont = (Font)(FontManager::currentFont & ~Font::faSolid);
    FontManager::currentFont = (Font)(FontManager::currentFont & ~Font::faBrands);
    FontManager::currentFont = (Font)(FontManager::currentFont | Font::faRegular);
    FontManager::ProcessFont();
}

void PushBrands() 
{
    FontManager::currentFont = (Font)(FontManager::currentFont & ~Font::faSolid);
    FontManager::currentFont = (Font)(FontManager::currentFont & ~Font::faRegular);
    FontManager::currentFont = (Font)(FontManager::currentFont | Font::faBrands);
    FontManager::ProcessFont();
}

void Pop() { FontManager::CleanFont(); }

int64_t RegisterInpoutEvent(InputEventCallback callback) { return SKSEMenuFrameworkMenu::RegisterInputEvent(callback); }

void UnregisterInputEvent(uint64_t id) { SKSEMenuFrameworkMenu::UnregisterInputEvent(id); }

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