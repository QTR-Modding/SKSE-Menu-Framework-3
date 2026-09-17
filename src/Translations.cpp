#include "Translations.h"

#include <nlohmann/json.hpp>
namespace {
    constexpr auto TRANSLATIONS_PATH = "Data/SKSE/Plugins/SKSEMenuFrameworkStrings.json";
    constexpr auto ENGLISH_TRANSLATIONS_PATH = "Data/SKSE/Plugins/SKSEMenuFrameworkStrings_EN.json";
    constexpr auto MISSING_TRANSLATION = "missing translation";

    using TranslationMap = std::map<std::string, std::string>;

    TranslationMap translations;
    TranslationMap englishTranslations;

    void LoadTranslations(const char* path, TranslationMap& destination) {
        destination.clear();

        std::ifstream file(path);
        if (!file.good()) {
            logger::error("Could not open translation file '{}'.", path);
            return;
        }

        try {
            const auto document = nlohmann::json::parse(file);
            if (!document.is_object()) {
                logger::warn("Translation file '{}' must contain a JSON object.", path);
                return;
            }

            for (const auto& [key, value] : document.items()) {
                if (!value.is_string()) {
                    logger::warn("Translation '{}' in '{}' must be a string.", key, path);
                    continue;
                }

                destination.emplace(key, value.get<std::string>());
            }
        } catch (const std::exception& exception) {
            logger::error("Could not read translation file '{}': {}", path, exception.what());
        }
    }
}


void Translations::Install() {
    LoadTranslations(ENGLISH_TRANSLATIONS_PATH, englishTranslations);
    LoadTranslations(TRANSLATIONS_PATH, translations);
}

const char* Translations::Get(std::string key) {
    if (const auto translation = translations.find(key); translation != translations.end()) {
        return translation->second.c_str();
    }

    if (const auto englishTranslation = englishTranslations.find(key);
        englishTranslation != englishTranslations.end()) {
        return englishTranslation->second.c_str();
    }

    logger::warn("Missing translation '{}'.", key);
    return MISSING_TRANSLATION;
}
