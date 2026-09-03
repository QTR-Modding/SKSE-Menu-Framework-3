#include "RootMenuConfig.h"

#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

namespace {
    constexpr auto CONFIG_PATH = "Data/SKSE/Plugins/SKSEMenuFrameworkMenuConfig.json";

    std::set<std::string> favoriteMenus;
    std::set<std::string> archivedMenus;

    void LoadMenuNames(const nlohmann::json& config, const char* key, std::set<std::string>& menuNames) {
        if (!config.contains(key) || !config[key].is_array()) {
            return;
        }

        for (const auto& value : config[key]) {
            if (value.is_string()) {
                menuNames.insert(value.get<std::string>());
            }
        }
    }

    void Save() {
        const nlohmann::json config = {
            {"favorites", favoriteMenus},
            {"archived", archivedMenus},
        };

        std::ofstream file(CONFIG_PATH, std::ios::trunc);
        if (!file.good()) {
            logger::error("Could not save root menu configuration to '{}'.", CONFIG_PATH);
            return;
        }

        file << config.dump(2) << '\n';
    }

    void SetMenuState(std::set<std::string>& menuNames, const std::string& menuName, bool enabled) {
        const bool changed = enabled ? menuNames.insert(menuName).second : menuNames.erase(menuName) > 0;
        if (changed) {
            Save();
        }
    }
}

void RootMenuConfig::Load() {
    favoriteMenus.clear();
    archivedMenus.clear();

    std::ifstream file(CONFIG_PATH);
    if (!file.good()) {
        return;
    }

    try {
        const auto config = nlohmann::json::parse(file);
        if (!config.is_object()) {
            logger::warn("Root menu configuration '{}' must contain a JSON object.", CONFIG_PATH);
            return;
        }

        LoadMenuNames(config, "favorites", favoriteMenus);
        LoadMenuNames(config, "archived", archivedMenus);
        LoadMenuNames(config, "hidden", archivedMenus);
    } catch (const std::exception& exception) {
        logger::error("Could not read root menu configuration '{}': {}", CONFIG_PATH, exception.what());
    }
}

bool RootMenuConfig::IsFavorite(const std::string& menuName) {
    return favoriteMenus.contains(menuName);
}

bool RootMenuConfig::IsArchived(const std::string& menuName) {
    return archivedMenus.contains(menuName);
}

void RootMenuConfig::SetFavorite(const std::string& menuName, bool favorite) {
    SetMenuState(favoriteMenus, menuName, favorite);
}

void RootMenuConfig::SetArchived(const std::string& menuName, bool archived) {
    SetMenuState(archivedMenus, menuName, archived);
}

void RootMenuConfig::RenameMenu(const std::string& oldName, const std::string& newName) {
    bool changed = false;
    if (favoriteMenus.erase(oldName) > 0) {
        favoriteMenus.insert(newName);
        changed = true;
    }
    if (archivedMenus.erase(oldName) > 0) {
        archivedMenus.insert(newName);
        changed = true;
    }
    if (changed) {
        Save();
    }
}

void RootMenuConfig::RemoveMenu(const std::string& menuName) {
    const bool favoriteRemoved = favoriteMenus.erase(menuName) > 0;
    const bool archivedRemoved = archivedMenus.erase(menuName) > 0;
    if (favoriteRemoved || archivedRemoved) {
        Save();
    }
}
