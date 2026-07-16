#pragma once

#include <string>

namespace RootMenuConfig {
    void Load();
    bool IsFavorite(const std::string& menuName);
    bool IsArchived(const std::string& menuName);
    void SetFavorite(const std::string& menuName, bool favorite);
    void SetArchived(const std::string& menuName, bool archived);
}
