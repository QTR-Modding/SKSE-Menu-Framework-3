#pragma once

#include "WindowManager.h"

namespace UI {
    enum class MenuMutationType {
        Rename,
        Delete
    };

    struct MenuMutation {
        MenuMutationType Type;
        std::string Path;
        std::string NewName;
    };

    class MenuTree {
    public:
        std::map<std::string, MenuTree*> Children;
        std::vector<std::pair<const std::string, MenuTree*>> SortedChildren;
        RenderFunction Render;
        std::string Title;
    };
    extern UI::MenuTree* RootMenu;
    void __stdcall RenderMenuWindow();
    void AddToTree(UI::MenuTree* node, std::vector<std::string>& path, RenderFunction render, std::string title);
    bool QueueMenuMutation(MenuMutation mutation);
    void ApplyPendingMenuMutations();
    void __stdcall RenderConfigWindow();
}
