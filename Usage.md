
import the SKSEMenuFramework.h into your header file, i reccomend you creating a UI.cpp/UI.h 
```cpp
#include "SKSEMenuFramework.h"
```

Define the render function for your menu entry. Here is an example:

```cpp
void __stdcall UI::Example1::Render() {
    ImGui::InputScalar("form id", ImGuiDataType_U32, &AddFormId, NULL, NULL, "%08X");

    if (ImGui::Button("Search")) {
        LookupForm();
    }

    if (AddBoundObject) {
        ImGui::Text("How much %s would you like to add?", AddBoundObject->GetName());
        ImGui::SliderInt("number", &Configuration::Example1::Number, 1, 100);
        if (ImGui::Button("Add")) {
            auto player = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();
            player->AddObjectToContainer(AddBoundObject, nullptr, Configuration::Example1::Number, nullptr);
        }
    } else {
        ImGui::Text("Form not found");
    }
}
```

You should create a function in order to register your menu entries:

You should check if the user has the menu framework installed before doing anything in the register function

```cpp
if (!SKSEMenuFramework::IsInstalled()) {
    return;
}
```

Before registering any entries, you should choose a section for your menu to be in. It is recommended that you use your mod name as the section name to keep things organized

```cpp
SKSEMenuFramework::SetSection("<menu section name>");
```

Register your menu entry, it will be a page on the Mod Control Panel

```cpp
SKSEMenuFramework::AddSectionItem("Add Item", Example1::Render);
```
Here is what this example will look like (The style of the picture is outdated):

![image](https://github.com/Thiago099/SKSE-Menu-Framework-SDK/assets/66787043/8ebcd191-55a3-498b-bf36-0ca7337eff3a)

## Renaming and deleting pages at runtime

Page paths are relative to the section selected with `SetSection`. Use the complete relative path for nested pages:

```cpp
SKSEMenuFramework::SetSection("My Mod");

SKSEMenuFramework::RenameSectionItem("Settings/Gameplay", "Combat");
SKSEMenuFramework::DeleteSectionItem("Settings/Combat");
```

Rename or delete the current section with:

```cpp
SKSEMenuFramework::RenameSection("My Renamed Mod");
SKSEMenuFramework::DeleteSection();
```

These functions return `true` when the request is accepted. They return `false` for invalid or missing paths, name collisions, or when the installed framework does not support the operation. A new name must be a single non-empty path segment.

Renaming an open page preserves its selection. Deleting the open page, or deleting a section that contains it, clears the selection. The framework does not automatically select another page.

## Font Awesome

Header file
```cpp
namespace Example4 {
	inline std::string TitleText = "This is an " + FontAwesome::UnicodeToUtf8(0xf2b4) + " Font Awesome usage example";
	inline std::string Button1Text = FontAwesome::UnicodeToUtf8(0xf0e9) + " Umbrella";
	inline std::string Button2Text = FontAwesome::UnicodeToUtf8(0xf06e) + " Eye";
	void __stdcall Render();
}
```
cpp file

```cpp
void __stdcall UI::Example4::Render() {
    FontAwesome::PushBrands();
    ImGui::Text(TitleText.c_str());
    FontAwesome::Pop();

    FontAwesome::PushSolid();
    ImGui::Button(Button1Text.c_str());
    FontAwesome::Pop();

    ImGui::SameLine();

    FontAwesome::PushRegular();
    ImGui::Button(Button2Text.c_str());
    FontAwesome::Pop();
}
```
Here is what this example will look like:

![image](https://github.com/Thiago099/SKSE-Menu-Framework-SDK/assets/66787043/c3b7a913-fbb9-41be-ae38-d4c9efa8e2b3)


You can browse icons and get the Unicode IDs from the [Font Awesome](https://fontawesome.com/search?o=r&m=free) website
![image](https://github.com/Thiago099/SKSE-Menu-Framework-SDK/assets/66787043/ec5f14f1-5658-4f6e-8e60-2342f47f078e)

All `.ttf` and `.otf` files in `Data/SKSE/Plugins/Fonts` are loaded at the small, default, and big configured sizes. Select one by filename or by its filename without the extension:

```cpp
SKSEMenuFramework::PushFont("SkyrimMenuFont.ttf"); // "SkyrimMenuFont" also works
ImGui::Text("This text uses the selected font.");
FontAwesome::Pop();
```

To override those three sizes for one font, place a JSON file with the same base name beside it. For example, `MyFont.json` configures `MyFont.ttf`:

```json
{
  "small": 16.0,
  "default": 32.0,
  "big": 64.0
}
```



## Creating your own windows

Define your window render function

```cpp
void __stdcall UI::Example2::RenderWindow() {
    auto viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{viewport->Size.x * 0.4f, viewport->Size.y * 0.4f}, ImGuiCond_Appearing);
    ImGui::Begin("My First Tool##MenuEntiryFromMod",nullptr, ImGuiWindowFlags_MenuBar); // If two mods have the same window name, and they open at the same time.
                                                                                         // The window content will be merged, is good practice to add ##ModName after the window name.
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */
            }
            if (ImGui::MenuItem("Close", "Ctrl+W")) {
                ExampleWindow->IsOpen = false;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    if (ImGui::Button("Close Window")) {
        ExampleWindow->IsOpen = false;
    }
    ImGui::End();
}
```

Register your window. The register method will return an object with which you can set the `IsOpen` property from anywhere to open and close your window

```cpp
UI::Example2::ExampleWindow = SKSEMenuFramework::AddWindow(Example2::RenderWindow);
UI::Example2::ExampleWindow->IsOpen = true; 
```

Here is the example header

```cpp
namespace Example2{
void __stdcall RenderWindow();
inline MENU_WINDOW ExampleWindow;
}
```

Here is what your window will look like:

![image](https://github.com/Thiago099/SKSE-Menu-Framework-SDK/assets/66787043/c301cc1b-d435-47ad-9bdc-a635fa385986)
