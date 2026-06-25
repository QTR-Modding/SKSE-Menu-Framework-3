#include <imgui.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include "Theme.h"
#include "Utils.h"
static ImVec4 HexToImVec4(const std::string& hex) {
    unsigned int v = 0;
    std::stringstream ss;
    ss << std::hex << hex.substr(1);
    ss >> v;
    float a = ((v >> 0) & 0xFF) / 255.0f;
    float b = ((v >> 8) & 0xFF) / 255.0f;
    float g = ((v >> 16) & 0xFF) / 255.0f;
    float r = ((v >> 24) & 0xFF) / 255.0f;
    return ImVec4(r, g, b, a);
}

std::vector<std::string> Theme::GetJsonFiles() {
    std::string folder = "Data\\SKSE\\plugins\\SKSEMenuFrameworkThemes";
    std::vector<std::string> out;
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.is_regular_file()) {
            auto path = entry.path();
            if (path.extension() == ".json") {
                out.push_back(Utils::toUpperCase(path.stem().string().c_str()));
            }
        }
    }
    return out;
}

void Theme::LoadJsonStyle(const std::string& path) {
    std::ifstream f("Data\\SKSE\\plugins\\SKSEMenuFrameworkThemes\\" + path + ".json");
    if (!f.good()) return;

    nlohmann::json j;
    f >> j;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBarBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
    style.FramePadding = ImVec2(4.0f, 3.0f);
    style.MouseCursorScale = 4.0f;

    ImGuiStyle& s = ImGui::GetStyle();

    if (j.contains("Alpha")) s.Alpha = j["Alpha"];
    if (j.contains("DisabledAlpha")) s.DisabledAlpha = j["DisabledAlpha"];
    if (j.contains("WindowPadding")) s.WindowPadding = ImVec2(j["WindowPadding"][0], j["WindowPadding"][1]);
    if (j.contains("WindowRounding")) s.WindowRounding = j["WindowRounding"];
    if (j.contains("WindowBorderSize")) s.WindowBorderSize = j["WindowBorderSize"];
    if (j.contains("WindowMinSize")) s.WindowMinSize = ImVec2(j["WindowMinSize"][0], j["WindowMinSize"][1]);
    if (j.contains("WindowTitleAlign")) s.WindowTitleAlign = ImVec2(j["WindowTitleAlign"][0], j["WindowTitleAlign"][1]);
    if (j.contains("ChildRounding")) s.ChildRounding = j["ChildRounding"];
    if (j.contains("ChildBorderSize")) s.ChildBorderSize = j["ChildBorderSize"];
    if (j.contains("PopupRounding")) s.PopupRounding = j["PopupRounding"];
    if (j.contains("PopupBorderSize")) s.PopupBorderSize = j["PopupBorderSize"];
    if (j.contains("FramePadding")) s.FramePadding = ImVec2(j["FramePadding"][0], j["FramePadding"][1]);
    if (j.contains("FrameRounding")) s.FrameRounding = j["FrameRounding"];
    if (j.contains("FrameBorderSize")) s.FrameBorderSize = j["FrameBorderSize"];
    if (j.contains("ItemSpacing")) s.ItemSpacing = ImVec2(j["ItemSpacing"][0], j["ItemSpacing"][1]);
    if (j.contains("ItemInnerSpacing")) s.ItemInnerSpacing = ImVec2(j["ItemInnerSpacing"][0], j["ItemInnerSpacing"][1]);
    if (j.contains("CellPadding")) s.CellPadding = ImVec2(j["CellPadding"][0], j["CellPadding"][1]);
    if (j.contains("TouchExtraPadding"))
        s.TouchExtraPadding = ImVec2(j["TouchExtraPadding"][0], j["TouchExtraPadding"][1]);
    if (j.contains("IndentSpacing")) s.IndentSpacing = j["IndentSpacing"];
    if (j.contains("ColumnsMinSpacing")) s.ColumnsMinSpacing = j["ColumnsMinSpacing"];
    if (j.contains("ScrollbarSize")) s.ScrollbarSize = j["ScrollbarSize"];
    if (j.contains("ScrollbarRounding")) s.ScrollbarRounding = j["ScrollbarRounding"];
    if (j.contains("GrabMinSize")) s.GrabMinSize = j["GrabMinSize"];
    if (j.contains("GrabRounding")) s.GrabRounding = j["GrabRounding"];
    if (j.contains("LogSliderDeadzone")) s.LogSliderDeadzone = j["LogSliderDeadzone"];
    if (j.contains("TabRounding")) s.TabRounding = j["TabRounding"];
    if (j.contains("TabBorderSize")) s.TabBorderSize = j["TabBorderSize"];
    if (j.contains("TabMinWidthForCloseButton")) s.TabMinWidthForCloseButton = j["TabMinWidthForCloseButton"];
    if (j.contains("TabBarBorderSize")) s.TabBarBorderSize = j["TabBarBorderSize"];
    if (j.contains("TableAngledHeadersAngle")) s.TableAngledHeadersAngle = j["TableAngledHeadersAngle"];
    if (j.contains("TableAngledHeadersTextAlign"))
        s.TableAngledHeadersTextAlign =
            ImVec2(j["TableAngledHeadersTextAlign"][0], j["TableAngledHeadersTextAlign"][1]);
    if (j.contains("ButtonTextAlign")) s.ButtonTextAlign = ImVec2(j["ButtonTextAlign"][0], j["ButtonTextAlign"][1]);
    if (j.contains("SelectableTextAlign"))
        s.SelectableTextAlign = ImVec2(j["SelectableTextAlign"][0], j["SelectableTextAlign"][1]);
    if (j.contains("SeparatorTextBorderSize")) s.SeparatorTextBorderSize = j["SeparatorTextBorderSize"];
    if (j.contains("SeparatorTextAlign"))
        s.SeparatorTextAlign = ImVec2(j["SeparatorTextAlign"][0], j["SeparatorTextAlign"][1]);
    if (j.contains("SeparatorTextPadding"))
        s.SeparatorTextPadding = ImVec2(j["SeparatorTextPadding"][0], j["SeparatorTextPadding"][1]);
    if (j.contains("DisplayWindowPadding"))
        s.DisplayWindowPadding = ImVec2(j["DisplayWindowPadding"][0], j["DisplayWindowPadding"][1]);
    if (j.contains("DisplaySafeAreaPadding"))
        s.DisplaySafeAreaPadding = ImVec2(j["DisplaySafeAreaPadding"][0], j["DisplaySafeAreaPadding"][1]);
    if (j.contains("DockingSeparatorSize")) s.DockingSeparatorSize = j["DockingSeparatorSize"];
    if (j.contains("MouseCursorScale")) s.MouseCursorScale = j["MouseCursorScale"];
    if (j.contains("AntiAliasedLines")) s.AntiAliasedLines = j["AntiAliasedLines"];
    if (j.contains("AntiAliasedLinesUseTex")) s.AntiAliasedLinesUseTex = j["AntiAliasedLinesUseTex"];
    if (j.contains("AntiAliasedFill")) s.AntiAliasedFill = j["AntiAliasedFill"];
    if (j.contains("CurveTessellationTol")) s.CurveTessellationTol = j["CurveTessellationTol"];
    if (j.contains("CircleTessellationMaxError")) s.CircleTessellationMaxError = j["CircleTessellationMaxError"];

    if (!j.contains("ImGuiCol")) return;

    auto& cols = j["ImGuiCol"];
    ImVec4* c = s.Colors;

    for (auto& e : cols.items()) {
        const std::string& k = e.key();
        const std::string& v = e.value();

        for (int i = 0; i < ImGuiCol_COUNT; i++) {
            if (std::string(ImGui::GetStyleColorName(i)) == k) {
                c[i] = HexToImVec4(v);
                break;
            }
        }
    }
}
