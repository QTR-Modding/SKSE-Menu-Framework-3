#include "UI.h"
#include "WindowManager.h"
#include "GamepadNavigation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <imgui.h>
#include <imgui_internal.h>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <vector>
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
    constexpr auto CATEGORY_ICON = "\xEF\x80\xAB";
    constexpr auto ROOT_MENU_PAYLOAD = "SMF_ROOT_MENU";
    constexpr ImVec4 FAVORITE_STAR_COLOR = ImVec4(1.0f, 0.84f, 0.0f, 1.0f);

    using RootMenuEntry = std::pair<const std::string, UI::MenuTree*>;

    struct RootMenuDrop {
        std::string MenuName;
        std::string CategoryName;
        std::string TargetMenuName;
        bool PlaceAfter = false;
    };

    struct RootMenuDropPreview {
        RootMenuDrop Placement;
        ImRect HeaderBounds;
        float InsertionY = 0.0f;
    };

    bool rootMenuMoveErrorRequested = false;

    struct MenuNodeLocation {
        UI::MenuTree* Parent = nullptr;
        UI::MenuTree* Node = nullptr;
        std::string Name;
    };

    std::recursive_mutex menuTreeMutex;
    std::deque<UI::MenuMutation> pendingMenuMutations;
    std::set<MenuPath::Segments> projectedMenuPaths;

    std::string pendingArchiveMenuName;
    UI::MenuTree* pendingArchiveMenu = nullptr;
    bool archiveConfirmationRequested = false;

    enum class CategoryEditorMode { Create, Rename, Delete };

    struct CategoryEditor {
        CategoryEditorMode Mode = CategoryEditorMode::Create;
        std::string OriginalName;
        std::array<char, 256> Name{};
        UI::MenuTree* Menu = nullptr;
        std::string MenuName;
        const char* Error = nullptr;
        bool AssignToMenu = false;
        bool FromManager = false;
        bool OpenRequested = false;
    } categoryEditor;

    bool categoryManagerRequested = false;
    std::optional<std::string> categoryToOpen;
    const char* categoryAssignmentError = nullptr;

    struct WindowSizeAndPosition {
        const char* Section;
        bool HasSavedState = false;
        bool PendingSave = false;
        ImVec2 Position{};
        ImVec2 Size{};
    };

    WindowSizeAndPosition mainWindowSizeAndPosition{"MainWindow"};
    WindowSizeAndPosition configWindowSizeAndPosition{"SettingsWindow"};

    struct PendingToggleChange {
        RE::INPUT_DEVICE Device = RE::INPUT_DEVICE::kNone;
        unsigned int Key = UI::KeyBindingCapture::UnboundKey;
        std::uint8_t Mode = 0;
        const char* Warning = nullptr;
        bool OpenRequested = false;
    } pendingToggleChange;

    void SaveToggleChange(RE::INPUT_DEVICE device, unsigned int key, std::uint8_t mode) {
        const bool keyboard = device == RE::INPUT_DEVICE::kKeyboard;
        (keyboard ? Config::ToggleKey : Config::ToggleKeyGamePad) = key;
        (keyboard ? Config::ToggleMode : Config::ToggleModeGamePad) = mode;
        Config::Save();
    }

    void RequestToggleChange(RE::INPUT_DEVICE device, unsigned int key, std::uint8_t mode) {
        const bool keyboard = device == RE::INPUT_DEVICE::kKeyboard;
        const auto currentKey = keyboard ? Config::ToggleKey : Config::ToggleKeyGamePad;
        const auto currentMode = keyboard ? Config::ToggleMode : Config::ToggleModeGamePad;
        if (key == currentKey && mode == currentMode) {
            return;
        }

        const char* warning = nullptr;
        if ((key == UI::KeyBindingCapture::UnboundKey && key != currentKey) ||
            (mode == GetToggleMode("OFF") && mode != currentMode)) {
            warning = "Settings.Binding.NoShortcut";
        } else if (!keyboard && (key == RE::BSWin32GamepadDevice::Key::kA || key == RE::BSWin32GamepadDevice::Key::kB) &&
                   (key != currentKey || (currentMode == GetToggleMode("OFF") && mode != currentMode))) {
            warning = key == RE::BSWin32GamepadDevice::Key::kA
                ? "Settings.Binding.ConfirmConflict" : "Settings.Binding.BackConflict";
        }
        if (warning) {
            pendingToggleChange = {device, key, mode, warning, true};
            UI::keyBindingCapture.BeginConfirmation();
        } else {
            SaveToggleChange(device, key, mode);
        }
    }

    void RenderBindingConfirmation() {
        const auto title = std::format("{}###BindingWarning", Translations::Get("Settings.Binding.Warning"));
        if (pendingToggleChange.OpenRequested) {
            ImGui::OpenPopup(title.c_str());
            pendingToggleChange.OpenRequested = false;
        }
        if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (!UI::keyBindingCapture.IsConfirming()) {
                UI::keyBindingCapture.Reset();
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }
            ImGui::TextUnformatted(Translations::Get(pendingToggleChange.Warning));
            ImGui::Separator();
            if (ImGui::IsWindowAppearing()) {
                ImGui::NavRestoreHighlightAfterMove();
            }
            const bool cancel = ImGui::Button(Translations::Get("Settings.Binding.Cancel"));
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            const bool confirm = ImGui::Button(Translations::Get("Settings.Binding.Continue"));
            if (cancel || confirm) {
                if (confirm && !cancel) {
                    SaveToggleChange(pendingToggleChange.Device, pendingToggleChange.Key, pendingToggleChange.Mode);
                }
                UI::keyBindingCapture.Reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void RenderKeyBinding(const char* id, unsigned int binding, RE::INPUT_DEVICE device) {
        ImGui::PushID(id);
        const auto mode = device == RE::INPUT_DEVICE::kKeyboard ? Config::ToggleMode : Config::ToggleModeGamePad;
        const auto title = std::format("{}###CaptureBinding", Translations::Get("Settings.Binding.Title"));
        const auto clearLabel = Translations::Get("Settings.Binding.Clear");
        const auto clearWidth = ImGui::CalcTextSize(clearLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        const auto bindingWidth = ImMax(ImGui::CalcItemWidth() - clearWidth - ImGui::GetStyle().ItemSpacing.x,
            ImGui::GetFrameHeight());
        if (ImGui::Button(GetKeyName(binding, device).c_str(), ImVec2{bindingWidth, 0.0f})) {
            UI::keyBindingCapture.Begin(device);
            ImGui::OpenPopup(title.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button(clearLabel) && binding != UI::KeyBindingCapture::UnboundKey) {
            RequestToggleChange(device, UI::KeyBindingCapture::UnboundKey, mode);
        }
        if (ImGui::BeginPopupModal(title.c_str(), nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNavInputs)) {
            unsigned int newKey = binding;
            auto state = UI::keyBindingCapture.Poll(newKey, device);
            ImGui::TextUnformatted(Translations::Get(device == RE::INPUT_DEVICE::kKeyboard
                ? "Settings.Binding.Keyboard" : "Settings.Binding.Gamepad"));
            ImGui::BeginDisabled(state == UI::KeyBindingCapture::State::Pressed);
            if (ImGui::Button(Translations::Get("Settings.Binding.Cancel"))) {
                state = UI::KeyBindingCapture::State::Cancelled;
            }
            ImGui::EndDisabled();
            if (state == UI::KeyBindingCapture::State::Idle ||
                state == UI::KeyBindingCapture::State::Complete || state == UI::KeyBindingCapture::State::Cancelled) {
                UI::keyBindingCapture.Reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            if (state == UI::KeyBindingCapture::State::Complete) {
                RequestToggleChange(device, newKey, mode);
            }
        }
        ImGui::PopID();
    }

    void RenderToggleSettings(const char* id, RE::INPUT_DEVICE device) {
        const bool keyboard = device == RE::INPUT_DEVICE::kKeyboard;
        const auto binding = keyboard ? Config::ToggleKey : Config::ToggleKeyGamePad;
        const char* toggleModeNames[] = {"SINGLEPRESS", "HOLD", "DOUBLEPRESS", "OFF"};
        int toggleMode = keyboard ? Config::ToggleMode : Config::ToggleModeGamePad;

        ImGui::PushID(id);
        ImGui::TextUnformatted(Translations::Get(
            keyboard ? "Settings.Toggle.Keyboard" : "Settings.Toggle.Gamepad"));
        ImGui::Separator();

        ImGui::TextUnformatted(Translations::Get("Settings.ToggleMode"));
        if (ImGui::Combo("##ToggleModeCombo", &toggleMode, toggleModeNames, IM_ARRAYSIZE(toggleModeNames))) {
            RequestToggleChange(device, binding, static_cast<std::uint8_t>(toggleMode));
        }

        ImGui::TextUnformatted(Translations::Get("Settings.ToggleKey"));
        RenderKeyBinding("ToggleKey", binding, device);
        ImGui::PopID();
    }

    void ApplyWindowSizeAndPosition(WindowSizeAndPosition& sizeAndPosition, const ImVec2& defaultPosition, const ImVec2& defaultSize,
                             const ImVec2& defaultPivot = ImVec2{0.0f, 0.0f}) {
        const auto viewport = ImGui::GetMainViewport();
        if (!sizeAndPosition.HasSavedState && viewport->Size.x > 0.0f && viewport->Size.y > 0.0f) {
            Ini ini("SKSEMenuFramework.ini");
            ini.SetSection(sizeAndPosition.Section);
            constexpr float missing = std::numeric_limits<float>::quiet_NaN();
            const ImVec2 position{ini.GetFloat("X", missing), ini.GetFloat("Y", missing)};
            const ImVec2 size{ini.GetFloat("Width", missing), ini.GetFloat("Height", missing)};
            if (std::isfinite(position.x) && std::isfinite(position.y) &&
                std::isfinite(size.x) && std::isfinite(size.y) && size.x > 0.0f && size.y > 0.0f) {
                // Fractions survive resolution changes; keep restored windows inside the display.
                sizeAndPosition.Size = ImClamp(ImMin(size, ImVec2{1.0f, 1.0f}) * viewport->Size,
                    ImMin(ImGui::GetStyle().WindowMinSize, viewport->Size), viewport->Size);
                // Round INI fractions back to pixels before ImGui truncates them on Begin.
                sizeAndPosition.Size = {std::round(sizeAndPosition.Size.x), std::round(sizeAndPosition.Size.y)};
                sizeAndPosition.Position = viewport->Pos + ImClamp(position, ImVec2{},
                    ImVec2{1.0f, 1.0f} - sizeAndPosition.Size / viewport->Size) * viewport->Size;
                sizeAndPosition.Position = {std::round(sizeAndPosition.Position.x), std::round(sizeAndPosition.Position.y)};
                sizeAndPosition.HasSavedState = true;
            }
        }
        if (sizeAndPosition.HasSavedState) {
            ImGui::SetNextWindowPos(sizeAndPosition.Position, ImGuiCond_Appearing);
            ImGui::SetNextWindowSize(sizeAndPosition.Size, ImGuiCond_Appearing);
        } else {
            ImGui::SetNextWindowPos(defaultPosition, ImGuiCond_Appearing, defaultPivot);
            ImGui::SetNextWindowSize(defaultSize, ImGuiCond_Appearing);
        }
    }

    void SaveWindowSizeAndPosition(WindowSizeAndPosition& sizeAndPosition) {
        sizeAndPosition.PendingSave |= sizeAndPosition.HasSavedState &&
            (sizeAndPosition.Position != ImGui::GetWindowPos() || sizeAndPosition.Size != ImGui::GetWindowSize());
        sizeAndPosition.Position = ImGui::GetWindowPos();
        sizeAndPosition.Size = ImGui::GetWindowSize();
        sizeAndPosition.HasSavedState = true;
    }

    ImVec2 GetCenteredWindowPosition(const ImGuiViewport* viewport, const ImVec2& size) {
        const auto center = viewport->GetCenter();
        return ImVec2{center.x - size.x * 0.5f, center.y - size.y * 0.5f};
    }

    void SetWindowSizeAndPosition(WindowSizeAndPosition& sizeAndPosition, const ImVec2& position, const ImVec2& size) {
        sizeAndPosition.PendingSave = true;
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

    MenuPath::Segments GetRenamedPath(const MenuPath::Segments& path, const std::string& newName) {
        auto renamedPath = path;
        renamedPath.back() = newName;
        return renamedPath;
    }

    bool IsPathInSubtree(const MenuPath::Segments& path, const MenuPath::Segments& subtreePath) {
        return path.size() >= subtreePath.size() &&
               std::equal(subtreePath.begin(), subtreePath.end(), path.begin());
    }

    void CollectMenuPaths(const UI::MenuTree* node, MenuPath::Segments& parentPath,
                          std::set<MenuPath::Segments>& paths) {
        for (const auto& [name, child] : node->Children) {
            parentPath.push_back(name);
            paths.insert(parentPath);
            CollectMenuPaths(child, parentPath, paths);
            parentPath.pop_back();
        }
    }

    void RebuildProjectedMenuPaths() {
        projectedMenuPaths.clear();
        MenuPath::Segments rootPath;
        CollectMenuPaths(UI::RootMenu, rootPath, projectedMenuPaths);
    }

    void ProjectMenuMutation(const UI::MenuMutation& mutation) {
        if (mutation.Type == UI::MenuMutationType::Delete) {
            std::erase_if(projectedMenuPaths, [&mutation](const MenuPath::Segments& path) {
                return IsPathInSubtree(path, mutation.Path);
            });
            return;
        }

        const auto renamedPath = GetRenamedPath(mutation.Path, mutation.NewName);
        std::set<MenuPath::Segments> renamedPaths;
        for (const auto& path : projectedMenuPaths) {
            if (IsPathInSubtree(path, mutation.Path)) {
                auto renamedDescendantPath = renamedPath;
                renamedDescendantPath.insert(renamedDescendantPath.end(), path.begin() + mutation.Path.size(),
                                             path.end());
                renamedPaths.insert(std::move(renamedDescendantPath));
            } else {
                renamedPaths.insert(path);
            }
        }
        projectedMenuPaths.swap(renamedPaths);
    }

    bool FindMenuNode(const MenuPath::Segments& path, MenuNodeLocation& location) {
        UI::MenuTree* parent = UI::RootMenu;

        for (const auto& name : path) {
            const auto child = parent->Children.find(name);
            if (child == parent->Children.end()) {
                return false;
            }

            location = {parent, child->second, name};
            parent = child->second;
        }
        return location.Node != nullptr;
    }

    void ReplaceSortedChildName(UI::MenuTree* parent, UI::MenuTree* node, const std::string& newName) {
        std::vector<std::pair<const std::string, UI::MenuTree*>> renamedChildren;
        renamedChildren.reserve(parent->SortedChildren.size());
        for (const auto& child : parent->SortedChildren) {
            renamedChildren.emplace_back(child.second == node ? newName : child.first, child.second);
        }
        parent->SortedChildren.swap(renamedChildren);
    }

    void RemoveSortedChild(UI::MenuTree* parent, const UI::MenuTree* node) {
        std::vector<std::pair<const std::string, UI::MenuTree*>> remainingChildren;
        remainingChildren.reserve(parent->SortedChildren.size());
        for (const auto& child : parent->SortedChildren) {
            if (child.second != node) {
                remainingChildren.emplace_back(child.first, child.second);
            }
        }
        parent->SortedChildren.swap(remainingChildren);
    }

    void DestroyMenuTree(UI::MenuTree* node) {
        for (const auto& child : node->Children) {
            DestroyMenuTree(child.second);
        }
        delete node;
    }

    bool RenameMenuNode(const UI::MenuMutation& mutation) {
        MenuNodeLocation location;
        if (!FindMenuNode(mutation.Path, location)) {
            return false;
        }

        if (location.Name == mutation.NewName) {
            if (location.Node->Render) {
                location.Node->Title = mutation.NewName;
            }
            return true;
        }
        if (location.Parent->Children.contains(mutation.NewName)) {
            return false;
        }

        location.Parent->Children.erase(location.Name);
        location.Parent->Children.emplace(mutation.NewName, location.Node);
        ReplaceSortedChildName(location.Parent, location.Node, mutation.NewName);

        if (location.Node->Render) {
            location.Node->Title = mutation.NewName;
        }
        if (location.Parent == UI::RootMenu) {
            RootMenuConfig::RenameMenu(location.Name, mutation.NewName);
            if (pendingArchiveMenu == location.Node) {
                pendingArchiveMenuName = mutation.NewName;
            }
            if (categoryEditor.Menu == location.Node) {
                categoryEditor.MenuName = mutation.NewName;
            }
        }
        return true;
    }

    bool DeleteMenuNode(const UI::MenuMutation& mutation) {
        MenuNodeLocation location;
        if (!FindMenuNode(mutation.Path, location)) {
            return false;
        }

        if (ContainsNode(location.Node, display_node)) {
            display_node = nullptr;
            UI::GamepadNavigation::NotifyPageClosed();
        }
        if (ContainsNode(location.Node, pendingArchiveMenu)) {
            pendingArchiveMenuName.clear();
            pendingArchiveMenu = nullptr;
            archiveConfirmationRequested = false;
        }
        if (ContainsNode(location.Node, categoryEditor.Menu)) {
            categoryEditor.Menu = nullptr;
            categoryEditor.MenuName.clear();
        }

        location.Parent->Children.erase(location.Name);
        RemoveSortedChild(location.Parent, location.Node);
        if (location.Parent == UI::RootMenu) {
            RootMenuConfig::RemoveMenu(location.Name);
        }
        DestroyMenuTree(location.Node);
        return true;
    }

    void ApplyPendingMenuMutationsLocked() {
        while (!pendingMenuMutations.empty()) {
            const auto mutation = std::move(pendingMenuMutations.front());
            pendingMenuMutations.pop_front();

            const bool applied = mutation.Type == UI::MenuMutationType::Rename
                                     ? RenameMenuNode(mutation)
                                     : DeleteMenuNode(mutation);
            if (!applied) {
                logger::warn("Could not apply queued Mod Control Panel mutation for '{}'.", mutation.RequestedPath);
            }
        }
        projectedMenuPaths.clear();
    }

    void AddToTreeInternal(UI::MenuTree* node, std::vector<std::string>& path, RenderFunction render,
                           const std::string& title) {
        if (!path.empty()) {
            const auto currentName = path.front();
            path.erase(path.begin());

            const auto foundItem = node->Children.find(currentName);
            if (foundItem != node->Children.end()) {
                AddToTreeInternal(foundItem->second, path, render, title);
            } else {
                auto newItem = new UI::MenuTree();
                node->Children[currentName] = newItem;
                node->SortedChildren.emplace_back(currentName, newItem);
                AddToTreeInternal(newItem, path, render, title);
            }
            return;
        }

        node->Render = render;
        node->Title = title;
    }

    void SetRootMenuArchived(const std::string& menuName, UI::MenuTree* menu, bool archived) {
        RootMenuConfig::SetArchived(menuName, archived);
        if (archived && ContainsNode(menu, display_node)) {
            display_node = nullptr;
            UI::GamepadNavigation::NotifyPageClosed();
        }
    }

    void RenderTooltip(const char* translationKey) {
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(Translations::Get(translationKey));
            ImGui::EndTooltip();
        }
    }

    void RequestCategoryEditor(CategoryEditorMode mode, const std::string& name, bool fromManager,
                               UI::MenuTree* menu = nullptr, const std::string& menuName = {}) {
        categoryEditor = {};
        categoryEditor.Mode = mode;
        categoryEditor.OriginalName = name;
        const auto length = std::min(name.size(), categoryEditor.Name.size() - 1);
        std::copy_n(name.begin(), length, categoryEditor.Name.begin());
        categoryEditor.Menu = menu;
        categoryEditor.MenuName = menuName;
        categoryEditor.AssignToMenu = menu != nullptr;
        categoryEditor.FromManager = fromManager;
        categoryEditor.OpenRequested = true;
    }

    bool CategoryCancelPressed() {
        return ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
    }

    std::vector<const RootMenuEntry*> GetOrderedRootMenus() {
        const auto savedOrder = RootMenuConfig::GetMenuOrder();
        std::map<std::string, std::size_t> positions;
        for (std::size_t index = 0; index < savedOrder.size(); ++index) {
            positions.emplace(savedOrder[index], index);
        }

        std::vector<const RootMenuEntry*> menus;
        menus.reserve(UI::RootMenu->Children.size());
        for (const auto& entry : UI::RootMenu->Children) {
            menus.push_back(&entry);
        }
        std::sort(menus.begin(), menus.end(), [&](auto left, auto right) {
            const auto leftPosition = positions.find(left->first);
            const auto rightPosition = positions.find(right->first);
            const auto leftIndex = leftPosition == positions.end() ? savedOrder.size() : leftPosition->second;
            const auto rightIndex = rightPosition == positions.end() ? savedOrder.size() : rightPosition->second;
            if (leftIndex != rightIndex) {
                return leftIndex < rightIndex;
            }
            const bool leftFavorite = RootMenuConfig::IsFavorite(left->first);
            const bool rightFavorite = RootMenuConfig::IsFavorite(right->first);
            if (leftFavorite != rightFavorite) {
                return leftFavorite;
            }
            return left->first < right->first;
        });
        return menus;
    }

    std::optional<std::string> GetDraggedRootMenuName() {
        const auto payload = ImGui::GetDragDropPayload();
        if (!payload || !payload->IsDataType(ROOT_MENU_PAYLOAD) || !payload->Data || payload->DataSize <= 1) {
            return std::nullopt;
        }
        const auto nameData = static_cast<const char*>(payload->Data);
        if (nameData[payload->DataSize - 1] != '\0') {
            return std::nullopt;
        }
        const std::string name(nameData, static_cast<std::size_t>(payload->DataSize - 1));
        if (!UI::RootMenu->Children.contains(name) || RootMenuConfig::IsArchived(name)) {
            return std::nullopt;
        }
        return name;
    }

    void RenderRootMenuDragSource(const std::string& menuName) {
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAutoExpirePayload |
            ImGuiDragDropFlags_SourceNoHoldToOpenOthers)) {
            ImGui::SetDragDropPayload(ROOT_MENU_PAYLOAD, menuName.c_str(), menuName.size() + 1);
            ImGui::TextUnformatted(menuName.c_str());
            ImGui::TextUnformatted(Translations::Get("Menu.Reorder"));
            ImGui::EndDragDropSource();
        }
    }

    std::optional<RootMenuDropPreview> RenderRootMenuDropTarget(const std::string& categoryName,
        const std::string& targetMenuName, std::optional<RootMenuDrop>& pendingDrop) {
        const auto draggedMenuName = GetDraggedRootMenuName();
        if (!draggedMenuName || *draggedMenuName == targetMenuName) {
            return std::nullopt;
        }

        const auto headerMin = ImGui::GetItemRectMin();
        const auto headerMax = ImGui::GetItemRectMax();
        ImRect targetRect(headerMin, headerMax);
        const bool categoryHeading = targetMenuName.empty();
        const float midpoint = (headerMin.y + headerMax.y) * 0.5f;
        const bool placeAfter = !categoryHeading && ImGui::GetIO().MousePos.y >= midpoint;
        if (!categoryHeading) {
            if (placeAfter) {
                targetRect.Min.y = midpoint;
            } else {
                targetRect.Max.y = midpoint;
            }
        }
        const auto targetID = ImHashStr(categoryHeading ? "DropAppend" : (placeAfter ? "DropAfter" : "DropBefore"),
            0, ImGui::GetItemID());
        if (!ImGui::BeginDragDropTargetCustom(targetRect, targetID)) {
            return std::nullopt;
        }

        std::optional<RootMenuDropPreview> preview;
        const auto payload = ImGui::AcceptDragDropPayload(ROOT_MENU_PAYLOAD,
            ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
        if (payload) {
            const float halfSpacing = ImGui::GetStyle().ItemSpacing.y * 0.5f;
            const float insertionY = categoryHeading || placeAfter ? headerMax.y + halfSpacing :
                headerMin.y - halfSpacing;
            preview = RootMenuDropPreview{
                RootMenuDrop{*draggedMenuName, categoryName, targetMenuName, placeAfter},
                ImRect(headerMin, headerMax), insertionY};
            if (payload->IsDelivery()) {
                pendingDrop = preview->Placement;
            }
        }
        ImGui::EndDragDropTarget();
        return preview;
    }

    void RenderRootMenuDropIndicator(const RootMenuDropPreview& preview) {
        const auto drawList = ImGui::GetWindowDrawList();
        auto accent = ImGui::GetStyleColorVec4(ImGuiCol_DragDropTarget);
        accent.w = 1.0f;
        const auto color = ImGui::GetColorU32(accent);
        const auto backing = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.85f));
        const float thickness = ImMax(3.0f, ImGui::GetFontSize() * 0.15f);
        const float endcapRadius = thickness;
        const bool categoryHeading = preview.Placement.TargetMenuName.empty();
        if (categoryHeading) {
            drawList->AddRect(preview.HeaderBounds.Min, preview.HeaderBounds.Max,
                backing, 0.0f, 0, thickness + 2.0f);
            drawList->AddRect(preview.HeaderBounds.Min, preview.HeaderBounds.Max, color, 0.0f, 0, thickness);
        }
        const float indent = categoryHeading ? ImGui::GetStyle().IndentSpacing : 0.0f;
        const ImVec2 lineMin(preview.HeaderBounds.Min.x + indent + endcapRadius, preview.InsertionY);
        const ImVec2 lineMax(preview.HeaderBounds.Max.x - endcapRadius, preview.InsertionY);
        if (lineMax.x > lineMin.x) {
            drawList->AddLine(lineMin, lineMax, backing, thickness + 2.0f);
            drawList->AddLine(lineMin, lineMax, color, thickness);
            for (const auto endpoint : {lineMin, lineMax}) {
                drawList->AddCircleFilled(endpoint, endcapRadius + 1.0f, backing);
                drawList->AddCircleFilled(endpoint, endcapRadius, color);
            }
        }

        const auto destination = categoryHeading ?
            (preview.Placement.CategoryName.empty() ? Translations::Get("Categories.Uncategorized") :
                preview.Placement.CategoryName.c_str()) : preview.Placement.TargetMenuName.c_str();
        const auto labelKey = categoryHeading ? "Menu.Reorder.End" :
            (preview.Placement.PlaceAfter ? "Menu.Reorder.After" : "Menu.Reorder.Before");
        ImGui::SetTooltip("%s\n%s %s", preview.Placement.MenuName.c_str(), Translations::Get(labelKey), destination);
    }

    bool ApplyRootMenuDrop(const RootMenuDrop& drop, const std::vector<const RootMenuEntry*>& rootMenus) {
        if (!UI::RootMenu->Children.contains(drop.MenuName) || RootMenuConfig::IsArchived(drop.MenuName)) {
            logger::warn("Could not move unavailable root menu '{}'.", drop.MenuName);
            return false;
        }
        if (!drop.TargetMenuName.empty() &&
            (!UI::RootMenu->Children.contains(drop.TargetMenuName) ||
                RootMenuConfig::IsArchived(drop.TargetMenuName) ||
                RootMenuConfig::GetCategory(drop.TargetMenuName) != drop.CategoryName)) {
            logger::warn("Could not move root menu '{}': the target menu is no longer available.", drop.MenuName);
            return false;
        }

        // Include hidden and unregistered menus so a filtered drag preserves their saved positions.
        auto orderedMenus = RootMenuConfig::GetMenuOrder();
        std::set<std::string> savedMenus(orderedMenus.begin(), orderedMenus.end());
        for (const auto entry : rootMenus) {
            if (savedMenus.insert(entry->first).second) {
                orderedMenus.push_back(entry->first);
            }
        }
        std::erase(orderedMenus, drop.MenuName);
        auto insertion = orderedMenus.end();
        if (!drop.TargetMenuName.empty()) {
            insertion = std::find(orderedMenus.begin(), orderedMenus.end(), drop.TargetMenuName);
            if (drop.PlaceAfter && insertion != orderedMenus.end()) {
                ++insertion;
            }
        } else {
            for (auto menu = orderedMenus.begin(); menu != orderedMenus.end(); ++menu) {
                if (RootMenuConfig::GetCategory(*menu) == drop.CategoryName) {
                    insertion = std::next(menu);
                }
            }
        }
        orderedMenus.insert(insertion, drop.MenuName);
        return RootMenuConfig::SetMenuPlacement(drop.MenuName, drop.CategoryName, orderedMenus);
    }

    void ScrollRootMenusWhileDragging() {
        if (!GetDraggedRootMenuName() ||
            !ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
            return;
        }
        const auto window = ImGui::GetCurrentWindow();
        const float edgeSize = ImGui::GetFrameHeight() * 1.5f;
        const float mouseY = ImGui::GetIO().MousePos.y;
        float direction = 0.0f;
        if (mouseY < window->InnerRect.Min.y + edgeSize) {
            direction = -1.0f;
        } else if (mouseY > window->InnerRect.Max.y - edgeSize) {
            direction = 1.0f;
        }
        if (direction != 0.0f) {
            const float distance = direction * ImGui::GetFontSize() * 20.0f * ImGui::GetIO().DeltaTime;
            ImGui::SetScrollY(ImClamp(ImGui::GetScrollY() + distance, 0.0f, ImGui::GetScrollMaxY()));
        }
    }

    void RenderRootMenuMoveError() {
        const auto title = std::format("{}###RootMenuMoveError", Translations::Get("Menu.Reorder.Error.Title"));
        if (rootMenuMoveErrorRequested) {
            ImGui::OpenPopup(title.c_str());
            rootMenuMoveErrorRequested = false;
        }
        const auto viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSizeConstraints(ImVec2{}, viewport->WorkSize * 0.85f);
        if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", Translations::Get("Menu.Reorder.Error.Save"));
            if (ImGui::Button(Translations::Get("Categories.Close")) || CategoryCancelPressed()) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // Hash category names as data so "##" and "###" remain ordinary name characters.
    void PushCategoryID(const std::string& name) {
        ImGui::PushID(static_cast<int>(ImHashData(name.data(), name.size())));
    }

    bool RenderCategoryHeading(const char* label) {
        if (ImGui::GetCurrentWindow()->SkipItems) {
            return false;
        }
        const auto headingPosition = ImGui::GetCursorScreenPos();
        for (const auto color : {ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive, ImGuiCol_Text}) {
            ImGui::PushStyleColor(color, ImVec4{});
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        const bool open = ImGui::TreeNodeEx("Group",
            ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen, "%s", label);
        const auto headingItem = GImGui->LastItemData;
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
        const auto nextPosition = ImGui::GetCursorScreenPos();

        // Keep the tree's interaction and state, and reserve space beside the separator for its caret.
        const float caretWidth = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x;
        ImGui::SetCursorScreenPos(ImVec2(headingPosition.x + caretWidth, headingPosition.y));
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, ImVec2(0.0f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding,
            ImVec2(ImGui::GetStyle().ItemSpacing.x, ImGui::GetStyle().FramePadding.y));
        ImGui::SeparatorTextEx(0, label, label + std::strlen(label), 0.0f);
        ImGui::PopStyleVar(3);
        const ImVec2 caretPosition(headingPosition.x + ImGui::GetStyle().FramePadding.x,
            (headingItem.Rect.Min.y + headingItem.Rect.Max.y - ImGui::GetFontSize()) * 0.5f);
        ImGui::RenderArrow(ImGui::GetWindowDrawList(), caretPosition, ImGui::GetColorU32(ImGuiCol_Text),
            open ? ImGuiDir_Down : ImGuiDir_Right);
        ImGui::SetCursorScreenPos(nextPosition);
        GImGui->LastItemData = headingItem;
        return open;
    }

    bool RenderCategoryOption(const std::string& name, const char* label, bool selected) {
        PushCategoryID(name);
        const auto textPosition = ImGui::GetCursorScreenPos();
        const auto labelEnd = label + std::strlen(label);
        const float textWidth = ImGui::CalcTextSize(label, labelEnd).x;
        const float width = ImMax(textWidth, ImGui::GetContentRegionAvail().x);
        const bool chosen = ImGui::Selectable("##CategoryOption", selected,
            ImGuiSelectableFlags_DontClosePopups, ImVec2(width, 0.0f));
        ImGui::RenderTextClipped(textPosition, ImGui::GetItemRectMax(), label, labelEnd, nullptr);
        if (selected) {
            ImGui::SetItemDefaultFocus();
        }
        ImGui::PopID();
        return chosen;
    }

    void RenderCategoryChooser(const std::string& menuName, UI::MenuTree* menu) {
        ImGui::SetNextWindowSizeConstraints(ImVec2{}, ImGui::GetMainViewport()->WorkSize * 0.85f);
        if (!ImGui::BeginPopup("CategoryChooser")) {
            return;
        }
        const auto currentCategory = RootMenuConfig::GetCategory(menuName);
        auto chooseCategory = [&](const std::string& name) {
            if (RootMenuConfig::SetCategory(menuName, name)) {
                categoryToOpen = name;
                ImGui::CloseCurrentPopup();
            } else {
                categoryAssignmentError = "Categories.Error.Save";
            }
        };
        if (RenderCategoryOption({}, Translations::Get("Categories.Uncategorized"), currentCategory.empty())) {
            chooseCategory({});
        }
        for (const auto& name : RootMenuConfig::GetCategories()) {
            if (RenderCategoryOption(name, name.c_str(), name == currentCategory)) {
                chooseCategory(name);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem(Translations::Get("Categories.Create"))) {
            RequestCategoryEditor(CategoryEditorMode::Create, {}, false, menu, menuName);
        }
        if (categoryAssignmentError) {
            ImGui::TextWrapped("%s", Translations::Get(categoryAssignmentError));
        }
        ImGui::EndPopup();
    }

    void RenderCategoryEditor() {
        const bool deleting = categoryEditor.Mode == CategoryEditorMode::Delete;
        const auto titleKey = deleting ? "Categories.Delete.Title" :
            (categoryEditor.Mode == CategoryEditorMode::Create ? "Categories.Create" : "Categories.Rename");
        const auto title = std::format("{}###MenuCategoryEditor", Translations::Get(titleKey));
        if (categoryEditor.OpenRequested) {
            ImGui::OpenPopup(title.c_str());
            categoryEditor.OpenRequested = false;
        }
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 20.0f, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2{}, ImGui::GetMainViewport()->WorkSize * 0.85f);
        if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            return;
        }
        if (categoryEditor.AssignToMenu && !categoryEditor.Menu) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        bool submitted = false;
        if (deleting) {
            ImGui::TextWrapped("%s", Translations::Get("Categories.Delete.Confirm"));
            ImGui::TextUnformatted(categoryEditor.OriginalName.c_str());
        } else {
            ImGui::TextUnformatted(Translations::Get("Categories.Name"));
            if (ImGui::IsWindowAppearing() && !UI::GamepadNavigation::IsActive()) {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            submitted = ImGui::InputText("##CategoryName", categoryEditor.Name.data(), categoryEditor.Name.size(),
                ImGuiInputTextFlags_EnterReturnsTrue);
            if (ImGui::IsItemEdited()) {
                categoryEditor.Error = nullptr;
            }
        }
        if (categoryEditor.Error) {
            ImGui::TextWrapped("%s", Translations::Get(categoryEditor.Error));
        }
        ImGui::Separator();
        const bool save = ImGui::Button(Translations::Get(deleting ? "Categories.Delete" : "Categories.Save"));
        ImGui::SameLine();
        const bool cancel = ImGui::Button(Translations::Get("Categories.Cancel")) || CategoryCancelPressed();
        if (deleting) {
            ImGui::SetItemDefaultFocus();
        }
        if (cancel) {
            ImGui::CloseCurrentPopup();
        } else if (save || submitted) {
            const auto name = RootMenuConfig::NormalizeCategoryName(categoryEditor.Name.data());
            const auto categories = RootMenuConfig::GetCategories();
            const bool validName = RootMenuConfig::IsValidCategoryName(name) &&
                (!categories.contains(name) || (categoryEditor.Mode == CategoryEditorMode::Rename &&
                                               name == categoryEditor.OriginalName));
            if (!deleting && !validName) {
                categoryEditor.Error = "Categories.Error.Name";
            } else {
                bool saved = false;
                switch (categoryEditor.Mode) {
                    case CategoryEditorMode::Create:
                        saved = RootMenuConfig::CreateCategory(name, categoryEditor.MenuName);
                        break;
                    case CategoryEditorMode::Rename:
                        saved = RootMenuConfig::RenameCategory(categoryEditor.OriginalName, name);
                        break;
                    case CategoryEditorMode::Delete:
                        saved = RootMenuConfig::RemoveCategory(categoryEditor.OriginalName);
                        break;
                }
                if (saved) {
                    categoryToOpen = deleting ? std::string{} : name;
                    ImGui::CloseCurrentPopup();
                } else {
                    categoryEditor.Error = "Categories.Error.Save";
                }
            }
        }
        ImGui::EndPopup();
    }

    void RenderCategoryDialogs() {
        const auto title = std::format("{}###MenuCategoryManager", Translations::Get("Categories.Manage.Title"));
        if (categoryManagerRequested) {
            ImGui::OpenPopup(title.c_str());
            categoryManagerRequested = false;
        }
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 24.0f, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2{}, ImGui::GetMainViewport()->WorkSize * 0.85f);
        if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::Button(Translations::Get("Categories.Create"))) {
                RequestCategoryEditor(CategoryEditorMode::Create, {}, true);
            }
            ImGui::BeginChild("CategoryList", ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() * 6.0f), ImGuiChildFlags_Border);
            const auto categories = RootMenuConfig::GetCategories();
            if (categories.empty()) {
                ImGui::TextUnformatted(Translations::Get("Categories.Empty"));
            } else if (ImGui::BeginTable("Categories", 3)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                for (const auto key : {"Categories.Rename", "Categories.Delete"}) {
                    const auto width = ImGui::CalcTextSize(Translations::Get(key)).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                    ImGui::TableSetupColumn(key, ImGuiTableColumnFlags_WidthFixed, width);
                }
                for (const auto& name : categories) {
                    PushCategoryID(name);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", name.c_str());
                    ImGui::TableNextColumn();
                    if (ImGui::Button(Translations::Get("Categories.Rename"))) {
                        RequestCategoryEditor(CategoryEditorMode::Rename, name, true);
                    }
                    ImGui::TableNextColumn();
                    if (ImGui::Button(Translations::Get("Categories.Delete"))) {
                        RequestCategoryEditor(CategoryEditorMode::Delete, name, true);
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
            const bool childOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
            const bool close = ImGui::Button(Translations::Get("Categories.Close")) || CategoryCancelPressed();
            if (close && !childOpen && !categoryEditor.OpenRequested) {
                ImGui::CloseCurrentPopup();
            }
            if (categoryEditor.FromManager) {
                RenderCategoryEditor();
            }
            ImGui::EndPopup();
        }
        if (!categoryEditor.FromManager) {
            RenderCategoryEditor();
        }
    }

    void RenderRootMenuButtons(const std::string& menuName, UI::MenuTree* menu, bool favorite) {
        const auto headerMin = ImGui::GetItemRectMin();
        const auto headerMax = ImGui::GetItemRectMax();
        const auto restoreCursor = ImGui::GetCursorScreenPos();
        const float buttonSize = headerMax.y - headerMin.y;

        ImGui::PushID(menu);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        ImGui::SetCursorScreenPos(ImVec2(headerMax.x - buttonSize * 3.0f, headerMin.y));
        ImGui::PushStyleColor(
            ImGuiCol_Text, favorite ? FAVORITE_STAR_COLOR : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (ImGui::Button(FAVORITE_STAR, ImVec2(buttonSize, buttonSize))) {
            RootMenuConfig::SetFavorite(menuName, !favorite);
        }
        ImGui::PopStyleColor();
        RenderTooltip(favorite ? "Menu.RemoveFavorite" : "Menu.AddFavorite");

        ImGui::SetCursorScreenPos(ImVec2(headerMax.x - buttonSize * 2.0f, headerMin.y));
        if (ImGui::Button(CATEGORY_ICON, ImVec2(buttonSize, buttonSize))) {
            categoryAssignmentError = nullptr;
            ImGui::OpenPopup("CategoryChooser");
        }
        RenderTooltip("Menu.Category");

        ImGui::SetCursorScreenPos(ImVec2(headerMax.x - buttonSize, headerMin.y));
        if (ImGui::Button(ARCHIVE_ICON, ImVec2(buttonSize, buttonSize))) {
            pendingArchiveMenuName = menuName;
            pendingArchiveMenu = menu;
            archiveConfirmationRequested = true;
        }
        RenderTooltip("Menu.Archive");

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        RenderCategoryChooser(menuName, menu);
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
            if (!pendingArchiveMenu) {
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }

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


    const bool itemClicked = ImGui::IsItemClicked();
    const bool itemToggledOpen = ImGui::IsItemToggledOpen();
    const bool gamepadButtonPressed = ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown);
    const bool itemIsFocused = ImGui::IsItemFocused();
    const bool activatedByGamepad = gamepadButtonPressed && itemIsFocused;

    if ((itemClicked || activatedByGamepad) && !itemToggledOpen) {
        if (node.second->Render) {
            display_node = node.second;
            if (activatedByGamepad) {
                UI::GamepadNavigation::RequestFocus(UI::GamepadNavigation::Area::PageContent);
            }
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

bool UI::QueueMenuMutation(MenuMutationType type, std::string_view path, std::string_view newName) {
    auto parsedPath = MenuPath::Parse(path);
    if (!parsedPath) {
        return false;
    }

    std::string parsedNewName;
    if (type == MenuMutationType::Rename) {
        auto parsedName = MenuPath::ParseSegment(newName);
        if (!parsedName) {
            return false;
        }
        parsedNewName = std::move(*parsedName);
    }

    MenuMutation mutation{type, std::move(*parsedPath), std::move(parsedNewName), std::string(path)};
    std::lock_guard<std::recursive_mutex> lock(menuTreeMutex);

    if (pendingMenuMutations.empty()) {
        RebuildProjectedMenuPaths();
    }
    if (!projectedMenuPaths.contains(mutation.Path)) {
        return false;
    }

    if (mutation.Type == MenuMutationType::Rename) {
        const auto renamedPath = GetRenamedPath(mutation.Path, mutation.NewName);
        if (renamedPath != mutation.Path && projectedMenuPaths.contains(renamedPath)) {
            return false;
        }
    }

    pendingMenuMutations.push_back(std::move(mutation));
    ProjectMenuMutation(pendingMenuMutations.back());
    return true;
}

void UI::ApplyPendingMenuMutations() {
    std::lock_guard<std::recursive_mutex> lock(menuTreeMutex);
    ApplyPendingMenuMutationsLocked();
}

void UI::SaveWindowSettings() {
    if ((!mainWindowSizeAndPosition.PendingSave && !configWindowSizeAndPosition.PendingSave) ||
        ((WindowManager::MainInterface->IsOpen || WindowManager::ConfigInterface->IsOpen) &&
         (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::GetCurrentContext()->NavWindowingTarget))) {
        return;
    }
    const auto viewport = ImGui::GetMainViewport();
    if (viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f) {
        return;
    }
    Ini ini("SKSEMenuFramework.ini");
    bool saved = true;
    for (const auto state : {&mainWindowSizeAndPosition, &configWindowSizeAndPosition}) {
        if (!state->PendingSave) {
            continue;
        }
        // Retry on the next layout change if writing fails, not on every render frame.
        state->PendingSave = false;
        ini.SetSection(state->Section);
        const ImVec2 position = (state->Position - viewport->Pos) / viewport->Size;
        const ImVec2 size = state->Size / viewport->Size;
        saved &= ini.SetFloat("X", position.x);
        saved &= ini.SetFloat("Y", position.y);
        saved &= ini.SetFloat("Width", size.x);
        saved &= ini.SetFloat("Height", size.y);
    }
    if (!saved || !ini.Save()) {
        logger::warn("Failed to save window layouts to SKSEMenuFramework.ini");
    }
}

void __stdcall UI::RenderMenuWindow() {
    ApplyPendingMenuMutations();
    std::lock_guard<std::recursive_mutex> lock(menuTreeMutex);

    auto viewport = ImGui::GetMainViewport();
    ApplyWindowSizeAndPosition(mainWindowSizeAndPosition, viewport->GetCenter(), ImVec2{viewport->Size.x * 0.8f, viewport->Size.y * 0.8f},
                        ImVec2{0.5f, 0.5f});
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_MenuBar;
    window_flags |= ImGuiWindowFlags_NoTitleBar;

    ImGui::Begin("#MCPMainWindow", nullptr, window_flags);
    SaveWindowSizeAndPosition(mainWindowSizeAndPosition);
    GamepadNavigation::BeginFrame(display_node != nullptr, WindowManager::ConfigInterface->IsOpen.load());
    GamepadNavigation::PushFocusStyle();

    if (ImGui::BeginMenuBar()) {
        PushSolid();
        const char* optionsLabel = Translations::Get("Options");
        const bool optionsWasOpen = ImGui::IsPopupOpen(optionsLabel);
        const bool toggleOptions = GamepadNavigation::IsOptionsToggleRequested();
        if (toggleOptions && !optionsWasOpen) {
            ImGui::OpenPopup(optionsLabel);
        }

        if (ImGui::BeginMenu(optionsLabel)) {
            GamepadNavigation::BeginOptionsMenu();
            const bool closeOptions = toggleOptions && optionsWasOpen;
            if (closeOptions) {
                ImGui::CloseCurrentPopup();
                GamepadNavigation::NotifyOptionsMenuClosed();
            }

            if (!closeOptions) {
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
                if (ImGui::MenuItem(Translations::Get("Options.ManageCategories"))) {
                    categoryManagerRequested = true;
                }
                RenderArchivedMenuRecovery();
            }
            GamepadNavigation::EndArea();
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
    const float hintBarHeight = GamepadNavigation::GetHintBarHeight();
    const float contentHeight =
        hintBarHeight > 0.0f ? -(hintBarHeight + ImGui::GetStyle().ItemSpacing.y) : -FLT_MIN;
    ImGui::BeginChild("SKSEModControlPanelTreeView",
                      ImVec2(ImGui::GetContentRegionAvail().x * 0.3f, contentHeight),
                      ImGuiChildFlags_Border);
    GamepadNavigation::BeginArea(GamepadNavigation::Area::PageTree);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 5.0f));
    const auto dragPayload = ImGui::GetDragDropPayload();
    if (dragPayload && dragPayload->IsDataType(ROOT_MENU_PAYLOAD) &&
        (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || !GetDraggedRootMenuName())) {
        ImGui::ClearDragDrop();
        ImGui::ClearActiveID();
    }
    const auto rootMenus = GetOrderedRootMenus();
    std::optional<RootMenuDrop> pendingDrop;
    std::optional<RootMenuDropPreview> dropPreview;

    const auto categories = RootMenuConfig::GetCategories();
    const bool showCategories = !categories.empty();
    std::map<std::string, std::vector<const RootMenuEntry*>> categoryMenus;
    categoryMenus.try_emplace(std::string{});
    for (const auto& name : categories) {
        categoryMenus.try_emplace(name);
    }
    for (const auto rootMenu : rootMenus) {
        const auto& item = *rootMenu;
        if (RootMenuConfig::IsArchived(item.first)) {
            for (auto node : item.second->Children) {
                DummyRenderer(node);
            }
            continue;
        }
        if (filter.PassFilter(item.first.c_str())) {
            categoryMenus[RootMenuConfig::GetCategory(item.first)].push_back(rootMenu);
        }
    }

    const auto requestedCategory = categoryToOpen;
    categoryToOpen.reset();
    // Uncategorized follows the named groups; map order keeps names alphabetical.
    auto renderCategory = [&](const std::string& name, const std::vector<const RootMenuEntry*>& menus) {
        if (menus.empty() && filter.IsActive() && !GetDraggedRootMenuName()) {
            return;
        }
        std::optional<RootMenuDropPreview> categoryPreview;
        if (showCategories) {
            ImGui::PushID("MenuCategory");
            PushCategoryID(name);
            if (filter.IsActive() || (requestedCategory && *requestedCategory == name)) {
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            }
            const auto label = name.empty() ? Translations::Get("Categories.Uncategorized") : name.c_str();
            const bool categoryOpen = RenderCategoryHeading(label);
            categoryPreview = RenderRootMenuDropTarget(name, {}, pendingDrop);
            ImGui::PopID();
            ImGui::PopID();
            if (!categoryOpen) {
                if (categoryPreview) {
                    dropPreview = categoryPreview;
                }
                return;
            }
            // Indent without pushing a category ID: a moved mod keeps its widget state.
            ImGui::Indent();
        }
        for (const auto rootMenu : menus) {
            const auto& item = *rootMenu;
            const bool favorite = RootMenuConfig::IsFavorite(item.first);
            const bool headerOpen = ImGui::TreeNodeEx(item.second,
                ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_AllowOverlap, "");
            const auto headerMin = ImGui::GetItemRectMin();
            const auto headerMax = ImGui::GetItemRectMax();
            const float buttonSize = headerMax.y - headerMin.y;
            const ImVec2 textMin{headerMin.x + ImGui::GetTreeNodeToLabelSpacing(),
                headerMin.y + ImGui::GetStyle().FramePadding.y};
            const ImVec2 textMax{headerMax.x - buttonSize * 3.0f, headerMax.y};
            if (textMax.x > textMin.x) {
                ImGui::RenderTextClipped(textMin, textMax, item.first.c_str(), item.first.c_str() + item.first.size(), nullptr);
            }
            RenderRootMenuDragSource(item.first);
            auto menuPreview = RenderRootMenuDropTarget(name, item.first, pendingDrop);
            RenderRootMenuButtons(item.first, item.second, favorite);
            if (!RootMenuConfig::IsArchived(item.first) && headerOpen) {
                for (auto node : item.second->SortedChildren) {
                    RenderNode(node);
                }
            } else {
                for (auto node : item.second->Children) {
                    DummyRenderer(node);
                }
            }
            if (menuPreview) {
                if (menuPreview->Placement.PlaceAfter) {
                    const float halfSpacing = ImGui::GetStyle().ItemSpacing.y * 0.5f;
                    menuPreview->InsertionY = ImMax(menuPreview->InsertionY,
                        ImGui::GetCursorScreenPos().y - halfSpacing);
                }
                dropPreview = menuPreview;
            }
        }
        if (showCategories) {
            ImGui::Unindent();
        }
        if (categoryPreview) {
            const float halfSpacing = ImGui::GetStyle().ItemSpacing.y * 0.5f;
            categoryPreview->InsertionY = ImMax(categoryPreview->InsertionY,
                ImGui::GetCursorScreenPos().y - halfSpacing);
            dropPreview = categoryPreview;
        }
    };
    for (const auto& [name, menus] : categoryMenus) {
        if (!name.empty()) {
            renderCategory(name, menus);
        }
    }
    if (const auto uncategorized = categoryMenus.find(std::string{}); uncategorized != categoryMenus.end()) {
        renderCategory(uncategorized->first, uncategorized->second);
    }
    if (dropPreview) {
        RenderRootMenuDropIndicator(*dropPreview);
    }
    ScrollRootMenusWhileDragging();
    if (pendingDrop) {
        if (ApplyRootMenuDrop(*pendingDrop, rootMenus)) {
            categoryToOpen = pendingDrop->CategoryName;
        } else {
            rootMenuMoveErrorRequested = true;
        }
    }
    ImGui::PopStyleVar();
    GamepadNavigation::EndArea();
    ImGui::EndChild();

    ImGui::SameLine();

    // Content section
    ImGui::BeginChild("SKSEModControlPanelMenuNode", ImVec2(0, contentHeight), ImGuiChildFlags_Border);
    GamepadNavigation::BeginArea(GamepadNavigation::Area::PageContent);
    if (display_node) {
        display_node->Render();
    }
    GamepadNavigation::EndArea();
    ImGui::EndChild();

    GamepadNavigation::RenderHintBar(display_node != nullptr);

    RenderArchiveConfirmation();
    RenderCategoryDialogs();
    RenderRootMenuMoveError();

    GamepadNavigation::RenderFocusedItemHighlight();
    GamepadNavigation::PopFocusStyle();
    GamepadNavigation::EndFrame();

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
        GamepadNavigation::NotifyPageClosed();
        return BackAction::PoppedPage;
    }

    return BackAction::CloseMenu;
}

void UI::AddToTree(UI::MenuTree* node, std::vector<std::string>& path, RenderFunction render, std::string title) {
    ApplyPendingMenuMutations();
    std::lock_guard<std::recursive_mutex> lock(menuTreeMutex);
    AddToTreeInternal(node, path, render, title);
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

        ImGui::TextUnformatted(Translations::Get("Settings.Font"));
        const auto& fonts = FontManager::fontSizes["Default"];
        if (ImGui::BeginCombo("##PrimaryFont", fonts.defaultFontName.c_str())) {
            for (const auto& name : fonts.textFontNames) {
                const bool isSelected = ImStricmp(name.c_str(), fonts.defaultFontName.c_str()) == 0;
                if (ImGui::Selectable(name.c_str(), isSelected) && ImStricmp(name.c_str(), Config::PrimaryFont.c_str()) != 0) {
                    Config::PrimaryFont = name;
                    Config::Save();
                    FontManager::RequestAtlasRebuild();
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        constexpr std::array glyphLanguageKeys = {"Settings.GlyphLanguage.Default",  "Settings.GlyphLanguage.Chinese",
                                                  "Settings.GlyphLanguage.Japanese", "Settings.GlyphLanguage.Korean",
                                                  "Settings.GlyphLanguage.Cyrillic", "Settings.GlyphLanguage.Thai",
                                                  "Settings.GlyphLanguage.Turkish",  "Settings.GlyphLanguage.Polish",
                                                  "Settings.GlyphLanguage.Custom"};
        const auto selectedGlyphLanguage = Config::GetGlyphLanguage();
        const auto selectedGlyphLanguageIndex = static_cast<std::size_t>(selectedGlyphLanguage);
        const auto glyphModalTitle = std::format("{}###CustomGlyphRanges",
            Translations::Get("Settings.GlyphLanguage.Custom.Title"));
        static Config::GlyphRangeSelection pendingGlyphRanges;
        const char* configureGlyphsLabel = Translations::Get("Settings.GlyphLanguage.Configure");
        const float configureGlyphsWidth = ImGui::CalcTextSize(configureGlyphsLabel).x +
            ImGui::GetStyle().FramePadding.x * 2.0f;
        const float glyphComboWidth = std::max(ImGui::GetFrameHeight(),
            contentWidth - configureGlyphsWidth - ImGui::GetStyle().ItemSpacing.x);

        ImGui::PushItemWidth(glyphComboWidth);
        if (ImGui::BeginCombo("##GlyphLanguage", Translations::Get(glyphLanguageKeys[selectedGlyphLanguageIndex]))) {
            for (std::size_t index = 0; index < static_cast<std::size_t>(Config::GlyphLanguage::Custom); ++index) {
                const auto language = static_cast<Config::GlyphLanguage>(index);
                const bool isSelected = language == selectedGlyphLanguage;
                if (ImGui::Selectable(Translations::Get(glyphLanguageKeys[index]), isSelected) && !isSelected) {
                    Config::SetGlyphLanguage(language);
                    Config::Save();
                    FontManager::RequestAtlasRebuild();
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(configureGlyphsLabel, ImVec2(configureGlyphsWidth, 0.0f))) {
            pendingGlyphRanges = Config::GetGlyphRangeSelection();
            ImGui::OpenPopup(glyphModalTitle.c_str());
        }
        if (ImGui::BeginPopupModal(glyphModalTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted(Translations::Get("Settings.GlyphLanguage.DefaultIncluded"));
            ImGui::Separator();
            ImGui::Checkbox(Translations::Get("Settings.GlyphLanguage.Chinese"), &pendingGlyphRanges.Chinese);
            ImGui::Checkbox(Translations::Get("Settings.GlyphLanguage.Japanese"), &pendingGlyphRanges.Japanese);
            ImGui::Checkbox(Translations::Get("Settings.GlyphLanguage.Korean"), &pendingGlyphRanges.Korean);
            ImGui::Checkbox(Translations::Get("Settings.GlyphLanguage.Cyrillic"), &pendingGlyphRanges.Cyrillic);
            ImGui::Checkbox(Translations::Get("Settings.GlyphLanguage.Thai"), &pendingGlyphRanges.Thai);
            ImGui::Checkbox(Translations::Get("Settings.GlyphLanguage.Turkish"), &pendingGlyphRanges.Turkish);
            ImGui::Checkbox(Translations::Get("Settings.GlyphLanguage.Polish"), &pendingGlyphRanges.Polish);
            ImGui::Separator();
            const bool cancel = ImGui::Button(Translations::Get("Settings.GlyphLanguage.Cancel")) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
            ImGui::SameLine();
            const bool apply = ImGui::Button(Translations::Get("Settings.GlyphLanguage.Apply"));
            if (cancel || apply) {
                if (apply && !cancel && pendingGlyphRanges != Config::GetGlyphRangeSelection()) {
                    Config::SetGlyphRangeSelection(pendingGlyphRanges);
                    Config::Save();
                    FontManager::RequestAtlasRebuild();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
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

        RenderToggleSettings("KeyboardToggleSettings", RE::INPUT_DEVICE::kKeyboard);
        ImGui::Spacing();
        RenderToggleSettings("GamepadToggleSettings", RE::INPUT_DEVICE::kGamepad);
        RenderBindingConfirmation();

        ImGui::PopItemWidth();  // ADDED: Pop the item width
        ImGui::EndGroup();      // ADDED: End the group
    }
    ImGui::End();  // MOVED: Always call End() after Begin()
}
