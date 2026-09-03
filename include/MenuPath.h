#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MenuPath {
    using Segments = std::vector<std::string>;

    std::optional<Segments> Parse(std::string_view path);
    std::optional<std::string> ParseSegment(std::string_view segment);
}
