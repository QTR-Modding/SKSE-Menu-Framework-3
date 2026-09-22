#include "FontConfigReader.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <stdexcept>
#include <utility>

namespace FontConfigReader {
    namespace {
        std::string ToLower(std::string value) {
            std::ranges::transform(value, value.begin(),
                                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return value;
        }

        void SkipWhitespace(std::string_view& text) {
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
                text.remove_prefix(1);
            }
        }

        std::string_view ReadToken(std::string_view& text) {
            SkipWhitespace(text);
            if (text.empty() || text.starts_with("//") || text.front() == '#' || text.front() == ';') {
                return {};
            }
            if (text.front() == '"') {
                text.remove_prefix(1);
                const auto end = text.find('"');
                if (end == std::string_view::npos) {
                    throw std::runtime_error("unterminated quoted string");
                }
                const auto token = text.substr(0, end);
                text.remove_prefix(end + 1);
                return token;
            }
            const auto end = text.front() == '=' ? 1 : text.find_first_of(" \t\r\n=,;#");
            const auto token = text.substr(0, end);
            if (token.empty()) {
                throw std::runtime_error("unexpected punctuation");
            }
            text.remove_prefix(token.size());
            return token;
        }

        std::string ReadLibrary(std::string_view text) {
            auto path = std::string(ReadToken(text));
            std::ranges::replace(path, '\\', '/');
            if (!ToLower(path).starts_with("interface/")) {
                throw std::runtime_error("fontlib path must be relative to Interface");
            }
            path.erase(0, sizeof("Interface/") - 1);
            if (!ToLower(path).ends_with(".swf") || path.find(':') != std::string::npos) {
                throw std::runtime_error("fontlib must name a SWF resource");
            }
            std::string_view remaining = path;
            while (!remaining.empty()) {
                const auto end = remaining.find('/');
                const auto part = remaining.substr(0, end);
                if (part.empty() || part == "." || part == "..") {
                    throw std::runtime_error("invalid fontlib path component");
                }
                if (end == std::string_view::npos) break;
                remaining.remove_prefix(end + 1);
            }
            return path;
        }

        FontMapping ReadMapping(std::string_view text) {
            if (ReadToken(text) != "=") {
                throw std::runtime_error("expected '=' after $StartMenuFont");
            }
            FontMapping mapping;
            mapping.name = ReadToken(text);
            if (mapping.name.empty()) {
                throw std::runtime_error("$StartMenuFont has no target face");
            }
            bool normal = false;
            while (true) {
                SkipWhitespace(text);
                if (!text.empty() && text.front() == ',') text.remove_prefix(1);
                const auto token = ReadToken(text);
                if (token.empty()) break;
                const auto style = ToLower(std::string(token));
                if (style == "normal") {
                    normal = true;
                } else if (style == "bold") {
                    mapping.bold = true;
                } else if (style == "italic") {
                    mapping.italic = true;
                } else if (style == "bolditalic" || style == "bold italic") {
                    mapping.bold = mapping.italic = true;
                } else {
                    throw std::runtime_error(std::format("unsupported $StartMenuFont style '{}'", token));
                }
            }
            if (normal && (mapping.bold || mapping.italic)) {
                throw std::runtime_error("$StartMenuFont combines Normal with Bold or Italic");
            }
            return mapping;
        }
    }

    FontConfig Parse(std::string_view text) {
        if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
        if (text.find('\0') != std::string_view::npos) {
            throw std::runtime_error("font configuration must be UTF-8 text");
        }

        FontConfig config;
        std::size_t lineNumber = 0;
        while (!text.empty()) {
            const auto end = text.find('\n');
            auto line = text.substr(0, end);
            text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
            ++lineNumber;
            try {
                const auto command = ToLower(std::string(ReadToken(line)));
                if (command == "fontlib") {
                    auto library = ReadLibrary(line);
                    const auto normalized = ToLower(library);
                    if (std::ranges::none_of(config.libraries,
                                             [&](const auto& existing) { return ToLower(existing) == normalized; })) {
                        config.libraries.push_back(std::move(library));
                    }
                } else if (command == "map" && ReadToken(line) == "$StartMenuFont") {
                    config.menuFont = ReadMapping(line);
                }
            } catch (const std::exception& exception) {
                throw std::runtime_error(std::format("line {}: {}", lineNumber, exception.what()));
            }
        }
        if (config.libraries.empty() || config.menuFont.name.empty()) {
            throw std::runtime_error("font configuration requires fontlib and $StartMenuFont declarations");
        }
        return config;
    }
}
