#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace FontConfigReader {
    struct FontMapping {
        std::string name;
        bool bold = false;
        bool italic = false;
    };

    struct FontConfig {
        // Library paths are relative to Interface, including any subdirectories.
        std::vector<std::string> libraries;
        FontMapping menuFont;
    };

    // Reads fontlib declarations and $StartMenuFont. Throws std::runtime_error
    // for malformed declarations, unsupported styles, or a missing mapping.
    FontConfig Parse(std::string_view text);
}
