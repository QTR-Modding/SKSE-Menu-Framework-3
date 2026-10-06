#include "RootMenuConfig.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace {
    constexpr auto CONFIG_PATH = "Data/SKSE/Plugins/SKSEMenuFrameworkMenuConfig.json";
    constexpr std::size_t MAX_CATEGORY_NAME_LENGTH = 255;

    std::set<std::string> favoriteMenus;
    std::set<std::string> archivedMenus;
    std::set<std::string> categories;
    std::map<std::string, std::string> menuCategories;
    std::vector<std::string> menuOrder;

    std::optional<std::string> ValidateCategoryName(std::string_view name) {
        auto normalizedName = RootMenuConfig::NormalizeCategoryName(name);
        if (normalizedName.empty() || normalizedName.size() > MAX_CATEGORY_NAME_LENGTH) {
            return std::nullopt;
        }

        if (std::any_of(normalizedName.begin(), normalizedName.end(), [](unsigned char character) {
                return character < 0x20 || character == 0x7f;
            })) {
            return std::nullopt;
        }
        return normalizedName;
    }

    void LoadMenuNames(const nlohmann::json& config, const char* key, std::set<std::string>& menuNames) {
        if (!config.contains(key)) {
            return;
        }
        if (!config[key].is_array()) {
            logger::warn("Ignoring root menu configuration field '{}': expected an array.", key);
            return;
        }

        for (const auto& value : config[key]) {
            if (value.is_string()) {
                menuNames.insert(value.get<std::string>());
            } else {
                logger::warn("Ignoring non-string entry in root menu configuration field '{}'.", key);
            }
        }
    }

    void LoadCategories(const nlohmann::json& config) {
        if (!config.contains("categories")) {
            return;
        }
        if (!config["categories"].is_array()) {
            logger::warn("Ignoring root menu configuration field 'categories': expected an array.");
            return;
        }

        for (const auto& value : config["categories"]) {
            if (!value.is_string()) {
                logger::warn("Ignoring non-string category in root menu configuration.");
                continue;
            }

            const auto categoryName = ValidateCategoryName(value.get<std::string>());
            if (!categoryName) {
                logger::warn("Ignoring invalid category name in root menu configuration: {}.", value.dump());
                continue;
            }
            if (!categories.insert(*categoryName).second) {
                logger::warn("Ignoring duplicate category '{}' in root menu configuration.", *categoryName);
            }
        }
    }

    void LoadMenuCategories(const nlohmann::json& config) {
        if (!config.contains("menuCategories")) {
            return;
        }
        if (!config["menuCategories"].is_object()) {
            logger::warn("Ignoring root menu configuration field 'menuCategories': expected an object.");
            return;
        }

        for (const auto& [menuName, value] : config["menuCategories"].items()) {
            if (menuName.empty() || !value.is_string()) {
                logger::warn("Ignoring invalid category assignment for menu '{}' in root menu configuration.", menuName);
                continue;
            }

            const auto categoryName = ValidateCategoryName(value.get<std::string>());
            if (!categoryName || !categories.contains(*categoryName)) {
                logger::warn("Ignoring invalid or unknown category assignment for menu '{}' in root menu configuration: {}.",
                    menuName, value.dump());
                continue;
            }
            menuCategories.emplace(menuName, *categoryName);
        }
    }

    void LoadMenuOrder(const nlohmann::json& config) {
        if (!config.contains("menuOrder")) {
            return;
        }
        if (!config["menuOrder"].is_array()) {
            logger::warn("Ignoring root menu configuration field 'menuOrder': expected an array.");
            return;
        }

        std::set<std::string> seenMenus;
        for (const auto& value : config["menuOrder"]) {
            if (!value.is_string() || value.get_ref<const std::string&>().empty()) {
                logger::warn("Ignoring invalid menu name in root menu configuration field 'menuOrder': {}.", value.dump());
                continue;
            }

            const auto menuName = value.get<std::string>();
            if (!seenMenus.insert(menuName).second) {
                logger::warn("Ignoring duplicate menu '{}' in root menu configuration field 'menuOrder'.", menuName);
                continue;
            }
            menuOrder.push_back(menuName);
        }
    }

    void RemoveTemporaryFile(const wchar_t* path) {
        if (path[0] == L'\0' || DeleteFileW(path)) {
            return;
        }

        const auto error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) {
            logger::warn("Could not remove temporary root menu configuration file (Windows error {}).", error);
        }
    }

    bool Save(const std::set<std::string>& savedCategories,
              const std::map<std::string, std::string>& savedMenuCategories,
              const std::vector<std::string>& savedMenuOrder) {
        std::array<wchar_t, MAX_PATH> temporaryPath{};
        try {
            const nlohmann::json config = {
                {"favorites", favoriteMenus},
                {"archived", archivedMenus},
                {"categories", savedCategories},
                {"menuCategories", savedMenuCategories},
                {"menuOrder", savedMenuOrder},
            };
            const auto contents = config.dump(2) + '\n';
            const std::filesystem::path configPath(CONFIG_PATH);
            if (!GetTempFileNameW(configPath.parent_path().c_str(), L"SMF", 0, temporaryPath.data())) {
                logger::error("Could not create a temporary root menu configuration file for '{}' (Windows error {}).",
                    CONFIG_PATH, GetLastError());
                return false;
            }

            std::ofstream file(std::filesystem::path(temporaryPath.data()), std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                logger::error("Could not open the temporary root menu configuration file for '{}'.", CONFIG_PATH);
                RemoveTemporaryFile(temporaryPath.data());
                return false;
            }

            file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
            file.flush();
            const bool writeSucceeded = file.good();
            file.close();
            if (!writeSucceeded || file.fail()) {
                logger::error("Could not write the temporary root menu configuration file for '{}'.", CONFIG_PATH);
                RemoveTemporaryFile(temporaryPath.data());
                return false;
            }

            if (!MoveFileExW(temporaryPath.data(), configPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                logger::error("Could not replace root menu configuration '{}' (Windows error {}).", CONFIG_PATH, GetLastError());
                RemoveTemporaryFile(temporaryPath.data());
                return false;
            }
            return true;
        } catch (const std::exception& exception) {
            logger::error("Could not save root menu configuration '{}': {}", CONFIG_PATH, exception.what());
            RemoveTemporaryFile(temporaryPath.data());
            return false;
        }
    }

    bool Save() {
        return Save(categories, menuCategories, menuOrder);
    }

    void SetMenuState(std::set<std::string>& menuNames, const std::string& menuName, bool enabled) {
        const bool changed = enabled ? menuNames.insert(menuName).second : menuNames.erase(menuName) > 0;
        if (changed && !Save()) {
            if (enabled) {
                menuNames.erase(menuName);
            } else {
                menuNames.insert(menuName);
            }
        }
    }
}

void RootMenuConfig::Load() {
    favoriteMenus.clear();
    archivedMenus.clear();
    categories.clear();
    menuCategories.clear();
    menuOrder.clear();

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
        LoadCategories(config);
        LoadMenuCategories(config);
        LoadMenuOrder(config);
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

std::set<std::string> RootMenuConfig::GetCategories() {
    return categories;
}

std::string RootMenuConfig::GetCategory(const std::string& menuName) {
    const auto assignment = menuCategories.find(menuName);
    return assignment == menuCategories.end() ? std::string{} : assignment->second;
}

std::string RootMenuConfig::NormalizeCategoryName(std::string_view name) {
    constexpr std::string_view whitespace = " \t\n\r\f\v";
    const auto first = name.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }

    const auto last = name.find_last_not_of(whitespace);
    return std::string(name.substr(first, last - first + 1));
}

bool RootMenuConfig::IsValidCategoryName(const std::string& name) {
    return ValidateCategoryName(name).has_value();
}

bool RootMenuConfig::CreateCategory(const std::string& name, const std::string& menuName) {
    const auto categoryName = ValidateCategoryName(name);
    if (!categoryName || categories.contains(*categoryName)) {
        return false;
    }

    auto updatedCategories = categories;
    auto updatedMenuCategories = menuCategories;
    updatedCategories.insert(*categoryName);
    if (!menuName.empty()) {
        updatedMenuCategories[menuName] = *categoryName;
    }
    if (!Save(updatedCategories, updatedMenuCategories, menuOrder)) {
        return false;
    }
    categories.swap(updatedCategories);
    menuCategories.swap(updatedMenuCategories);
    return true;
}

bool RootMenuConfig::RenameCategory(const std::string& oldName, const std::string& newName) {
    const auto oldCategoryName = ValidateCategoryName(oldName);
    const auto newCategoryName = ValidateCategoryName(newName);
    if (!oldCategoryName || !newCategoryName || !categories.contains(*oldCategoryName)) {
        return false;
    }
    if (*oldCategoryName == *newCategoryName) {
        return true;
    }
    if (categories.contains(*newCategoryName)) {
        return false;
    }

    auto updatedCategories = categories;
    auto updatedMenuCategories = menuCategories;
    updatedCategories.erase(*oldCategoryName);
    updatedCategories.insert(*newCategoryName);
    for (auto& assignment : updatedMenuCategories) {
        if (assignment.second == *oldCategoryName) {
            assignment.second = *newCategoryName;
        }
    }
    if (!Save(updatedCategories, updatedMenuCategories, menuOrder)) {
        return false;
    }
    categories.swap(updatedCategories);
    menuCategories.swap(updatedMenuCategories);
    return true;
}

bool RootMenuConfig::RemoveCategory(const std::string& name) {
    if (!categories.contains(name)) {
        return false;
    }

    auto updatedCategories = categories;
    auto updatedMenuCategories = menuCategories;
    updatedCategories.erase(name);
    std::erase_if(updatedMenuCategories, [&name](const auto& assignment) {
        return assignment.second == name;
    });
    if (!Save(updatedCategories, updatedMenuCategories, menuOrder)) {
        return false;
    }
    categories.swap(updatedCategories);
    menuCategories.swap(updatedMenuCategories);
    return true;
}

bool RootMenuConfig::SetCategory(const std::string& menuName, const std::string& categoryName) {
    if (menuName.empty()) {
        return false;
    }

    std::string normalizedCategory;
    if (!categoryName.empty()) {
        const auto normalizedName = ValidateCategoryName(categoryName);
        if (!normalizedName || !categories.contains(*normalizedName)) {
            return false;
        }
        normalizedCategory = *normalizedName;
    }
    if (GetCategory(menuName) == normalizedCategory) {
        return true;
    }

    auto updatedMenuCategories = menuCategories;
    if (normalizedCategory.empty()) {
        updatedMenuCategories.erase(menuName);
    } else {
        updatedMenuCategories[menuName] = normalizedCategory;
    }
    if (!Save(categories, updatedMenuCategories, menuOrder)) {
        return false;
    }
    menuCategories.swap(updatedMenuCategories);
    return true;
}

std::vector<std::string> RootMenuConfig::GetMenuOrder() {
    return menuOrder;
}

bool RootMenuConfig::SetMenuPlacement(const std::string& menuName, const std::string& categoryName,
                                    const std::vector<std::string>& orderedMenus) {
    if (menuName.empty()) {
        return false;
    }

    std::set<std::string> seenMenus;
    for (const auto& name : orderedMenus) {
        if (name.empty() || !seenMenus.insert(name).second) {
            return false;
        }
    }
    if (!seenMenus.contains(menuName)) {
        return false;
    }

    std::string normalizedCategory;
    if (!categoryName.empty()) {
        const auto normalizedName = ValidateCategoryName(categoryName);
        if (!normalizedName || !categories.contains(*normalizedName)) {
            return false;
        }
        normalizedCategory = *normalizedName;
    }
    if (GetCategory(menuName) == normalizedCategory && menuOrder == orderedMenus) {
        return true;
    }

    auto updatedMenuCategories = menuCategories;
    if (normalizedCategory.empty()) {
        updatedMenuCategories.erase(menuName);
    } else {
        updatedMenuCategories[menuName] = normalizedCategory;
    }
    auto updatedMenuOrder = orderedMenus;
    if (!Save(categories, updatedMenuCategories, updatedMenuOrder)) {
        return false;
    }
    menuCategories.swap(updatedMenuCategories);
    menuOrder.swap(updatedMenuOrder);
    return true;
}

void RootMenuConfig::RenameMenu(const std::string& oldName, const std::string& newName) {
    if (oldName == newName) {
        return;
    }

    bool changed = false;
    if (favoriteMenus.erase(oldName) > 0) {
        favoriteMenus.insert(newName);
        changed = true;
    }
    if (archivedMenus.erase(oldName) > 0) {
        archivedMenus.insert(newName);
        changed = true;
    }
    const auto assignment = menuCategories.find(oldName);
    if (assignment != menuCategories.end()) {
        menuCategories[newName] = std::move(assignment->second);
        menuCategories.erase(assignment);
        changed = true;
    }
    if (std::find(menuOrder.begin(), menuOrder.end(), oldName) != menuOrder.end()) {
        std::erase(menuOrder, newName);
        std::replace(menuOrder.begin(), menuOrder.end(), oldName, newName);
        changed = true;
    }
    if (changed) {
        Save();
    }
}

void RootMenuConfig::RemoveMenu(const std::string& menuName) {
    const bool favoriteRemoved = favoriteMenus.erase(menuName) > 0;
    const bool archivedRemoved = archivedMenus.erase(menuName) > 0;
    const bool categoryRemoved = menuCategories.erase(menuName) > 0;
    const bool orderRemoved = std::erase(menuOrder, menuName) > 0;
    if (favoriteRemoved || archivedRemoved || categoryRemoved || orderRemoved) {
        Save();
    }
}
