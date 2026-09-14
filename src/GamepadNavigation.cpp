#include "GamepadNavigation.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "TextureLoader.h"
#include "Translations.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace UI::GamepadNavigation {
    namespace {
        constexpr std::size_t kAreaCount = static_cast<std::size_t>(Area::Count);
        constexpr float kScrollSpeed = 900.0f;
        constexpr float kNavigationRepeatDelay = 0.40f;
        constexpr float kNavigationRepeatRate = 0.14f;

        struct NavigableItem {
            ImGuiID Id = 0;
            ImGuiWindow* Window = nullptr;
            ImGuiID FocusScopeId = 0;
            ImGuiNavLayer Layer = ImGuiNavLayer_Main;
            ImRect Rect{};
        };

        struct ItemRow {
            std::size_t First = 0;
            std::size_t Last = 0;
        };

        enum class IconSlot { Up, Down, Left, Right, Confirm, Cancel, Options, RightShoulder, RightStick, Count };

        struct IconAsset {
            std::string Path;
            ImTextureID Texture = nullptr;
        };

        struct Hint {
            IconSlot PrimaryIcon;
            IconSlot SecondaryIcon;
            const char* TranslationKey;
        };

        std::array<std::vector<NavigableItem>, kAreaCount> previousItems;
        std::array<std::vector<NavigableItem>, kAreaCount> currentItems;
        std::array<std::size_t, kAreaCount> focusedIndices{};
        std::array<ImGuiWindow*, kAreaCount> areaWindows{};

        std::array<IconAsset, static_cast<std::size_t>(IconSlot::Count)> iconAssets;

        Area activeArea = Area::PageTree;
        Area areaBeforeOptions = Area::PageTree;
        Area recordingArea = Area::PageTree;
        std::optional<Area> requestedFocus;

        ImGuiWindow* panelWindow = nullptr;
        ImVec4 focusAccent{};
        bool gamepadActive = false;
        bool gamepadBecameActive = false;
        bool suspended = false;
        bool wasSuspended = false;
        bool recordingItems = false;
        bool optionsVisible = false;
        bool optionsNeedFocus = false;
        bool optionsToggleRequested = false;
        bool focusStylePushed = false;
        bool iconSetInitialized = false;
        bool iconSetUsesPlayStation = false;
        bool iconsAvailable = false;
        bool missingIconsLogged = false;
        int lastRenderedFrame = -1;

        std::size_t AreaIndex(Area area) { return static_cast<std::size_t>(area); }

        bool IsPanelNavigationActive() { return gamepadActive && !suspended; }

        bool IsWindowInside(const ImGuiWindow* window, const ImGuiWindow* ancestor) {
            for (const ImGuiWindow* current = window; current; current = current->ParentWindowInBeginStack) {
                if (current == ancestor) {
                    return true;
                }
            }
            return false;
        }

        bool IsChildContainer(const ImGuiContext* context, const ImGuiWindow* parent, ImGuiID id) {
            for (ImGuiWindow* candidate : context->Windows) {
                if (candidate->ParentWindow == parent && candidate->ChildId == id) {
                    return true;
                }
            }
            return false;
        }

        void ObserveItem(ImGuiContext* context, ImGuiWindow* window, const ImGuiLastItemData* itemData) {
            if (!IsPanelNavigationActive() || !recordingItems || !context || !window || !itemData ||
                itemData->ID == 0) {
                return;
            }
            if (itemData->InFlags & (ImGuiItemFlags_NoNav | ImGuiItemFlags_Disabled)) {
                return;
            }
            if (window->Flags & ImGuiWindowFlags_Tooltip) {
                return;
            }
            if ((window->Flags & ImGuiWindowFlags_Popup) &&
                (recordingArea != Area::OptionsMenu || window != areaWindows[AreaIndex(Area::OptionsMenu)])) {
                return;
            }
            if (itemData->NavRect.GetWidth() <= 0.0f || itemData->NavRect.GetHeight() <= 0.0f) {
                return;
            }
            if (IsChildContainer(context, window, itemData->ID)) {
                return;
            }

            auto& items = currentItems[AreaIndex(recordingArea)];
            const auto duplicate = std::find_if(items.begin(), items.end(), [&](const NavigableItem& item) {
                return item.Id == itemData->ID && item.Window == window;
            });
            if (duplicate != items.end()) {
                return;
            }

            items.push_back(
                {itemData->ID, window, context->CurrentFocusScopeId, window->DC.NavLayerCurrent, itemData->NavRect});
        }

        void CancelDefaultMoveRequest() {
            ImGuiContext& context = *GImGui;
            if (context.NavMoveSubmitted || context.NavMoveScoringItems) {
                ImGui::NavMoveRequestCancel();
            }
        }

        bool IsPressedWithNavigationRepeat(ImGuiKey key) {
            if (ImGui::IsKeyPressed(key, false)) {
                return true;
            }

            const ImGuiKeyData* keyData = ImGui::GetKeyData(key);
            return keyData->Down && ImGui::CalcTypematicRepeatAmount(keyData->DownDurationPrev, keyData->DownDuration,
                                                                     kNavigationRepeatDelay, kNavigationRepeatRate) > 0;
        }

        bool IsDirectionPressed(ImGuiKey dpadKey, ImGuiKey stickKey) {
            return IsPressedWithNavigationRepeat(dpadKey) || IsPressedWithNavigationRepeat(stickKey);
        }

        bool IsDirectionDown(ImGuiKey dpadKey, ImGuiKey stickKey) {
            return ImGui::IsKeyDown(dpadKey) || ImGui::IsKeyDown(stickKey);
        }

        bool IsPopupBlockingAreaNavigation() {
            return ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        }

        bool IsPopupBlockingAreaNavigation(Area area) {
            if (!IsPopupBlockingAreaNavigation()) {
                return false;
            }
            if (area != Area::OptionsMenu) {
                return true;
            }

            ImGuiWindow* navWindow = GImGui->NavWindow;
            ImGuiWindow* optionsWindow = areaWindows[AreaIndex(Area::OptionsMenu)];
            return navWindow && (navWindow->Flags & ImGuiWindowFlags_Popup) && navWindow != optionsWindow;
        }

        void FocusWindowWithoutItem(ImGuiWindow* window) {
            if (!window) {
                return;
            }

            ImGuiContext& context = *GImGui;
            ImGui::FocusWindow(window, ImGuiFocusRequestFlags_UnlessBelowModal);
            ImGui::NavInitWindow(window, true);
            context.NavInputSource = ImGuiInputSource_Gamepad;
            context.NavDisableHighlight = false;
        }

        void FocusItem(const NavigableItem& item) {
            if (!item.Window || (!item.Window->Active && !item.Window->WasActive)) {
                return;
            }

            ImGuiContext& context = *GImGui;
            ImGui::FocusWindow(item.Window, ImGuiFocusRequestFlags_UnlessBelowModal);
            ImGui::SetNavID(item.Id, item.Layer, item.FocusScopeId, ImGui::WindowRectAbsToRel(item.Window, item.Rect));
            ImGui::ScrollToRectEx(item.Window, item.Rect,
                                  ImGuiScrollFlags_KeepVisibleEdgeX | ImGuiScrollFlags_KeepVisibleEdgeY);
            context.NavInputSource = ImGuiInputSource_Gamepad;
            context.NavDisableHighlight = false;
        }

        std::optional<std::size_t> FindFocusedItem(const std::vector<NavigableItem>& items) {
            const ImGuiContext& context = *GImGui;
            for (std::size_t index = 0; index < items.size(); ++index) {
                if (items[index].Id == context.NavId && items[index].Window == context.NavWindow) {
                    return index;
                }
            }
            return std::nullopt;
        }

        std::size_t GetFocusedIndex(Area area, const std::vector<NavigableItem>& items) {
            const std::optional<std::size_t> focusedItem = FindFocusedItem(items);
            return focusedItem.value_or(std::min(focusedIndices[AreaIndex(area)], items.size() - 1));
        }

        bool ItemsShareRow(const NavigableItem& first, const NavigableItem& second) {
            if (first.Window != second.Window) {
                return false;
            }

            const float overlap =
                std::min(first.Rect.Max.y, second.Rect.Max.y) - std::max(first.Rect.Min.y, second.Rect.Min.y);
            const float smallerHeight = std::min(first.Rect.GetHeight(), second.Rect.GetHeight());
            return overlap >= smallerHeight * 0.5f;
        }

        ItemRow FindItemRow(const std::vector<NavigableItem>& items, std::size_t index) {
            ItemRow row{index, index};
            while (row.First > 0 && ItemsShareRow(items[index], items[row.First - 1])) {
                --row.First;
            }
            while (row.Last + 1 < items.size() && ItemsShareRow(items[index], items[row.Last + 1])) {
                ++row.Last;
            }
            return row;
        }

        void MoveSequentially(Area area, int direction) {
            const std::size_t areaIndex = AreaIndex(area);
            const auto& items = previousItems[areaIndex];
            if (items.empty()) {
                requestedFocus = area;
                return;
            }

            std::size_t index = GetFocusedIndex(area, items);
            if (direction < 0) {
                index = index == 0 ? items.size() - 1 : index - 1;
            } else {
                index = (index + 1) % items.size();
            }

            focusedIndices[areaIndex] = index;
            FocusItem(items[index]);
        }

        void MoveVertically(Area area, int direction) {
            const std::size_t areaIndex = AreaIndex(area);
            const auto& items = previousItems[areaIndex];
            if (items.empty()) {
                requestedFocus = area;
                return;
            }

            const std::size_t currentIndex = GetFocusedIndex(area, items);
            const ItemRow currentRow = FindItemRow(items, currentIndex);
            const std::size_t adjacentIndex = direction < 0
                                                  ? (currentRow.First == 0 ? items.size() - 1 : currentRow.First - 1)
                                                  : (currentRow.Last + 1 == items.size() ? 0 : currentRow.Last + 1);
            const std::size_t targetIndex = FindItemRow(items, adjacentIndex).First;

            focusedIndices[areaIndex] = targetIndex;
            FocusItem(items[targetIndex]);
        }

        bool HasHorizontalTargets(Area area) {
            const auto& items = previousItems[AreaIndex(area)];
            if (items.empty()) {
                return false;
            }

            const std::size_t currentIndex = GetFocusedIndex(area, items);
            const ItemRow row = FindItemRow(items, currentIndex);
            return row.First != row.Last;
        }

        void MoveHorizontally(Area area, int direction) {
            const std::size_t areaIndex = AreaIndex(area);
            const auto& items = previousItems[areaIndex];
            const std::size_t currentIndex = GetFocusedIndex(area, items);
            const ItemRow row = FindItemRow(items, currentIndex);
            const std::size_t targetIndex = direction < 0 ? (currentIndex == row.First ? row.Last : currentIndex - 1)
                                                          : (currentIndex == row.Last ? row.First : currentIndex + 1);

            focusedIndices[areaIndex] = targetIndex;
            FocusItem(items[targetIndex]);
        }

        void ProcessAreaInput(Area area) {
            if (activeArea != area || IsPopupBlockingAreaNavigation(area) || ImGui::IsAnyItemActive()) {
                return;
            }

            const bool previousDown = IsDirectionDown(ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadLStickUp);
            const bool nextDown = IsDirectionDown(ImGuiKey_GamepadDpadDown, ImGuiKey_GamepadLStickDown);
            if (previousDown || nextDown) {
                CancelDefaultMoveRequest();
            }

            const bool previous = IsDirectionPressed(ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadLStickUp);
            const bool next = IsDirectionPressed(ImGuiKey_GamepadDpadDown, ImGuiKey_GamepadLStickDown);
            if (previous != next) {
                if (area == Area::PageTree) {
                    MoveVertically(area, previous ? -1 : 1);
                } else {
                    MoveSequentially(area, previous ? -1 : 1);
                }
                return;
            }

            if (previousDown || nextDown || area != Area::PageTree || !HasHorizontalTargets(area)) {
                return;
            }

            const bool leftDown = IsDirectionDown(ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadLStickLeft);
            const bool rightDown = IsDirectionDown(ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickRight);
            if (leftDown || rightDown) {
                CancelDefaultMoveRequest();
            }

            const bool left = IsDirectionPressed(ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadLStickLeft);
            const bool right = IsDirectionPressed(ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickRight);
            if (left != right) {
                MoveHorizontally(area, left ? -1 : 1);
            }
        }

        void ScrollArea(Area area) {
            if (activeArea != area || IsPopupBlockingAreaNavigation(area)) {
                return;
            }

            const float direction = (ImGui::IsKeyDown(ImGuiKey_GamepadRStickDown) ? 1.0f : 0.0f) -
                                    (ImGui::IsKeyDown(ImGuiKey_GamepadRStickUp) ? 1.0f : 0.0f);
            if (direction == 0.0f) {
                return;
            }

            ImGuiWindow* window = GImGui->NavWindow;
            const std::size_t areaIndex = AreaIndex(area);
            if (!window || !IsWindowInside(window, areaWindows[areaIndex])) {
                window = areaWindows[areaIndex];
            }
            if (!window) {
                return;
            }

            const float deltaTime = ImGui::GetIO().DeltaTime > 0.0f ? ImGui::GetIO().DeltaTime : 1.0f / 60.0f;
            ImGui::SetScrollY(window, window->Scroll.y + direction * kScrollSpeed * deltaTime);
        }

        const char* IconName(IconSlot slot, bool playStation) {
            switch (slot) {
                case IconSlot::Up:
                    return "Up";
                case IconSlot::Down:
                    return "Down";
                case IconSlot::Left:
                    return "Left";
                case IconSlot::Right:
                    return "Right";
                case IconSlot::Confirm:
                    return playStation ? "PS3_A" : "360_A";
                case IconSlot::Cancel:
                    return playStation ? "PS3_B" : "360_B";
                case IconSlot::Options:
                    return playStation ? "PS3_X" : "360_X";
                case IconSlot::RightShoulder:
                    return playStation ? "PS3_RB" : "360_RB";
                case IconSlot::RightStick:
                    return playStation ? "PS3_R3" : "360_RS";
                case IconSlot::Count:
                    return "UnknownKey";
            }
            return "UnknownKey";
        }

        bool UsesPlayStationIcons() {
            const auto controlMap = RE::ControlMap::GetSingleton();
            return controlMap && controlMap->GetGamePadType() == RE::PC_GAMEPAD_TYPE::kOrbis;
        }

        void EnsureIconsLoaded() {
            const bool usePlayStation = UsesPlayStationIcons();
            if (iconSetInitialized && iconSetUsesPlayStation == usePlayStation) {
                return;
            }

            iconSetInitialized = true;
            iconSetUsesPlayStation = usePlayStation;
            iconsAvailable = true;

            for (std::size_t index = 0; index < iconAssets.size(); ++index) {
                IconAsset& asset = iconAssets[index];
                const IconSlot slot = static_cast<IconSlot>(index);
                asset.Path = std::format("Data/Interface/ImGuiIcons/Icons/{}.png", IconName(slot, usePlayStation));
                asset.Texture = nullptr;

                std::error_code error;
                if (!std::filesystem::exists(asset.Path, error) || error) {
                    iconsAvailable = false;
                    if (!missingIconsLogged) {
                        logger::error("ImGui Icons is required for gamepad prompts; missing '{}'", asset.Path);
                        missingIconsLogged = true;
                    }
                    continue;
                }

                asset.Texture = TextureLoader::GetTexture(asset.Path);
                if (!asset.Texture) {
                    iconsAvailable = false;
                    if (!missingIconsLogged) {
                        logger::error("Could not load required ImGui Icons texture '{}'", asset.Path);
                        missingIconsLogged = true;
                    }
                }
            }
        }

        const IconAsset& GetIcon(IconSlot slot) { return iconAssets[static_cast<std::size_t>(slot)]; }

        std::vector<Hint> GetHints(bool hasPage) {
            constexpr IconSlot noSecondaryIcon = IconSlot::Count;
            switch (activeArea) {
                case Area::OptionsMenu:
                case Area::Popup:
                    return {
                        {IconSlot::Up, IconSlot::Down, "Gamepad.Navigate"},
                        {IconSlot::Confirm, noSecondaryIcon, "Gamepad.Select"},
                        {IconSlot::Cancel, noSecondaryIcon, "Gamepad.Back"},
                    };
                case Area::PageContent:
                    return {
                        {IconSlot::Up, IconSlot::Down, "Gamepad.Navigate"},
                        {IconSlot::Left, IconSlot::Right, "Gamepad.Adjust"},
                        {IconSlot::Confirm, noSecondaryIcon, "Gamepad.Select"},
                        {IconSlot::RightStick, noSecondaryIcon, "Gamepad.Scroll"},
                        {IconSlot::Cancel, noSecondaryIcon, "Gamepad.Back"},
                    };
                case Area::PageTree: {
                    std::vector<Hint> hints{
                        {IconSlot::Up, IconSlot::Down, "Gamepad.Navigate"},
                        {IconSlot::Confirm, noSecondaryIcon, "Gamepad.Select"},
                        {IconSlot::Options, noSecondaryIcon, "Gamepad.Options"},
                    };
                    if (HasHorizontalTargets(Area::PageTree)) {
                        hints.insert(hints.begin() + 1, Hint{IconSlot::Left, IconSlot::Right, "Gamepad.Actions"});
                    }
                    if (hasPage) {
                        hints.push_back({IconSlot::RightShoulder, noSecondaryIcon, "Gamepad.Controls"});
                    }
                    hints.push_back({IconSlot::Cancel, noSecondaryIcon, "Gamepad.Back"});
                    return hints;
                }
                case Area::Count:
                    break;
            }
            return {};
        }

        float HintWidth(const Hint& hint, float iconSize) {
            float width = iconSize;
            if (hint.SecondaryIcon != IconSlot::Count) {
                width += iconSize + ImGui::GetStyle().ItemInnerSpacing.x;
            }
            width += ImGui::GetStyle().ItemInnerSpacing.x;
            width += ImGui::CalcTextSize(Translations::Get(hint.TranslationKey)).x;
            return width;
        }

        void RenderIcon(IconSlot slot, float size) {
            const IconAsset& asset = GetIcon(slot);
            ImGui::Image(asset.Texture, ImVec2(size, size));
        }

        void RenderHint(const Hint& hint, float iconSize) {
            ImGui::BeginGroup();
            RenderIcon(hint.PrimaryIcon, iconSize);
            if (hint.SecondaryIcon != IconSlot::Count) {
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                RenderIcon(hint.SecondaryIcon, iconSize);
            }
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(Translations::Get(hint.TranslationKey));
            ImGui::EndGroup();
        }
    }

    void BeginFrame(bool hasPage, bool shouldSuspend) {
        ImGui::SetItemAddObserver(ObserveItem);

        panelWindow = ImGui::GetCurrentWindow();
        focusAccent = ImGui::GetStyleColorVec4(ImGuiCol_NavHighlight);
        suspended = shouldSuspend;
        optionsVisible = false;
        optionsToggleRequested = false;
        recordingItems = false;
        areaWindows.fill(nullptr);
        for (auto& items : currentItems) {
            items.clear();
        }

        const int frame = ImGui::GetFrameCount();
        const bool firstFrameAfterOpen = lastRenderedFrame < 0 || frame != lastRenderedFrame + 1;
        lastRenderedFrame = frame;

        if (!hasPage) {
            previousItems[AreaIndex(Area::PageContent)].clear();
            focusedIndices[AreaIndex(Area::PageContent)] = 0;
            if (activeArea == Area::PageContent) {
                activeArea = Area::PageTree;
            }
        }

        if (!IsPanelNavigationActive()) {
            wasSuspended = suspended;
            return;
        }

        if (firstFrameAfterOpen || gamepadBecameActive || (wasSuspended && !suspended)) {
            activeArea = Area::PageTree;
            requestedFocus = Area::PageTree;
            gamepadBecameActive = false;
        }
        wasSuspended = suspended;

        optionsToggleRequested = ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false) && !ImGui::IsAnyItemActive();

        if (!IsPopupBlockingAreaNavigation() && !ImGui::IsAnyItemActive()) {
            if (hasPage && ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false)) {
                RequestFocus(Area::PageContent);
            }
        }
    }

    void EndFrame() {
        recordingItems = false;

        if (IsPanelNavigationActive() && activeArea == Area::OptionsMenu && !optionsVisible) {
            activeArea = areaBeforeOptions;
            requestedFocus = activeArea;
        }

        ImGui::SetItemAddObserver(nullptr);
    }

    void NotifyInputDevice(RE::INPUT_DEVICE device) {
        if (device == RE::INPUT_DEVICE::kGamepad) {
            if (!gamepadActive) {
                gamepadBecameActive = true;
            }
            gamepadActive = true;
            return;
        }

        if (device == RE::INPUT_DEVICE::kKeyboard || device == RE::INPUT_DEVICE::kMouse) {
            gamepadActive = false;
            gamepadBecameActive = false;
            requestedFocus.reset();
        }
    }

    void NotifyGamepadAnalogInput() {
        if (!gamepadActive) {
            gamepadBecameActive = true;
        }
        gamepadActive = true;
    }

    bool IsActive() { return IsPanelNavigationActive(); }

    bool IsOptionsToggleRequested() { return IsPanelNavigationActive() && optionsToggleRequested; }

    void RequestFocus(Area area) {
        if (!IsPanelNavigationActive() || area == Area::Count) {
            return;
        }
        CancelDefaultMoveRequest();
        activeArea = area;
        requestedFocus = area;
    }

    void BeginArea(Area area) {
        const std::size_t areaIndex = AreaIndex(area);
        areaWindows[areaIndex] = ImGui::GetCurrentWindow();
        recordingArea = area;
        recordingItems = IsPanelNavigationActive() &&
                         (area == Area::PageTree || area == Area::PageContent || area == Area::OptionsMenu);

        if (!IsPanelNavigationActive()) {
            return;
        }

        ProcessAreaInput(area);
        ScrollArea(area);
    }

    void EndArea() {
        if (!recordingItems) {
            return;
        }

        recordingItems = false;
        const std::size_t areaIndex = AreaIndex(recordingArea);
        auto& items = currentItems[areaIndex];

        if (requestedFocus && *requestedFocus == recordingArea) {
            if (items.empty()) {
                FocusWindowWithoutItem(areaWindows[areaIndex]);
                focusedIndices[areaIndex] = 0;
            } else {
                focusedIndices[areaIndex] = std::min(focusedIndices[areaIndex], items.size() - 1);
                FocusItem(items[focusedIndices[areaIndex]]);
            }
            requestedFocus.reset();
        } else if (activeArea == recordingArea && !IsPopupBlockingAreaNavigation(recordingArea)) {
            const auto focusedItem = FindFocusedItem(items);
            if (focusedItem) {
                focusedIndices[areaIndex] = *focusedItem;
            } else if (!items.empty()) {
                focusedIndices[areaIndex] = std::min(focusedIndices[areaIndex], items.size() - 1);
                FocusItem(items[focusedIndices[areaIndex]]);
            }
        }

        previousItems[areaIndex] = items;
    }

    void BeginOptionsMenu() {
        optionsVisible = true;
        if (!IsPanelNavigationActive()) {
            return;
        }

        if (activeArea != Area::OptionsMenu) {
            areaBeforeOptions = activeArea;
            activeArea = Area::OptionsMenu;
            requestedFocus = Area::OptionsMenu;
            optionsNeedFocus = true;
        }

        if (optionsNeedFocus) {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            FocusWindowWithoutItem(window);
            optionsNeedFocus = false;
        }

        BeginArea(Area::OptionsMenu);
    }

    void NotifyOptionsMenuClosed() {
        if (activeArea != Area::OptionsMenu) {
            return;
        }
        activeArea = areaBeforeOptions;
        requestedFocus = activeArea;
        optionsNeedFocus = false;
    }

    void NotifyPageClosed() {
        previousItems[AreaIndex(Area::PageContent)].clear();
        focusedIndices[AreaIndex(Area::PageContent)] = 0;
        if (gamepadActive) {
            activeArea = Area::PageTree;
            requestedFocus = Area::PageTree;
        }
    }

    void PushFocusStyle() {
        if (!IsPanelNavigationActive() || focusStylePushed) {
            return;
        }
        ImGui::PushStyleColor(ImGuiCol_NavHighlight, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        focusStylePushed = true;
    }

    void PopFocusStyle() {
        if (!focusStylePushed) {
            return;
        }
        ImGui::PopStyleColor();
        focusStylePushed = false;
    }

    void RenderFocusedItemHighlight() {
        if (!IsPanelNavigationActive() || !panelWindow) {
            return;
        }

        ImGuiContext& context = *GImGui;
        ImGuiWindow* window = context.NavWindow;
        if (!window || context.NavId == 0 || !IsWindowInside(window, panelWindow)) {
            return;
        }

        ImRect rect = ImGui::WindowRectRelToAbs(window, window->NavRectRel[context.NavLayer]);
        rect.Expand(2.0f);
        rect.ClipWith(window->ClipRect);
        if (rect.IsInverted() || rect.GetWidth() <= 0.0f || rect.GetHeight() <= 0.0f) {
            return;
        }

        const bool active = context.ActiveId == context.NavId;
        ImVec4 fillColor = focusAccent;
        ImVec4 glowColor = focusAccent;
        ImVec4 outlineColor = focusAccent;
        fillColor.w *= active ? 0.30f : 0.16f;
        glowColor.w *= active ? 0.55f : 0.32f;
        outlineColor.w = std::max(outlineColor.w, active ? 0.95f : 0.78f);

        const float rounding = std::max(ImGui::GetStyle().FrameRounding, 3.0f);
        ImDrawList* drawList = ImGui::GetForegroundDrawList(window);
        drawList->PushClipRect(window->ClipRect.Min, window->ClipRect.Max, true);
        drawList->AddRectFilled(rect.Min, rect.Max, ImGui::ColorConvertFloat4ToU32(fillColor), rounding);

        ImRect glowRect = rect;
        glowRect.Expand(2.0f);
        drawList->AddRect(glowRect.Min, glowRect.Max, ImGui::ColorConvertFloat4ToU32(glowColor), rounding + 2.0f,
                          ImDrawFlags_RoundCornersAll, active ? 4.0f : 3.0f);
        drawList->AddRect(rect.Min, rect.Max, ImGui::ColorConvertFloat4ToU32(outlineColor), rounding,
                          ImDrawFlags_RoundCornersAll, active ? 3.0f : 2.0f);

        const float markerWidth = active ? 5.0f : 3.0f;
        drawList->AddRectFilled(rect.Min, ImVec2(rect.Min.x + markerWidth, rect.Max.y),
                                ImGui::ColorConvertFloat4ToU32(outlineColor), rounding, ImDrawFlags_RoundCornersLeft);
        drawList->PopClipRect();
    }

    float GetHintBarHeight() {
        if (!IsPanelNavigationActive()) {
            return 0.0f;
        }
        return ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().WindowPadding.y;
    }

    void RenderHintBar(bool hasPage) {
        if (!IsPanelNavigationActive()) {
            return;
        }

        EnsureIconsLoaded();
        const float height = GetHintBarHeight();
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::BeginChild("##MCPGamepadHints", ImVec2(0.0f, height), ImGuiChildFlags_None, flags);
        ImGui::Separator();

        if (!iconsAvailable) {
            const char* warning = Translations::Get("Gamepad.ImGuiIconsMissing");
            const float warningWidth = ImGui::CalcTextSize(warning).x;
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowSize().x - warningWidth) * 0.5f));
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "%s", warning);
            ImGui::EndChild();
            return;
        }

        const std::vector<Hint> hints = GetHints(hasPage);
        const float iconSize = ImGui::GetTextLineHeight();
        const float itemSpacing = ImGui::GetStyle().ItemSpacing.x * 2.0f;
        float totalWidth = 0.0f;
        for (const Hint& hint : hints) {
            totalWidth += HintWidth(hint, iconSize);
        }
        if (hints.size() > 1) {
            totalWidth += itemSpacing * static_cast<float>(hints.size() - 1);
        }

        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowSize().x - totalWidth) * 0.5f));
        for (std::size_t index = 0; index < hints.size(); ++index) {
            RenderHint(hints[index], iconSize);
            if (index + 1 < hints.size()) {
                ImGui::SameLine(0.0f, itemSpacing);
            }
        }
        ImGui::EndChild();
    }
}
