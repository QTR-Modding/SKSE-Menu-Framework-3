#pragma once

#include <atomic>
#include "imgui_internal.h"

enum Font {
    none = 0,
    faSolid = 1 << 0,
    faRegular = 1 << 1,
    faBrands = 1 << 2,
    fontSizeDefault = 1 << 4,
};

struct FontContainer {
    ImFont* defaultFont = nullptr;
    std::string defaultFontName;
    std::map<std::string, ImFont*> fonts;
    std::vector<std::string> textFontNames;
};

class FontManager {
public:
    static void ProcessFont();
    static ImFont* GetFont(ImGuiIO& io, std::string name, float size, const ImFontConfig* font_cfg,
                    const ImWchar* glyph_ranges);
    static void SetFont(Font font);
    static void SetFont(const std::string& name);
    static FontContainer LoadFonts(ImGuiIO& io, float size);
    static void RequestAtlasRebuild();
    static bool ConsumeAtlasRebuildRequest();
    static void CleanFontStack();
    static void CleanFont();
    static inline std::map<std::string, FontContainer> fontSizes;
    static inline Font currentFont = Font::fontSizeDefault;
    static inline std::string currentFontName;
    static inline std::map<float, ImVector<ImWchar>> persistentGlyphRanges;
    static inline std::atomic_bool atlasRebuildRequested = false;
};

