# Updating

Mods using the old header should be fully compatible; however, if you want to use the new features, you need to update the [header file](https://github.com/QTR-Modding/SKSE-Menu-Framework-3/blob/master/resources/SKSEMenuFramework.h)


# New features 3.4

## DisposeTexture

This function can be used to free the texture memory when it is no longer used and to allow the texture to be refreshed once LoadTexture is called.

```cpp
SKSEMenuFramework::DisposeTexture(TEXTURE_PATH);
```

## SKSEMenuFramework::Model::Event

This class can be used to listen for open and close menu events.

```cpp
void __stdcall EventListener(SKSEMenuFramework::Model::EventType eventType) {
    logger::trace("Event: {}", eventType);
}

SKSEMenuFramework::Model::Event* event;
void UI::Register() {
    event = new SKSEMenuFramework::Model::Event(EventListener);
}
```

to stop listening:

```cpp
delete event;
```

# New features

Video demonstration of the update

[![Video Demonstration of the update](https://img.youtube.com/vi/pjeNZ6jXDmI/0.jpg)](https://www.youtube.com/watch?v=pjeNZ6jXDmI)

## Translation

Thanks to blnkxin, now it is possible to translate this mod and mods that use the mod control panel to Chinese and other languages with all sorts of character ranges.
Now the mod is fully translatable; I'll soon post both a Brazilian Portuguese and a Chinese translation.

## Themes

Now it is possible to create custom themes by simply placing JSON files with the theme specification into the Data\\SKSE\\plugins\\SKSEMenuFrameworkThemes folder.

`WindowBlurStrength` controls the custom blur drawn behind each top-level ImGui window. Set it to `0.0` to disable the effect; values from `0.25` to `16.0` increase the sampling radius. This effect is independent from the full-screen background blur setting.

## Configration

The mod now has its own configuration menu, which allows you to customize many settings while in the game; all changes are persisted in the config file. You can access the menu on Options/Open Settings.

If you are on a gamepad you can select this menu by pressing X

## Unpause

Now it is possible for modders to create menus that do not pause the game, and also it is now possible to unpause the mod control panel on Options/Resume Game.

# New Features Added To The API

## Is any window open API

If returns true, the player is currently controlling the game; otherwise, it returns false

```cpp
bool IsAnyBlockingWindowOpened()
```

## Input Capturing and blocking

Register function signature

```cpp
inline Model::InputEvent* AddInputEvent(Model::InputEventCallback callback) 
```

Example of registering a user input hook

```cpp
SKSEMenuFramework::AddInputEvent(Example5::OnInput);
```

Example of defining a user input callback function

```cpp
bool __stdcall UI::Example5::OnInput(RE::InputEvent* event) { 
    bool blockThisUserInput = false;

    if (event->device == RE::INPUT_DEVICE::kKeyboard) {
        if (auto button = event->AsButtonEvent()) {
            if (button->GetIDCode() == RE::BSWin32KeyboardDevice::Key::kB && button->IsDown()) {
                NonPausingWindow->IsOpen = !NonPausingWindow->IsOpen;
                blockThisUserInput = true;
            }
        }
    }

    return blockThisUserInput;
}
```
## Foreground drawing

Register function signature

```cpp
Model::HudElement* AddHudElement(Model::HudElementCallback callback)
```

Example of registering a HUD drawing function

```cpp
SKSEMenuFramework::AddHudElement(Example5::RenderOverlay);
```
For example, the following code will render text on the top right corner of the screen if the user is not interacting with any menu
```cpp

void __stdcall UI::Example5::RenderOverlay() {
    if (SKSEMenuFramework::IsAnyBlockingWindowOpened()) {
        return;
    }
    auto drawList = ImGui::GetForegroundDrawList(); 

    const char* text = "Press B to toggle the info window";
    ImVec2 textSize;
    ImGui::CalcTextSize(&textSize, text,0, false, 0);
    ImVec2 textPos = ImVec2(ImGui::GetIO()->DisplaySize.x - textSize.x - 20, 20);  // 10px padding from edges
    ImGui::ImDrawListManager::AddText(drawList, textPos, IM_COL32(255, 255, 255, 255), text);
}
```
The following code will render a circle in the middle of the screen
```cpp
void __stdcall UI::Example5::RenderOverlay() {
    if (SKSEMenuFramework::IsAnyBlockingWindowOpened()) {
        return;
    }
    auto drawList = ImGui::GetForegroundDrawList(); 
    ImVec2 center = ImGui::GetIO()->DisplaySize;
    center.x *= 0.5;
    center.y *= 0.5;
    ImGui::ImDrawListManager::AddCircle(drawList, center, 100, IM_COL32(255, 0, 0, 255), 100, 10);
}
```
## Non blocking window

To create a non blocking window you can simply pass false to the second argument of the add window function

```cpp
UI::Example5::NonPausingWindow = SKSEMenuFramework::AddWindow(Example5::RenderWindow, false);
```

You can also change it later
```cpp
UI::Example5::NonPausingWindow->BlockUserInput = true;
```

## Load Texture File

SVG, DDS, and most conventional image files are supported
```cpp
inline ImTextureID LoadTexture(std::string texturePath, ImVec2 size = {0, 0})
```
Example of how to render images on imgui
```cpp
auto texture = SKSEMenuFramework::LoadTexture("Data\\interface\\unlit-bomb.svg", {100, 100});
auto texture2 = SKSEMenuFramework::LoadTexture("Data\\interface\\screenshot.png");
ImGui::Text("Image Display: ");
ImGui::SameLine();
ImGui::Image(texture, ImVec2(100, 100));
ImGui::Image(texture2, ImVec2(640, 360));
```

# Compiling and building

## Environment variables

[How to set up envioriment variables](https://gist.github.com/Thiago099/b45ec7832fb754325b29a61006bcd10c)

- COMMONLIB_SSE_FOLDER

  Clone [this](https://github.com/alandtse/CommonLibVR) Repository, to somewhere safe and adds its path to this environment variable on Windows.

```bash
git clone --recursive https://github.com/alandtse/CommonLibVR
cd CommonLibVR
git checkout ng
```
  
## Optional ouput folder optional variables

- SKYRIM_FOLDER
- WILDLANDER_OWRT_FOLDER
- SKYRIM_OWRT_FOLDER
- SKYRIM_MODS_FOLDER
- SKYRIM_MODS_FOLDER2
