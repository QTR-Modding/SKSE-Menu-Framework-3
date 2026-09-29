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
        bool isVanillaFallback = false;
        bool bold = false;
        bool italic = false;
    };

    // Keeps regular Futura as the base and resolves supplemental $StartMenuFont
    // faces from Interface/fontconfig*.txt through BSResource, including overrides.
    // Vanilla supplemental faces are used when configuration cannot be resolved.
    std::vector<FontData> LoadMenuFonts();
}
