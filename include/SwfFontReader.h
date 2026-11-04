#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SwfFontReader {
    enum class SupplementalGlyphLanguage : std::uint8_t { None, Chinese, Japanese, Configured };

    struct FontData {
        std::string name;
        std::string sourceName;
        std::vector<std::string> aliases;
        std::vector<std::uint8_t> trueTypeData;
        std::string supplementalName;
        std::vector<std::uint8_t> supplementalTrueTypeData;
        SupplementalGlyphLanguage supplementalLanguage = SupplementalGlyphLanguage::None;
        // ascent - descent of the converted faces, in TrueType units. ImGui sizes a face by this extent.
        std::int32_t lineExtent = 0;
        std::int32_t supplementalLineExtent = 0;
        bool isVanillaFallback = false;
        bool bold = false;
        bool italic = false;
    };

    // Loads Futura composites and independently selectable $StartMenuFont replacements
    // from Interface/fontconfig*.txt through BSResource, including overrides.
    // Known vanilla localized faces stay supplemental; vanilla fonts are used
    // when configuration cannot be resolved.
    std::vector<FontData> LoadMenuFonts();
}
