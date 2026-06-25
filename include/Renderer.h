#pragma once
#include "Config.h"
#include "Input.h"

namespace UI {

    class Renderer {
    public:
        static void ImGuiBegin();
        static void ImGuiEnd();
        static void RenderWindows();
        static inline std::atomic<bool> initialized{false};
    };

}
