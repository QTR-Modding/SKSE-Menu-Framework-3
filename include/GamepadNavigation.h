#pragma once

#include "RE/Skyrim.h"

namespace UI::GamepadNavigation {
    enum class Area { PageTree, PageContent, OptionsMenu, Popup, Count };

    void BeginFrame(bool hasPage, bool suspended);
    void EndFrame();

    void NotifyInputDevice(RE::INPUT_DEVICE device);
    void NotifyGamepadAnalogInput();

    bool IsActive();
    bool IsOptionsToggleRequested();

    void RequestFocus(Area area);
    void BeginArea(Area area);
    void EndArea();
    void BeginOptionsMenu();
    void NotifyOptionsMenuClosed();
    void NotifyPageClosed();

    void PushFocusStyle();
    void PopFocusStyle();
    void RenderFocusedItemHighlight();

    float GetHintBarHeight();
    void RenderHintBar(bool hasPage);
}
