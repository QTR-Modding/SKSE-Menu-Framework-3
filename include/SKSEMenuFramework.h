#pragma once
#include <cstdint>
#include "InputEventHandler.h"
#include "WindowManager.h"
#include "HudManager.h"
#include "imgui.h"
#include "Event.h"

#define FUNCTION_PREFIX extern "C" [[maybe_unused]] __declspec(dllexport)

// Menu paths use unescaped '/' characters as separators. Prefix a literal slash with a backslash.
FUNCTION_PREFIX void AddSectionItem(const char* path, RenderFunction rendererFunction);
FUNCTION_PREFIX bool RenameSection(const char* path, const char* newName);
FUNCTION_PREFIX bool DeleteSection(const char* path);
FUNCTION_PREFIX WindowInterface* AddWindow(RenderFunction rendererFunction);
FUNCTION_PREFIX void PushBig();
FUNCTION_PREFIX void PushDefault();
FUNCTION_PREFIX void PushSmall();
FUNCTION_PREFIX void PushSolid();
FUNCTION_PREFIX void PushRegular();
FUNCTION_PREFIX void PushBrands();
FUNCTION_PREFIX void PushFont(const char* name);
FUNCTION_PREFIX void Pop();
FUNCTION_PREFIX int64_t RegisterInpoutEvent(InputEventCallback callback);
FUNCTION_PREFIX void UnregisterInputEvent(uint64_t id);
FUNCTION_PREFIX int64_t RegisterHudElement(HudElementCallback callback);
FUNCTION_PREFIX void UnregisterHudElement(uint64_t id);
FUNCTION_PREFIX bool IsAnyBlockingWindowOpened();
FUNCTION_PREFIX ImTextureID LoadTexture(const char* texturePath, ImVec2* size);
FUNCTION_PREFIX void DisposeTexture(const char* texturePath);
FUNCTION_PREFIX int64_t RegisterEvent(Event::EventCallback callback);
FUNCTION_PREFIX int64_t RegisterEventPriority(Event::EventCallback callback, float priority);
FUNCTION_PREFIX void UnregisterEvent(int64_t id);
FUNCTION_PREFIX float GetMenuFrameworkVersion();
FUNCTION_PREFIX uint32_t GetMenuFrameworkAPIVersion();
FUNCTION_PREFIX WindowInterface* GetMainWindow();
FUNCTION_PREFIX void SetHotkeyEnabled(bool enabled);
FUNCTION_PREFIX bool IsHotkeyEnabled();
