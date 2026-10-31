#pragma once

#include <cstdint>

#include "MenuPath.h"
#include "WindowManager.h"

namespace UI {
    enum class MenuMutationType {
        Rename,
        Delete
    };

    struct MenuMutation {
        MenuMutationType Type;
        MenuPath::Segments Path;
        std::string NewName;
        std::string RequestedPath;
    };

    class MenuTree {
    public:
        std::map<std::string, MenuTree*> Children;
        std::vector<std::pair<const std::string, MenuTree*>> SortedChildren;
        RenderFunction Render;
        std::string Title;
        uint32_t Flags = 0;
    };
    extern UI::MenuTree* RootMenu;

    // What a gamepad "back" press should do, given where the menu currently is.
    enum class BackAction {
        PassToImGui,  // a popup is open or a widget is being edited - ImGui's own cancel owns it
        PoppedPage,   // a page was open; went back to the tree
        CloseMenu     // already at the root, so back means close
    };
    BackAction ResolveBack();
    void __stdcall RenderMenuWindow();
    void AddToTree(UI::MenuTree* node, std::vector<std::string>& path, RenderFunction render, std::string title);
    bool QueueMenuMutation(MenuMutationType type, std::string_view path, std::string_view newName = {});
    bool SetMenuSectionFlags(std::string_view path, uint32_t flags, uint32_t mask);
    bool GetMenuSectionFlags(std::string_view path, uint32_t* out);
    void ApplyPendingMenuMutations();
    void SaveWindowSettings();
    void __stdcall RenderConfigWindow();
}
