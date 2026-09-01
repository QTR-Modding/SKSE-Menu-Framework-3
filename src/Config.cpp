#include "Config.h"
#include "Application.h"
#include "Theme.h"
#include "Utils.h"
#include <algorithm>
#include <cmath>
unsigned int Config::ToggleKey = 0x3B;
uint8_t Config::ToggleMode = 0;
unsigned int Config::ToggleKeyGamePad = 0;
uint8_t Config::ToggleModeGamePad = 0;
int Config::DoublePressThreshold = 300;
bool Config::ConsumeToggleKey = false;
bool Config::FreezeTimeOnMenu = true;
int Config::MenuStyle = 0;
std::vector<std::string> Config::MenuStyles;
bool Config::BlurBackgroundOnMenu = true;
std::string Config::PrimaryFont = "CN.ttf";  
bool Config::EnableChinese = true;                                
bool Config::EnableJapanese = false;
bool Config::EnableKorean = false;
bool Config::EnableCyrillic = false;
bool Config::EnableThai = false;
bool Config::EnableTurkish = false;
float Config::FontSizeMedium = 32.0f;
float Config::MinFontSize = 12.0f;
float Config::MaxFontSize = 64.0f;


void Config::Init() {
	const auto ini = new Ini("SKSEMenuFramework.ini");
    ini->SetSection("General");

    ToggleKey = GetKeyBinding(ini->GetString("ToggleKey", "f1"));
    ToggleMode = GetToggleMode(ini->GetString("ToggleMode", "SinglePress"));

    ToggleKeyGamePad = GetKeyBinding(ini->GetString("ToggleKeyGamePad", ""),RE::INPUT_DEVICE::kGamepad);
    ToggleModeGamePad = GetToggleMode(ini->GetString("ToggleModeGamePad", "DoublePress"));
    DoublePressThreshold = ini->GetInt("DoublePressThreshold", 300);
    if (DoublePressThreshold < 50) DoublePressThreshold = 50;
    if (DoublePressThreshold > 2000) DoublePressThreshold = 2000;
    ConsumeToggleKey = ini->GetBool("ConsumeToggleKey", false);

    FreezeTimeOnMenu = ini->GetBool("FreezeTimeOnMenu", true);
    BlurBackgroundOnMenu = ini->GetBool("BlurBackgroundOnMenu", true);
    auto menuStyleStr = Utils::toUpperCase(ini->GetString("MenuStyle", "skyrimDefault"));

    MenuStyles = Theme::GetJsonFiles();
    Config::MenuStyle = Utils::indexOf(Config::MenuStyles, Utils::toUpperCase(menuStyleStr));

    ini->SetSection("Fonts");  
    PrimaryFont = ini->GetString("PrimaryFont", "MainFont.ttf");
    EnableChinese = ini->GetBool("EnableChinese", true);
    EnableJapanese = ini->GetBool("EnableJapanese", false);
    EnableKorean = ini->GetBool("EnableKorean", false);
    EnableCyrillic = ini->GetBool("EnableCyrillic", false);
    EnableThai = ini->GetBool("EnableThai", false);
    EnableTurkish = ini->GetBool("EnableTurkish", false);
    MinFontSize = ini->GetFloat("MinFontSize", 12.0f);
    MaxFontSize = ini->GetFloat("MaxFontSize", 64.0f);
    if (!std::isfinite(MinFontSize)) {
        MinFontSize = 12.0f;
    }
    if (!std::isfinite(MaxFontSize)) {
        MaxFontSize = 64.0f;
    }
    if (MinFontSize > MaxFontSize) {
        std::swap(MinFontSize, MaxFontSize);
    }
    FontSizeMedium = NormalizeFontSize(ini->GetFloat("FontSizeMedium", 32.0f));



    delete ini;
    delete[] menuStyleStr;
}

void Config::Save() {
    const auto ini = new Ini("SKSEMenuFramework.ini");

    // General Section
    ini->SetSection("General");
    // Note: You'll need helper functions to convert key bindings and toggle modes back to strings
    // For now, these are placeholders - implement KeyBindingToString() and ToggleModeToString()
    // ini->SetString("ToggleKey", KeyBindingToString(ToggleKey).c_str());
    // ini->SetString("ToggleMode", ToggleModeToString(ToggleMode).c_str());
    // ini->SetString("ToggleKeyGamePad", KeyBindingToString(ToggleKeyGamePad).c_str());
    // ini->SetString("ToggleModeGamePad", ToggleModeToString(ToggleModeGamePad).c_str());

    ini->SetBool("FreezeTimeOnMenu", FreezeTimeOnMenu);
    ini->SetBool("BlurBackgroundOnMenu", BlurBackgroundOnMenu);


    if (ToggleMode == 0) {
        ini->SetString("ToggleMode", "SINGLEPRESS");
    }
    else if (ToggleMode == 1) {
        ini->SetString("ToggleMode", "HOLD");
    }
    else if (ToggleMode == 2) {
        ini->SetString("ToggleMode", "DOUBLEPRESS");
    } else if (ToggleMode == 3) {
        ini->SetString("ToggleMode", "OFF");
    }

    if (ToggleModeGamePad == 0) {
        ini->SetString("ToggleModeGamePad", "SINGLEPRESS");
    } else if (ToggleModeGamePad == 1) {
        ini->SetString("ToggleModeGamePad", "HOLD");
    } else if (ToggleModeGamePad == 2) {
        ini->SetString("ToggleModeGamePad", "DOUBLEPRESS");
    } else if (ToggleModeGamePad == 3) {
        ini->SetString("ToggleModeGamePad", "OFF");
    }

    ini->SetString("ToggleKey", GetKeyName(ToggleKey, RE::INPUT_DEVICE::kKeyboard).c_str());
    ini->SetString("ToggleKeyGamePad", GetKeyName(ToggleKeyGamePad, RE::INPUT_DEVICE::kGamepad).c_str());


    ini->SetString("MenuStyle", MenuStyles[MenuStyle].c_str());

    // Fonts Section
    ini->SetSection("Fonts");
    ini->SetString("PrimaryFont", PrimaryFont.c_str());
    ini->SetBool("EnableChinese", EnableChinese);
    ini->SetBool("EnableJapanese", EnableJapanese);
    ini->SetBool("EnableKorean", EnableKorean);
    ini->SetBool("EnableCyrillic", EnableCyrillic);
    ini->SetBool("EnableThai", EnableThai);
    ini->SetBool("EnableTurkish", EnableTurkish);

    ini->SetFloat("MinFontSize", MinFontSize);
    ini->SetFloat("MaxFontSize", MaxFontSize);
    ini->SetFloat("FontSizeMedium", FontSizeMedium);

    // Save to file
    if (!ini->Save()) {
        // Log error if save fails
        // logger::error("Failed to save configuration to INI file");
    }

    delete ini;
}

void Config::LoadStyle() {
    
    Theme::LoadJsonStyle(MenuStyles[MenuStyle]);

}

float Config::NormalizeFontSize(float size) {
    if (!std::isfinite(size)) {
        return 32.0f;
    }
    return std::clamp(size, MinFontSize, MaxFontSize);
}
