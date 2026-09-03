#include "MenuPath.h"

#include <utility>

std::optional<MenuPath::Segments> MenuPath::Parse(std::string_view path) {
    if (path.empty()) {
        return std::nullopt;
    }

    Segments segments;
    std::string segment;
    segment.reserve(path.size());

    for (std::size_t index = 0; index < path.size(); ++index) {
        const char character = path[index];
        if (character == '\\' && index + 1 < path.size() && path[index + 1] == '/') {
            segment.push_back('/');
            ++index;
            continue;
        }

        if (character == '/') {
            if (segment.empty()) {
                return std::nullopt;
            }
            segments.push_back(std::move(segment));
            segment.clear();
            continue;
        }

        segment.push_back(character);
    }

    if (segment.empty()) {
        return std::nullopt;
    }

    segments.push_back(std::move(segment));
    return segments;
}

std::optional<std::string> MenuPath::ParseSegment(std::string_view segment) {
    auto parsed = Parse(segment);
    if (!parsed || parsed->size() != 1) {
        return std::nullopt;
    }
    return std::move(parsed->front());
}
