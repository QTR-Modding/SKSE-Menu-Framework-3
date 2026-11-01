#pragma once

class Config {
public:
    enum class GlyphLanguage : uint8_t {
        Default,
        Chinese,
        Japanese,
        Korean,
        Cyrillic,
        Thai,
        Turkish,
        Polish,
        Vietnamese,
        Custom
    };

    struct GlyphRangeSelection {
        bool Chinese = false;
        bool Japanese = false;
        bool Korean = false;
        bool Cyrillic = false;
        bool Thai = false;
        bool Turkish = false;
        bool Polish = false;
        bool Vietnamese = false;

        bool operator==(const GlyphRangeSelection&) const = default;
    };

    static float MinFontSize;
    static float MaxFontSize;

    static void Init();
    static void Save();
    static unsigned int ToggleKey;
    static uint8_t ToggleMode;
    static unsigned int ToggleKeyGamePad;
    static uint8_t ToggleModeGamePad;
    static int DoublePressThreshold;
    static bool ConsumeToggleKey;
    static bool FreezeTimeOnMenu;
    static bool BlurBackgroundOnMenu;
    static int MenuStyle;
    static std::vector<std::string> MenuStyles;
    static std::string PrimaryFont;
    static bool EnableChinese;
    static bool EnableJapanese;
    static bool EnableKorean;
    static bool EnableCyrillic;
    static bool EnableThai;
    static bool EnableTurkish;
    static bool EnablePolish;
    static bool EnableVietnamese;
    static float FontSizeMedium;
    static GlyphLanguage GetGlyphLanguage();
    static void SetGlyphLanguage(GlyphLanguage language);
    static GlyphRangeSelection GetGlyphRangeSelection();
    static void SetGlyphRangeSelection(const GlyphRangeSelection& selection);
    static float NormalizeFontSize(float size);
    static void LoadStyle();
};
