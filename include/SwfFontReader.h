#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SwfFontReader {
    enum class SupplementalGlyphLanguage : std::uint8_t { None, Chinese, Japanese };

    struct FontData {
        std::string name;
        std::string sourceName;
        std::vector<std::string> aliases;
        std::vector<std::uint8_t> trueTypeData;
        std::string supplementalName;
        std::vector<std::uint8_t> supplementalTrueTypeData;
        SupplementalGlyphLanguage supplementalLanguage = SupplementalGlyphLanguage::None;
        bool bold = false;
        bool italic = false;
    };

    // Reads the regular Futura Condensed face from each of Skyrim's
    // game-relative Interface/fonts_*.swf resources. Localized supplemental
    // glyph faces remain attached to their Futura face and are not exposed as
    // separate choices. Loose files and files supplied by the game's archives
    // are both handled by BSResource.
    std::vector<FontData> LoadRegularFuturaFonts();
}
