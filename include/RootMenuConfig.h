#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace RootMenuConfig {
    void Load();
    bool IsFavorite(const std::string& menuName);
    bool IsArchived(const std::string& menuName);
    void SetFavorite(const std::string& menuName, bool favorite);
    void SetArchived(const std::string& menuName, bool archived);
    std::set<std::string> GetCategories();
    // An empty category name represents Uncategorized.
    std::string GetCategory(const std::string& menuName);
    std::string NormalizeCategoryName(std::string_view name);
    bool IsValidCategoryName(const std::string& name);
    // A supplied menu name is assigned in the same saved change as creation.
    bool CreateCategory(const std::string& name, const std::string& menuName = {});
    bool RenameCategory(const std::string& oldName, const std::string& newName);
    bool RemoveCategory(const std::string& name);
    bool SetCategory(const std::string& menuName, const std::string& categoryName);
    std::vector<std::string> GetMenuOrder();
    // Saves category assignment and root menu order together; failure leaves both unchanged.
    bool SetMenuPlacement(const std::string& menuName, const std::string& categoryName,
                          const std::vector<std::string>& orderedMenus);
    void RenameMenu(const std::string& oldName, const std::string& newName);
    void RemoveMenu(const std::string& menuName);
}
