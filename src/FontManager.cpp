#include "FontManager.h"
#include "Config.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <ranges>
#include <system_error>
#include <vector>

#define ICON_MIN_FA 0xe005
#define ICON_MAX_FA 0xf8ff

namespace {
    constexpr ImWchar TURKISH_GLYPH_RANGES[] = {
        0x011E, 0x011F, // G with breve
        0x0130, 0x0131, // Dotted and dotless I
        0x015E, 0x015F, // S with cedilla
        0,
    };
    constexpr ImWchar POLISH_GLYPH_RANGES[] = {
        0x00D3, 0x00D3,  // O-acute (caps)
        0x00F3, 0x00F3,  // O-acute (small)
        0x0104, 0x0107,  // A-ogonek (caps/small), C-acute (caps/small)
        0x0118, 0x0119,  // E-ogonek (caps/small)
        0x0141, 0x0144,  // L-stroke (caps/small), N-acute (caps/small)
        0x015A, 0x015B,  // S-acute (caps/small)
        0x0179, 0x017C,  // Z-acute (caps/small), Z-dot (caps/small)
        0,
    };

    constexpr auto FONT_DIRECTORY = "Data/SKSE/Plugins/Fonts";

    std::string NormalizeFontName(std::string name) {
        std::ranges::transform(name, name.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return name;
    }

    bool IsSupportedFont(const std::filesystem::path& path) {
        const auto extension = NormalizeFontName(path.extension().string());
        return extension == ".ttf" || extension == ".otf";
    }

    bool IsFontAwesome(const std::string& name) {
        const auto normalizedName = NormalizeFontName(std::filesystem::path(name).filename().string());
        return normalizedName == "fa-solid-900.ttf" || normalizedName == "fa-regular-400.ttf" ||
               normalizedName == "fa-brands-400.ttf";
    }

    float GetFontSize(const std::filesystem::path& fontPath, float fallback) {
        auto configPath = std::filesystem::path(FONT_DIRECTORY) / fontPath.filename();
        configPath.replace_extension(".json");
        if (!std::filesystem::exists(configPath)) {
            return fallback;
        }

        try {
            std::ifstream stream(configPath);
            const auto config = nlohmann::json::parse(stream);

            const char* sizeName = "fontSize";

            if (sizeName && config.contains(sizeName) && config[sizeName].is_number()) {
                const auto configuredSize = config[sizeName].get<float>();
                if (configuredSize > 0.0f) {
                    return configuredSize;
                }
            }
        } catch (const std::exception& exception) {
            SKSE::log::warn("FontLoader: Could not read '{}': {}", configPath.string(), exception.what());
        }

        return fallback;
    }

    void RegisterFont(FontContainer& container, const std::filesystem::path& path, ImFont* font) {
        if (!font) {
            return;
        }

        container.fonts[NormalizeFontName(path.filename().string())] = font;
        container.fonts[NormalizeFontName(path.stem().string())] = font;
        if (!IsFontAwesome(path.filename().string())) {
            container.textFontNames.push_back(path.filename().string());
        }
    }
}

FontContainer FontManager::LoadFonts(ImGuiIO& io, float size) {
    auto result = FontContainer();

    SKSE::log::info("FontLoader: Begin loading process for font size {}.", size);

    // Check if a glyph range has already been constructed for this font size.
    if (persistentGlyphRanges.find(size) == persistentGlyphRanges.end()) {
        SKSE::log::info("FontLoader: No cached glyph ranges for size {}. Building new ones...", size);

        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesDefault());  // Basic English

        if (Config::EnableChinese) builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
        if (Config::EnableJapanese) builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
        if (Config::EnableKorean) builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
        if (Config::EnableCyrillic) builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        if (Config::EnableThai) builder.AddRanges(io.Fonts->GetGlyphRangesThai());
        if (Config::EnableTurkish) builder.AddRanges(TURKISH_GLYPH_RANGES);
        if (Config::EnablePolish) builder.AddRanges(POLISH_GLYPH_RANGES);

        builder.BuildRanges(&persistentGlyphRanges[size]);

        SKSE::log::info("FontLoader: Glyph ranges for size {} successfully built and cached.", size);
    } else {
        SKSE::log::info("FontLoader: Using cached glyph ranges for size {}.", size);
    }

    // =========================================================================
    //                            Font loading section
    // =========================================================================

    ImFontConfig font_config;
    font_config.PixelSnapH = true;

    if (Config::EnableChinese) 
    {
        font_config.OversampleH = 1;
        font_config.OversampleV = 1;
        io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
        io.Fonts->TexDesiredWidth = 16384;
    }

    std::vector<std::filesystem::path> fontFiles;
    std::error_code error;
    for (std::filesystem::directory_iterator iterator(FONT_DIRECTORY, error), end; !error && iterator != end;
         iterator.increment(error)) {
        if (iterator->is_regular_file(error) && IsSupportedFont(iterator->path())) {
            fontFiles.push_back(iterator->path());
        }
    }
    std::ranges::sort(fontFiles);

    if (error) {
        SKSE::log::error("FontLoader: Could not enumerate '{}': {}", FONT_DIRECTORY, error.message());
    }

    const auto primaryName = NormalizeFontName(Config::PrimaryFont);
    auto primary = std::ranges::find_if(fontFiles, [&](const auto& path) {
        return NormalizeFontName(path.filename().string()) == primaryName;
    });
    if (primary != fontFiles.end()) {
        result.defaultFontName = primary->filename().string();
        result.defaultFont = GetFont(io, result.defaultFontName, GetFontSize(*primary, size), &font_config,
            persistentGlyphRanges.at(size).Data);
        RegisterFont(result, *primary, result.defaultFont);
    }

    // rollback mechanism
    if (!result.defaultFont) {
        SKSE::log::warn("Primary font '{}' failed to load. Falling back to SkyrimMenuFont.ttf.", Config::PrimaryFont);
        result.defaultFontName = "SkyrimMenuFont.ttf";
        result.defaultFont = GetFont(io, result.defaultFontName, GetFontSize("SkyrimMenuFont.ttf", size), nullptr,
            io.Fonts->GetGlyphRangesDefault());
        RegisterFont(result, "SkyrimMenuFont.ttf", result.defaultFont);
    }

    // Merge default font and Font Awesome icon
    ImFontConfig merge_config;
    merge_config.MergeMode = true;
    merge_config.PixelSnapH = true;
    if (Config::EnableChinese) {
        merge_config.OversampleH = 1;
        merge_config.OversampleV = 1;
    }

    GetFont(io, "SkyrimMenuFont.ttf", GetFontSize("SkyrimMenuFont.ttf", size), &merge_config,
        io.Fonts->GetGlyphRangesDefault());

    static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    GetFont(io, "fa-solid-900.ttf", GetFontSize("fa-solid-900.ttf", size), &merge_config, icons_ranges);
    GetFont(io, "fa-regular-400.ttf", GetFontSize("fa-regular-400.ttf", size), &merge_config, icons_ranges);
    GetFont(io, "fa-brands-400.ttf", GetFontSize("fa-brands-400.ttf", size), &merge_config, icons_ranges);

    // Every font is also loaded as an independently selectable face. Both its
    // filename ("Example.ttf") and stem ("Example") are accepted by the API.
    for (const auto& path : fontFiles) {
        const auto filename = path.filename().string();
        if (result.fonts.contains(NormalizeFontName(filename))) {
            continue;
        }

        ImFont* font = nullptr;
        if (IsFontAwesome(filename)) {
            // Icon fonts do not contain ordinary text. Start a new composite
            // face with the primary text font, then merge only this icon style.
            const auto configuredSize = GetFontSize(path, size);
            font = GetFont(io, result.defaultFontName, configuredSize, &font_config, persistentGlyphRanges.at(size).Data);
            if (font) {
                ImFontConfig namedMergeConfig;
                namedMergeConfig.MergeMode = true;
                namedMergeConfig.PixelSnapH = true;
                if (Config::EnableChinese) {
                    namedMergeConfig.OversampleH = 1;
                    namedMergeConfig.OversampleV = 1;
                }
                GetFont(io, filename, configuredSize, &namedMergeConfig, icons_ranges);
            }
        } else {
            font = GetFont(io, filename, GetFontSize(path, size), &font_config, persistentGlyphRanges.at(size).Data);
        }
        RegisterFont(result, path, font);
        if (!font) {
            SKSE::log::warn("FontLoader: '{}' failed to load at size {}.", filename, size);
        }
    }

    std::ranges::sort(result.textFontNames);
    SKSE::log::info("Font loading process for size {} completed.", size);
    return result;
}
void FontManager::CleanFontStack() {
    auto ctx = ImGui::GetCurrentContext();
    while (ctx->FontStack.size() > 0) {
        ImGui::PopFont();
    }
}
void FontManager::CleanFont() {
    CleanFontStack();
    currentFont = Font::fontSizeDefault;
    currentFontName.clear();
}

void FontManager::RequestAtlasRebuild() {
    atlasRebuildRequested.store(true);
}

bool FontManager::ConsumeAtlasRebuildRequest() {
    return atlasRebuildRequested.exchange(false);
}

void FontManager::SetFont(Font font) {
    currentFont = font;
    ProcessFont();
}

void FontManager::SetFont(const std::string& name) {
    currentFontName = NormalizeFontName(std::filesystem::path(name).filename().string());
    ProcessFont();
}

void FontManager::ProcessFont() {
    auto& container = fontSizes["Default"];
    if (currentFontName.empty()) {
        if (container.defaultFont) {
            ImGui::PushFont(container.defaultFont);
        }
        return;
    }

    auto font = container.fonts.find(currentFontName);
    if (font == container.fonts.end()) {
        font = container.fonts.find(NormalizeFontName(std::filesystem::path(currentFontName).stem().string()));
    }
    if (font != container.fonts.end() && font->second) {
        ImGui::PushFont(font->second);
    } else {
        SKSE::log::warn("Font '{}' is not loaded for the selected size.", currentFontName);
    }
}

ImFont* FontManager::GetFont(ImGuiIO& io, std::string name, float size, const ImFontConfig* font_cfg = NULL,
                const ImWchar* glyph_ranges = NULL) {
    std::string path = "Data/SKSE/Plugins/Fonts/" + name;
    if (std::filesystem::exists(path)) {
        return io.Fonts->AddFontFromFileTTF(path.c_str(), size, font_cfg, glyph_ranges);
    }
    return nullptr;
}
