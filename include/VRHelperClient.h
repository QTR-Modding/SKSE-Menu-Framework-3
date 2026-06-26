#pragma once

namespace VRHelperClient {
    // Connect to the ImGuiVRHelper overlay (VR only). Call at kPostPostLoad so
    // the helper's messaging listener is registered regardless of load order.
    void ConnectVRHelper();
    void Update();
}