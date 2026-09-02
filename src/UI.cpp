#include "UI.h"
#include "WindowManager.h"
#include <algorithm>
#include <imgui.h>
#include "Renderer.h"
#include "Application.h"
#include "SKSEMenuFramework.h"
#include "Translations.h"
#include "FontManager.h"
#include "RootMenuConfig.h"
static ImGuiTextFilter filter;

UI::MenuTree* UI::RootMenu = new UI::MenuTree();


int frame = 0;

UI::MenuTree* display_node;


static ImGuiTreeNodeFlags base_flags =
    ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
static int selection_mask = (1 << 2);

namespace {
    constexpr auto FAVORITE_STAR = "\xEF\x80\x85";
    constexpr auto ARCHIVE_ICON = "\xEF\x86\x87";
    constexpr ImVec4 FAVORITE_STAR_COLOR = ImVec4(1.0f, 0.84f, 0.0f, 1.0f);

    std::string pendingArchiveMenuName;
    UI::MenuTree* pendingArchiveMenu = nullptr;
    bool archiveConfirmationRequested = false;

    struct WindowSizeAndPosition {
        bool HasSavedState = false;
        ImVec2 Position{};
        ImVec2 Size{};
    };

    WindowSizeAndPosition mainWindowSizeAndPosition;
    WindowSizeAndPosition configWindowSizeAndPosition;

    void ApplyWindowSizeAndPosition(WindowSizeAndPosition& sizeAndPosition, const ImVec2& defaultPosition, const ImVec2& defaultSize,
                             const ImVec2& defaultPivot = ImVec2{0.0f, 0.0f}) {
        if (sizeAndPosition.HasSavedState) {
            ImGui::SetNextWindowPos(sizeAndPosition.Position, ImGuiCond_Appearing);
            ImGui::SetNextWindowSize(sizeAndPosition.Size, ImGuiCond_Appearing);
        } else {
            ImGui::SetNextWindowPos(defaultPosition, ImGuiCond_Appearing, defaultPivot);
            ImGui::SetNextWindowSize(defaultSize, ImGuiCond_Appearing);
        }
    }

    void SaveWindowSizeAndPosition(WindowSizeAndPosition& sizeAndPosition) {
        sizeAndPosition.Position = ImGui::GetWindowPos();
        sizeAndPosition.Size = ImGui::GetWindowSize();
        sizeAndPosition.HasSavedState = true;
    }

    ImVec2 GetCenteredWindowPosition(const ImGuiViewport* viewport, const ImVec2& size) {
        const auto center = viewport->GetCenter();
        return ImVec2{center.x - size.x * 0.5f, center.y - size.y * 0.5f};
    }

    void SetWindowSizeAndPosition(WindowSizeAndPosition& sizeAndPosition, const ImVec2& position, const ImVec2& size) {
        sizeAndPosition.Position = position;
        sizeAndPosition.Size = size;
        sizeAndPosition.HasSavedState = true;
    }

    void ResetBuiltInWindowSizeAndPosition(const ImGuiViewport* viewport) {
        const ImVec2 mainWindowSize{viewport->Size.x * 0.8f, viewport->Size.y * 0.8f};
        const ImVec2 mainWindowPosition = GetCenteredWindowPosition(viewport, mainWindowSize);
        SetWindowSizeAndPosition(mainWindowSizeAndPosition, mainWindowPosition, mainWindowSize);
        ImGui::SetWindowPos("#MCPMainWindow", mainWindowPosition, ImGuiCond_Always);
        ImGui::SetWindowSize("#MCPMainWindow", mainWindowSize, ImGuiCond_Always);

        const ImVec2 configWindowSize{viewport->Size.x * 0.4f, viewport->Size.y * 0.4f};
        const ImVec2 configWindowPosition = GetCenteredWindowPosition(viewport, configWindowSize);
        SetWindowSizeAndPosition(configWindowSizeAndPosition, configWindowPosition, configWindowSize);
        ImGui::SetWindowPos("Settings##Window", configWindowPosition, ImGuiCond_Always);
        ImGui::SetWindowSize("Settings##Window", configWindowSize, ImGuiCond_Always);
    }

    bool ContainsNode(const UI::MenuTree* root, const UI::MenuTree* target) {
        if (!root || !target) {
            return false;
        }
        if (root == target) {
            return true;
        }

        for (const auto& child : root->Children) {
            if (ContainsNode(child.second, target)) {
                return true;
            }
        }
        return false;
    }

    void SetRootMenuArchived(const std::string& menuName, UI::MenuTree* menu, bool archived) {
        RootMenuConfig::SetArchived(menuName, archived);
        if (archived && ContainsNode(menu, display_node)) {
            display_node = nullptr;
        }
    }

    void RenderTooltip(const char* translationKey) {
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(Translations::Get(translationKey));
            ImGui::EndTooltip();
        }
    }

    void RenderRootMenuButtons(const std::string& menuName, UI::MenuTree* menu, bool favorite) {
        const auto headerMin = ImGui::GetItemRectMin();
        const auto headerMax = ImGui::GetItemRectMax();
        const auto restoreCursor = ImGui::GetCursorScreenPos();
        const float buttonSize = headerMax.y - headerMin.y;

        ImGui::PushID(menuName.c_str());
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        ImGui::SetCursorScreenPos(ImVec2(headerMax.x - buttonSize * 2.0f, headerMin.y));
        ImGui::PushStyleColor(
            ImGuiCol_Text, favorite ? FAVORITE_STAR_COLOR : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (ImGui::Button(FAVORITE_STAR, ImVec2(buttonSize, buttonSize))) {
            RootMenuConfig::SetFavorite(menuName, !favorite);
        }
        ImGui::PopStyleColor();
        RenderTooltip(favorite ? "Menu.RemoveFavorite" : "Menu.AddFavorite");

        ImGui::SetCursorScreenPos(ImVec2(headerMax.x - buttonSize, headerMin.y));
        if (ImGui::Button(ARCHIVE_ICON, ImVec2(buttonSize, buttonSize))) {
            pendingArchiveMenuName = menuName;
            pendingArchiveMenu = menu;
            archiveConfirmationRequested = true;
        }
        RenderTooltip("Menu.Archive");

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopID();
        ImGui::SetCursorScreenPos(restoreCursor);
    }

    void RenderArchivedMenuRecovery() {
        if (ImGui::BeginMenu(Translations::Get("Options.RestoreArchivedMenus"))) {
            bool hasArchivedMenus = false;
            for (const auto& [menuName, menu] : UI::RootMenu->Children) {
                if (!RootMenuConfig::IsArchived(menuName)) {
                    continue;
                }

                hasArchivedMenus = true;
                ImGui::PushID(menuName.c_str());
                if (ImGui::MenuItem(menuName.c_str())) {
                    SetRootMenuArchived(menuName, menu, false);
                }
                ImGui::PopID();
            }

            if (!hasArchivedMenus) {
                ImGui::MenuItem(Translations::Get("Options.NoArchivedMenus"), nullptr, false, false);
            }
            ImGui::EndMenu();
        }
    }

    void RenderArchiveConfirmation() {
        const auto popupTitle = std::format("{}##ArchiveRootMenuConfirmation",
            Translations::Get("Menu.Archive.Title"));
        if (archiveConfirmationRequested) {
            ImGui::OpenPopup(popupTitle.c_str());
            archiveConfirmationRequested = false;
        }

        const auto viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal(popupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted(Translations::Get("Menu.Archive.Confirm"));
            ImGui::TextUnformatted(pendingArchiveMenuName.c_str());
            ImGui::Separator();

            if (ImGui::Button(Translations::Get("Menu.Archive.Yes"))) {
                SetRootMenuArchived(pendingArchiveMenuName, pendingArchiveMenu, true);
                pendingArchiveMenuName.clear();
                pendingArchiveMenu = nullptr;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(Translations::Get("Menu.Archive.No"))) {
                pendingArchiveMenuName.clear();
                pendingArchiveMenu = nullptr;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

void DummyRenderer(std::pair<const std::string, UI::MenuTree*>& node) {
    for (auto& item : node.second->Children) {
        DummyRenderer(item);
    }
}

void RenderNode(std::pair<const std::string, UI::MenuTree*>& node) {
    ImGuiTreeNodeFlags node_flags = base_flags;
    if (display_node == node.second) node_flags |= ImGuiTreeNodeFlags_Selected;

    if (node.second->Children.size() == 0) {
        node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    bool node_open = ImGui::TreeNodeEx(node.second, node_flags, "%s", node.first.c_str());


    bool itemClicked = ImGui::IsItemClicked();
    bool itemToggledOpen = ImGui::IsItemToggledOpen();
    bool gamepadButtonPressed = ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown);  // Typically A button
    bool itemIsFocused = ImGui::IsItemFocused();  // Check if the item is focused/highlighted by gamepad navigation

    if ((itemClicked || (gamepadButtonPressed && itemIsFocused)) && !itemToggledOpen) {
        if (node.second->Render) {
            display_node = node.second;
        }
    }
    if (node_open && node.second->Children.size() != 0) {
        for (auto& item : node.second->SortedChildren) {
            RenderNode(item);
        }
        ImGui::TreePop();
    } else {
        for (auto& item : node.second->Children) {
            DummyRenderer(item);
        }
    }
}

void __stdcall UI::RenderMenuWindow() {
    auto viewport = ImGui::GetMainViewport();
    ApplyWindowSizeAndPosition(mainWindowSizeAndPosition, viewport->GetCenter(), ImVec2{viewport->Size.x * 0.8f, viewport->Size.y * 0.8f},
                        ImVec2{0.5f, 0.5f});
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_MenuBar;
    window_flags |= ImGuiWindowFlags_NoTitleBar;

    ImGui::Begin("#MCPMainWindow", nullptr, window_flags);
    SaveWindowSizeAndPosition(mainWindowSizeAndPosition);

    if (ImGui::BeginMenuBar()) {
        PushSolid();
        if (ImGui::BeginMenu(Translations::Get("Options"))) {
            if (ImGui::MenuItem(Translations::Get("Settings.ResetWindows"))) {
                ResetBuiltInWindowSizeAndPosition(viewport);
            }

            if (ImGui::MenuItem(Translations::Get("Options.ResumeGame"))) {
                WindowManager::MainInterface->BlockUserInput = false;
                WindowManager::ConfigInterface->BlockUserInput = false;
            }
            if (ImGui::MenuItem(Translations::Get("Options.OpenSettings"))) {
                WindowManager::ConfigInterface->IsOpen = true;
            }
            ImGui::Separator();
            RenderArchivedMenuRecovery();
            ImGui::EndMenu();
        }
        Pop();

        float barWidth = ImGui::GetWindowWidth();
        float barHeight = ImGui::GetFrameHeight();
        float textWidth = ImGui::CalcTextSize(Translations::Get("ModControlPanel")).x;

        float closeButtonSize = barHeight;
        float padding = ImGui::GetStyle().ItemSpacing.x;

        float availableWidth = barWidth - closeButtonSize - padding;
        float pos = (availableWidth * 0.5f) - (textWidth * 0.5f);
        ImGui::SameLine(pos);
        ImGui::Text(Translations::Get("ModControlPanel"));

        float closeButtonPos = barWidth - closeButtonSize - padding;
        ImGui::SameLine(closeButtonPos);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        if (ImGui::Button("X", ImVec2(closeButtonSize, closeButtonSize))) {
            WindowManager::Close();
        }
        ImGui::PopStyleVar();

        ImGui::EndMenuBar();
    }

    float filterHeight = 50.0f;
    float headerHeight = 41.0f;
    float headerOffsetY = 5.0f;



    // Filter section
    ImGui::BeginChild("TreeView2", ImVec2(ImGui::GetContentRegionAvail().x * 0.3f, filterHeight), ImGuiChildFlags_None);
    filter.Draw("##SKSEModControlPanelMenuFilter", -FLT_MIN);
    ImGui::EndChild();

    ImGui::SameLine();

    // Header section
    ImGui::BeginChild("SKSEModControlPanelModMenuHeader", ImVec2(0, headerHeight), ImGuiChildFlags_None);
    if (display_node) {
        auto windowWidth = ImGui::GetWindowSize().x;
        auto textWidth = ImGui::CalcTextSize(display_node->Title.c_str()).x;
        float offsetX = (windowWidth - textWidth) * 0.5f;
        ImGui::SetCursorPosX(offsetX);
        ImGui::SetCursorPosY(headerOffsetY);
        ImGui::Text("%s", display_node->Title.c_str());
    }
    ImGui::EndChild();

    // Tree view section
    ImGui::BeginChild("SKSEModControlPanelTreeView", ImVec2(ImGui::GetContentRegionAvail().x * 0.3f, -FLT_MIN),
                      ImGuiChildFlags_Border);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 5.0f));
    std::vector<const std::pair<const std::string, UI::MenuTree*>*> rootMenus;
    rootMenus.reserve(RootMenu->Children.size());
    for (const auto& item : RootMenu->Children) {
        rootMenus.push_back(&item);
    }
    std::stable_sort(rootMenus.begin(), rootMenus.end(), [](const auto* left, const auto* right) {
        const bool leftFavorite = RootMenuConfig::IsFavorite(left->first);
        const bool rightFavorite = RootMenuConfig::IsFavorite(right->first);
        if (leftFavorite != rightFavorite) {
            return leftFavorite;
        }
        return left->first < right->first;
    });

    for (const auto* rootMenu : rootMenus) {
        const auto& item = *rootMenu;
        if (RootMenuConfig::IsArchived(item.first)) {
            for (auto node : item.second->Children) {
                DummyRenderer(node);
            }
            continue;
        }

        const bool favorite = RootMenuConfig::IsFavorite(item.first);
        const bool passesFilter = filter.PassFilter(item.first.c_str());
        const auto headerLabel = std::format("{}##RootMenu-{}", item.first, item.first);
        const auto headerFlags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_ClipLabelForTrailingButton;
        const bool headerOpen = passesFilter && ImGui::CollapsingHeader(headerLabel.c_str(), headerFlags);

        if (passesFilter) {
            RenderRootMenuButtons(item.first, item.second, favorite);
        }

        if (RootMenuConfig::IsArchived(item.first)) {
            for (auto node : item.second->Children) {
                DummyRenderer(node);
            }
        } else if (headerOpen) {
            for (auto node : item.second->SortedChildren) {
                RenderNode(node);
            }
        } else {
            for (auto node : item.second->Children) {
                DummyRenderer(node);
            }
        }
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::SameLine();

    // Content section
    ImGui::BeginChild("SKSEModControlPanelMenuNode", ImVec2(0, -FLT_MIN), ImGuiChildFlags_Border);
    if (display_node) {
        display_node->Render();
    }
    ImGui::EndChild();

    RenderArchiveConfirmation();

    ImGui::End();
}

UI::BackAction UI::ResolveBack() {
    // Let ImGui cancel first: a modal, or an in-progress edit, is "inside" the
    // page and has to unwind before the page itself does.
    if (ImGui::IsAnyItemActive() ||
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        return BackAction::PassToImGui;
    }

    // A page is showing: back means return to the tree, not leave the menu.
    if (display_node) {
        display_node = nullptr;
        return BackAction::PoppedPage;
    }

    return BackAction::CloseMenu;
}

void UI::AddToTree(UI::MenuTree* node, std::vector<std::string>& path, RenderFunction render, std::string title) {
    if (!path.empty()) {
        auto currentName = path.front();
        path.erase(path.begin());

        auto foundItem = node->Children.find(currentName);
        if (foundItem != node->Children.end()) {
            AddToTree(foundItem->second, path, render, title);
        } else {
            auto newItem = new UI::MenuTree();
            node->Children[currentName] = newItem;
            node->SortedChildren.push_back(std::pair<const std::string, UI::MenuTree*>(currentName, newItem));
            AddToTree(newItem, path, render, title);
        }
    } else {
        node->Render = render;
        node->Title = title;
    }
}

bool ToggleButton(const char* label, bool* v) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float height = ImGui::GetFrameHeight();
    float width = height * 1.8f;
    float radius = height * 0.5f;

    // Use a unique ID for the button
    ImGui::PushID(label);

    // Use Selectable instead of InvisibleButton for gamepad navigation
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));

    bool clicked = ImGui::Selectable("##toggle", false, 0, ImVec2(width, height));

    ImGui::PopStyleColor(3);
    ImGui::PopID();

    if (clicked) {
        *v = !*v;
    }

    // Draw the toggle on top of the selectable
    ImVec2 p_min = ImVec2(p.x, p.y);
    ImVec2 p_max = ImVec2(p.x + width, p.y + height);

    float t = *v ? 1.0f : 0.0f;
    ImU32 col_bg = ImGui::GetColorU32(*v ? ImGuiCol_ButtonActive : ImGuiCol_ButtonActive);

    draw_list->AddRectFilled(p_min, p_max, col_bg, height * 0.5f);
    draw_list->AddCircleFilled(ImVec2(p.x + radius + t * (width - radius * 2.0f), p.y + radius), radius - 1.5f,
                               IM_COL32(255, 255, 255, 255));

    ImGui::SameLine();
    ImGui::Text("%s", label);

    return clicked;
}

void UI::RenderConfigWindow() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ApplyWindowSizeAndPosition(configWindowSizeAndPosition, viewport->GetCenter(), ImVec2{viewport->Size.x * 0.4f, viewport->Size.y * 0.4f},
                        ImVec2{0.5f, 0.5f});
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_MenuBar;
    window_flags |= ImGuiWindowFlags_NoTitleBar;

    bool shouldRenderContent = ImGui::Begin("Settings##Window", nullptr, window_flags);
    SaveWindowSizeAndPosition(configWindowSizeAndPosition);
    if (shouldRenderContent) {
        if (ImGui::BeginMenuBar()) {
            ImGui::Text(Translations::Get("Settings.Title"));
            float barWidth = ImGui::GetWindowWidth();
            float barHeight = ImGui::GetFrameHeight();
            float textWidth = ImGui::CalcTextSize(Translations::Get("Settings.Title")).x;

            float closeButtonSize = barHeight;
            float padding = ImGui::GetStyle().ItemSpacing.x;

            float closeButtonPos = barWidth - closeButtonSize - padding;
            ImGui::SameLine(closeButtonPos);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            if (ImGui::Button("X", ImVec2(closeButtonSize, closeButtonSize))) {
                WindowManager::ConfigInterface->IsOpen = false;
            }
            ImGui::PopStyleVar();
            ImGui::EndMenuBar();
        }

        float windowWidth = ImGui::GetContentRegionAvail().x;
        float contentWidth = windowWidth * 0.8f;
        float offsetX = (windowWidth - contentWidth) * 0.5f;

        if (offsetX > 0) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        }

        ImGui::BeginGroup();
        ImGui::PushItemWidth(contentWidth);

        // ... all your combo boxes and settings ...

        std::vector<const char*> styleNames;
        styleNames.reserve(Config::MenuStyles.size());

        for (const auto& s : Config::MenuStyles) {
            styleNames.push_back(s.c_str());
        }

        ImGui::Text(Translations::Get("Settings.MenuStyle"));
        if (ImGui::Combo("##MenuStyleCombo", &Config::MenuStyle, styleNames.data(), styleNames.size())) {
            Config::LoadStyle();
            Config::Save();
        }
        if (ToggleButton(Translations::Get("Settings.FreezeTime"), &Config::FreezeTimeOnMenu)) {
            Config::Save();
        }

        if (ToggleButton(Translations::Get("Settings.BlurBackground"), &Config::BlurBackgroundOnMenu)) {
            Config::Save();
        }

        static float pendingFontSize = Config::FontSizeMedium;
        const auto fontSizeLabel = std::format("{} ({:g}-{:g}):", Translations::Get("Settings.FontSize"),
            Config::MinFontSize, Config::MaxFontSize);
        ImGui::TextUnformatted(fontSizeLabel.c_str());
        ImGui::InputFloat("##FontSize", &pendingFontSize, 1.0f, 4.0f, "%.1f");
        if (pendingFontSize != Config::FontSizeMedium) {
            if (ImGui::Button(Translations::Get("Settings.FontSize.Apply"))) {
                Config::FontSizeMedium = Config::NormalizeFontSize(pendingFontSize);
                pendingFontSize = Config::FontSizeMedium;
                Config::Save();
                FontManager::RequestAtlasRebuild();
            }
        }

        const char* togleModeNames[] = {"SINGLEPRESS", "HOLD", "DOUBLEPRESS", "OFF"};
        int currentTogleMode = static_cast<int>(Config::ToggleMode);
        ImGui::Text(Translations::Get("Settings.ToggleMode.Keyboard"));
        if (ImGui::Combo("##ToggleModeCombo", &currentTogleMode, togleModeNames, IM_ARRAYSIZE(togleModeNames))) {
            Config::ToggleMode = currentTogleMode;
            Config::Save();
        }

        ImGui::Separator();

        ImGui::Text(Translations::Get("Settings.ToggleKey.Keyboard"));
        std::string currentKeyName = GetKeyName(Config::ToggleKey, RE::INPUT_DEVICES::kKeyboard);
        if (ImGui::BeginCombo("##ToggleKeyKeyboard", currentKeyName.c_str())) {
            const std::vector<std::pair<std::string, int>> keyboardKeys = {
                {"NONE", 0x00},       {"ESCAPE", 0x01},      {"1", 0x02},           {"2", 0x03},
                {"3", 0x04},          {"4", 0x05},           {"5", 0x06},           {"6", 0x07},
                {"7", 0x08},          {"8", 0x09},           {"9", 0x0A},           {"0", 0x0B},
                {"MINUS", 0x0C},      {"EQUALS", 0x0D},      {"BACKSPACE", 0x0E},   {"TAB", 0x0F},
                {"Q", 0x10},          {"W", 0x11},           {"E", 0x12},           {"R", 0x13},
                {"T", 0x14},          {"Y", 0x15},           {"U", 0x16},           {"I", 0x17},
                {"O", 0x18},          {"P", 0x19},           {"BRACKETLEFT", 0x1A}, {"BRACKETRIGHT", 0x1B},
                {"ENTER", 0x1C},      {"LEFTCONTROL", 0x1D}, {"A", 0x1E},           {"S", 0x1F},
                {"D", 0x20},          {"F", 0x21},           {"G", 0x22},           {"H", 0x23},
                {"J", 0x24},          {"K", 0x25},           {"L", 0x26},           {"SEMICOLON", 0x27},
                {"APOSTROPHE", 0x28}, {"TILDE", 0x29},       {"LEFTSHIFT", 0x2A},   {"BACKSLASH", 0x2B},
                {"Z", 0x2C},          {"X", 0x2D},           {"C", 0x2E},           {"V", 0x2F},
                {"B", 0x30},          {"N", 0x31},           {"M", 0x32},           {"COMMA", 0x33},
                {"PERIOD", 0x34},     {"SLASH", 0x35},       {"RIGHTSHIFT", 0x36},  {"KP_MULTIPLY", 0x37},
                {"LEFTALT", 0x38},    {"SPACEBAR", 0x39},    {"CAPSLOCK", 0x3A},    {"F1", 0x3B},
                {"F2", 0x3C},         {"F3", 0x3D},          {"F4", 0x3E},          {"F5", 0x3F},
                {"F6", 0x40},         {"F7", 0x41},          {"F8", 0x42},          {"F9", 0x43},
                {"F10", 0x44},        {"NUMLOCK", 0x45},     {"SCROLLLOCK", 0x46},  {"KP_7", 0x47},
                {"KP_8", 0x48},       {"KP_9", 0x49},        {"KP_SUBTRACT", 0x4A}, {"KP_4", 0x4B},
                {"KP_5", 0x4C},       {"KP_6", 0x4D},        {"KP_PLUS", 0x4E},     {"KP_1", 0x4F},
                {"KP_2", 0x50},       {"KP_3", 0x51},        {"KP_0", 0x52},        {"KP_DECIMAL", 0x53},
                {"F11", 0x57},        {"F12", 0x58},         {"KP_ENTER", 0x9C},    {"RIGHTCONTROL", 0x9D},
                {"KP_DIVIDE", 0xB5},  {"PRINTSCREEN", 0xB7}, {"RIGHTALT", 0xB8},    {"PAUSE", 0xC5},
                {"HOME", 0xC7},       {"UP", 0xC8},          {"PAGEUP", 0xC9},      {"LEFT", 0xCB},
                {"RIGHT", 0xCD},      {"END", 0xCF},         {"DOWN", 0xD0},        {"PAGEDOWN", 0xD1},
                {"INSERT", 0xD2},     {"DELETE", 0xD3},      {"LEFTWIN", 0xDB},     {"RIGHTWIN", 0xDC}};

            for (const auto& [name, code] : keyboardKeys) {
                bool isSelected = (Config::ToggleKey == code);
                if (ImGui::Selectable(name.c_str(), isSelected)) {
                    Config::ToggleKey = code;
                    Config::Save();
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        int currentTogleModeGamepad = static_cast<int>(Config::ToggleModeGamePad);
        ImGui::Text(Translations::Get("Settings.ToggleMode.Gamepad"));
        if (ImGui::Combo("##ToggleModeComboGAmepad", &currentTogleModeGamepad, togleModeNames,
                         IM_ARRAYSIZE(togleModeNames))) {
            Config::ToggleModeGamePad = currentTogleModeGamepad;
            Config::Save();
        }

        ImGui::Text(Translations::Get("Settings.ToggleKey.Gamepad"));
        std::string currentButtonName = GetKeyName(Config::ToggleKeyGamePad, RE::INPUT_DEVICES::kGamepad);
        if (ImGui::BeginCombo("##ToggleKeyGamepad", currentButtonName.c_str())) {
            const std::vector<std::pair<std::string, int>> gamepadButtons = {
                {"NONE", 0},  {"DPAD_UP", 1}, {"DPAD_DOWN", 2}, {"DPAD_LEFT", 4}, {"DPAD_RIGHT", 8}, {"START", 16},
                {"BACK", 32}, {"LS", 64},     {"RS", 128},      {"LB", 256},      {"RB", 512},       {"LT", 9},
                {"RT", 10},   {"A", 4096},    {"B", 8192},      {"X", 16384},     {"Y", 32768}};

            for (const auto& [name, code] : gamepadButtons) {
                bool isSelected = (Config::ToggleKeyGamePad == code);
                if (ImGui::Selectable(name.c_str(), isSelected)) {
                    Config::ToggleKeyGamePad = code;
                    Config::Save();
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::PopItemWidth();  // ADDED: Pop the item width
        ImGui::EndGroup();      // ADDED: End the group
    }
    ImGui::End();  // MOVED: Always call End() after Begin()
}
