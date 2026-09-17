#include "SwfFontReader.h"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

#include "PCH.h"

// The SWF outline conversion follows the DefineFont2/DefineFont3 and quadratic
// contour behavior used by JPEXS Free Flash Decompiler's FontExporter. This is
// a native, in-memory implementation tailored to the font tags used by Skyrim.

namespace SwfFontReader {
    namespace {
        constexpr std::uint16_t DEFINE_FONT_2 = 48;
        constexpr std::uint16_t DEFINE_FONT_3 = 75;
        constexpr std::uint32_t MAX_SWF_SIZE = 256 * 1024 * 1024;
        constexpr std::int32_t TRUE_TYPE_UNITS_PER_EM = 1024;

        class ParseError final : public std::runtime_error {
        public:
            explicit ParseError(const std::string& message) : std::runtime_error(message) {}
        };

        class ByteReader {
        public:
            explicit ByteReader(std::span<const std::uint8_t> data) : data_(data) {}

            [[nodiscard]] std::size_t Position() const { return position_; }
            [[nodiscard]] std::size_t Remaining() const { return data_.size() - position_; }

            void Seek(std::size_t position) {
                if (position > data_.size()) {
                    throw ParseError("seek exceeds input");
                }
                position_ = position;
            }

            void Skip(std::size_t count) {
                Require(count);
                position_ += count;
            }

            std::uint8_t ReadU8() {
                Require(1);
                return data_[position_++];
            }

            std::uint16_t ReadU16() {
                Require(2);
                const auto value = static_cast<std::uint16_t>(data_[position_]) |
                                   static_cast<std::uint16_t>(data_[position_ + 1] << 8);
                position_ += 2;
                return value;
            }

            std::int16_t ReadS16() { return static_cast<std::int16_t>(ReadU16()); }

            std::uint32_t ReadU32() {
                Require(4);
                const auto value = static_cast<std::uint32_t>(data_[position_]) |
                                   (static_cast<std::uint32_t>(data_[position_ + 1]) << 8) |
                                   (static_cast<std::uint32_t>(data_[position_ + 2]) << 16) |
                                   (static_cast<std::uint32_t>(data_[position_ + 3]) << 24);
                position_ += 4;
                return value;
            }

            std::span<const std::uint8_t> ReadBytes(std::size_t count) {
                Require(count);
                const auto result = data_.subspan(position_, count);
                position_ += count;
                return result;
            }

        private:
            void Require(std::size_t count) const {
                if (count > Remaining()) {
                    throw ParseError("unexpected end of input");
                }
            }

            std::span<const std::uint8_t> data_;
            std::size_t position_ = 0;
        };

        class BitReader {
        public:
            explicit BitReader(std::span<const std::uint8_t> data) : data_(data) {}

            std::uint32_t ReadUnsigned(std::uint8_t bitCount) {
                if (bitCount > 32 || bitPosition_ + bitCount > data_.size() * 8) {
                    throw ParseError("bit field exceeds input");
                }

                std::uint32_t value = 0;
                for (std::uint8_t index = 0; index < bitCount; ++index) {
                    const auto byteIndex = bitPosition_ / 8;
                    const auto bitIndex = 7 - (bitPosition_ % 8);
                    value = (value << 1) | ((data_[byteIndex] >> bitIndex) & 1);
                    ++bitPosition_;
                }
                return value;
            }

            std::int32_t ReadSigned(std::uint8_t bitCount) {
                if (bitCount == 0) {
                    return 0;
                }

                const auto value = ReadUnsigned(bitCount);
                const auto signBit = std::uint32_t{1} << (bitCount - 1);
                if ((value & signBit) == 0) {
                    return static_cast<std::int32_t>(value);
                }
                return static_cast<std::int32_t>(value | (~std::uint32_t{0} << bitCount));
            }

            void Align() { bitPosition_ = (bitPosition_ + 7) & ~std::size_t{7}; }
            [[nodiscard]] std::size_t BytePosition() const { return (bitPosition_ + 7) / 8; }

        private:
            std::span<const std::uint8_t> data_;
            std::size_t bitPosition_ = 0;
        };

        struct Point {
            std::int16_t x = 0;
            std::int16_t y = 0;
            bool onCurve = true;

            bool operator==(const Point&) const = default;
        };

        struct Edge {
            std::int32_t fromX = 0;
            std::int32_t fromY = 0;
            std::int32_t controlX = 0;
            std::int32_t controlY = 0;
            std::int32_t toX = 0;
            std::int32_t toY = 0;
            bool curved = false;

            [[nodiscard]] Edge Reversed() const { return {toX, toY, controlX, controlY, fromX, fromY, curved}; }
        };

        struct Glyph {
            std::uint16_t code = 0;
            std::uint16_t advance = 0;
            std::vector<std::vector<Point>> contours;
            std::int16_t xMin = 0;
            std::int16_t yMin = 0;
            std::int16_t xMax = 0;
            std::int16_t yMax = 0;
        };

        struct ParsedFont {
            std::uint16_t id = 0;
            std::string tagName;
            std::string fullName;
            std::string sourceName;
            std::vector<Glyph> glyphs;
            std::int16_t ascent = 0;
            std::int16_t descent = 0;
            std::int16_t leading = 0;
            SupplementalGlyphLanguage supplementalLanguage = SupplementalGlyphLanguage::None;
            bool bold = false;
            bool italic = false;
        };

        std::string ToLower(std::string value) {
            std::ranges::transform(value, value.begin(),
                                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return value;
        }

        std::string TrimNulls(std::string value) {
            while (!value.empty() && value.back() == '\0') {
                value.pop_back();
            }
            return value;
        }

        bool IsRegularFuturaCondensed(const std::string& name, bool bold, bool italic) {
            if (bold || italic) {
                return false;
            }
            const auto normalized = ToLower(name);
            return normalized == "futura condensed" || normalized == "futura condensed test";
        }

        SupplementalGlyphLanguage GetSupplementalGlyphLanguage(const std::string& sourceName,
                                                               const std::string& fontName, bool bold, bool italic) {
            if (bold || italic) {
                return SupplementalGlyphLanguage::None;
            }

            const auto normalizedSource = ToLower(std::filesystem::path(sourceName).filename().string());
            const auto normalizedName = ToLower(fontName);
            if (normalizedSource == "fonts_cn.swf" && normalizedName == "dfming-b5 otf w9") {
                return SupplementalGlyphLanguage::Chinese;
            }
            if (normalizedSource == "fonts_ja.swf" && normalizedName == "1_skyrim_jp_everyfont_0805") {
                return SupplementalGlyphLanguage::Japanese;
            }
            return SupplementalGlyphLanguage::None;
        }

        std::string GetLanguageName(const std::string& sourceName) {
            auto name = ToLower(std::filesystem::path(sourceName).stem().string());
            if (name.starts_with("fonts_")) {
                name.erase(0, 6);
            }
            std::ranges::transform(name, name.begin(),
                                   [](unsigned char character) { return static_cast<char>(std::toupper(character)); });
            return name;
        }

        std::size_t ReadRectSize(std::span<const std::uint8_t> data) {
            BitReader bits(data);
            const auto coordinateBits = static_cast<std::uint8_t>(bits.ReadUnsigned(5));
            for (int index = 0; index < 4; ++index) {
                bits.ReadSigned(coordinateBits);
            }
            bits.Align();
            return bits.BytePosition();
        }

        std::optional<std::vector<std::uint8_t>> ReadGameResource(const std::string& filename) {
            const auto resourcePath = std::format("Interface\\{}", filename);
            RE::BSResourceNiBinaryStream stream(resourcePath);
            if (!stream.good() || !stream.stream) {
                return std::nullopt;
            }

            const auto size = static_cast<std::size_t>(stream.stream->totalSize);
            if (size < 8 || size > MAX_SWF_SIZE) {
                logger::warn("SWF font reader: '{}' has invalid size {}.", resourcePath, size);
                return std::nullopt;
            }

            std::vector<std::uint8_t> data(size);
            std::uint64_t bytesRead = 0;
            stream.stream->DoRead(data.data(), data.size(), bytesRead);
            if (bytesRead != data.size()) {
                logger::warn("SWF font reader: Read {} of {} bytes from '{}'.", bytesRead, data.size(), resourcePath);
                return std::nullopt;
            }
            return data;
        }

        std::vector<std::uint8_t> DecompressSwf(std::span<const std::uint8_t> file, const std::string& sourceName) {
            if (file.size() < 8 || file[1] != 'W' || file[2] != 'S') {
                throw ParseError("invalid SWF signature");
            }

            ByteReader header(file.subspan(4));
            const auto declaredSize = header.ReadU32();
            if (declaredSize < 8 || declaredSize > MAX_SWF_SIZE) {
                throw ParseError("invalid uncompressed SWF size");
            }

            if (file[0] == 'F') {
                if (file.size() < declaredSize) {
                    throw ParseError("truncated uncompressed SWF");
                }
                return {file.begin() + 8, file.begin() + declaredSize};
            }
            if (file[0] != 'C') {
                throw ParseError("unsupported SWF compression (only FWS and CWS are supported)");
            }

            constexpr std::size_t MINIMUM_OUTPUT_GROWTH = 64 * 1024;
            constexpr std::size_t MAX_SWF_BODY_SIZE = MAX_SWF_SIZE - 8;

            std::vector<std::uint8_t> result(declaredSize - 8);
            z_stream stream{};
            stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(file.data() + 8));
            stream.avail_in = static_cast<uInt>(file.size() - 8);

            const auto initializationStatus = inflateInit(&stream);
            if (initializationStatus != Z_OK) {
                throw ParseError(std::format("zlib initialization failed ({})", initializationStatus));
            }

            int status = Z_OK;
            try {
                while (status != Z_STREAM_END) {
                    if (stream.total_out == result.size()) {
                        if (result.size() >= MAX_SWF_BODY_SIZE) {
                            throw ParseError("decompressed SWF exceeds maximum size");
                        }

                        const auto growth = std::max(result.size(), MINIMUM_OUTPUT_GROWTH);
                        const auto expandedSize = std::min(result.size() + growth, MAX_SWF_BODY_SIZE);
                        result.resize(expandedSize);
                    }

                    stream.next_out = reinterpret_cast<Bytef*>(result.data() + stream.total_out);
                    stream.avail_out = static_cast<uInt>(result.size() - stream.total_out);
                    status = inflate(&stream, Z_NO_FLUSH);
                    if (status != Z_OK && status != Z_STREAM_END) {
                        throw ParseError(std::format("zlib decompression failed ({})", status));
                    }
                }
            } catch (...) {
                inflateEnd(&stream);
                throw;
            }

            const auto decompressedSize = static_cast<std::size_t>(stream.total_out);
            const auto finalizationStatus = inflateEnd(&stream);
            if (finalizationStatus != Z_OK) {
                throw ParseError(std::format("zlib finalization failed ({})", finalizationStatus));
            }

            result.resize(decompressedSize);
            const auto actualFileSize = decompressedSize + 8;
            if (actualFileSize != declaredSize) {
                logger::warn(
                    "SWF font reader: '{}' declares {} uncompressed bytes but contains {}; using the "
                    "actual size.",
                    sourceName, declaredSize, actualFileSize);
            }
            return result;
        }

        std::vector<std::vector<Edge>> StitchContours(std::map<std::uint32_t, std::vector<Edge>>& edgesByFill) {
            std::vector<std::vector<Edge>> result;
            for (auto& [fillStyle, edges] : edgesByFill) {
                (void)fillStyle;
                while (!edges.empty()) {
                    std::vector<Edge> contour;
                    contour.push_back(edges.front());
                    edges.erase(edges.begin());

                    while (!edges.empty()) {
                        const auto endX = contour.back().toX;
                        const auto endY = contour.back().toY;
                        auto next = std::ranges::find_if(
                            edges, [&](const Edge& edge) { return edge.fromX == endX && edge.fromY == endY; });
                        if (next != edges.end()) {
                            contour.push_back(*next);
                            edges.erase(next);
                            continue;
                        }

                        next = std::ranges::find_if(
                            edges, [&](const Edge& edge) { return edge.toX == endX && edge.toY == endY; });
                        if (next == edges.end()) {
                            break;
                        }
                        contour.push_back(next->Reversed());
                        edges.erase(next);
                    }
                    result.push_back(std::move(contour));
                }
            }
            return result;
        }

        std::int16_t ConvertCoordinate(std::int32_t value, double divider, bool invert) {
            auto converted = static_cast<std::int32_t>(std::ceil(static_cast<double>(value) / divider));
            if (invert) {
                converted = -converted;
            }
            return static_cast<std::int16_t>(
                std::clamp(converted, static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::min()),
                           static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::max())));
        }

        std::vector<std::vector<Point>> ParseGlyphShape(std::span<const std::uint8_t> data, double divider) {
            BitReader bits(data);
            auto fillBits = static_cast<std::uint8_t>(bits.ReadUnsigned(4));
            auto lineBits = static_cast<std::uint8_t>(bits.ReadUnsigned(4));
            std::uint32_t fillStyle0 = 0;
            std::uint32_t fillStyle1 = 0;
            std::uint32_t lineStyle = 0;
            std::int32_t x = 0;
            std::int32_t y = 0;
            std::map<std::uint32_t, std::vector<Edge>> edgesByFill;
            std::vector<Edge> unstyledEdges;

            while (true) {
                const auto typeFlag = bits.ReadUnsigned(1) != 0;
                if (!typeFlag) {
                    const auto newStyles = bits.ReadUnsigned(1) != 0;
                    const auto changesLine = bits.ReadUnsigned(1) != 0;
                    const auto changesFill1 = bits.ReadUnsigned(1) != 0;
                    const auto changesFill0 = bits.ReadUnsigned(1) != 0;
                    const auto moves = bits.ReadUnsigned(1) != 0;
                    if (!newStyles && !changesLine && !changesFill1 && !changesFill0 && !moves) {
                        break;
                    }

                    if (moves) {
                        const auto moveBits = static_cast<std::uint8_t>(bits.ReadUnsigned(5));
                        x = bits.ReadSigned(moveBits);
                        y = bits.ReadSigned(moveBits);
                    }
                    if (changesFill0) {
                        fillStyle0 = bits.ReadUnsigned(fillBits);
                    }
                    if (changesFill1) {
                        fillStyle1 = bits.ReadUnsigned(fillBits);
                    }
                    if (changesLine) {
                        lineStyle = bits.ReadUnsigned(lineBits);
                    }
                    if (newStyles) {
                        throw ParseError("font glyph unexpectedly defines new styles");
                    }
                    continue;
                }

                Edge edge;
                edge.fromX = x;
                edge.fromY = y;
                const auto straight = bits.ReadUnsigned(1) != 0;
                const auto coordinateBits = static_cast<std::uint8_t>(bits.ReadUnsigned(4) + 2);
                if (straight) {
                    const auto generalLine = bits.ReadUnsigned(1) != 0;
                    if (generalLine) {
                        x += bits.ReadSigned(coordinateBits);
                        y += bits.ReadSigned(coordinateBits);
                    } else {
                        const auto verticalLine = bits.ReadUnsigned(1) != 0;
                        if (verticalLine) {
                            y += bits.ReadSigned(coordinateBits);
                        } else {
                            x += bits.ReadSigned(coordinateBits);
                        }
                    }
                } else {
                    edge.curved = true;
                    edge.controlX = x + bits.ReadSigned(coordinateBits);
                    edge.controlY = y + bits.ReadSigned(coordinateBits);
                    x = edge.controlX + bits.ReadSigned(coordinateBits);
                    y = edge.controlY + bits.ReadSigned(coordinateBits);
                }
                edge.toX = x;
                edge.toY = y;
                unstyledEdges.push_back(edge);

                if (fillStyle0 != 0) {
                    edgesByFill[fillStyle0].push_back(edge.Reversed());
                }
                if (fillStyle1 != 0) {
                    edgesByFill[fillStyle1].push_back(edge);
                }
                (void)lineStyle;
            }

            if (edgesByFill.empty() && !unstyledEdges.empty()) {
                edgesByFill[1] = std::move(unstyledEdges);
            }

            std::vector<std::vector<Point>> contours;
            for (const auto& edges : StitchContours(edgesByFill)) {
                if (edges.empty()) {
                    continue;
                }

                std::vector<Point> points;
                points.push_back({ConvertCoordinate(edges.front().fromX, divider, false),
                                  ConvertCoordinate(edges.front().fromY, divider, true), true});
                for (const auto& edge : edges) {
                    if (edge.curved) {
                        points.push_back({ConvertCoordinate(edge.controlX, divider, false),
                                          ConvertCoordinate(edge.controlY, divider, true), false});
                    }
                    points.push_back({ConvertCoordinate(edge.toX, divider, false),
                                      ConvertCoordinate(edge.toY, divider, true), true});
                }

                if (points.size() > 1 && points.front() == points.back()) {
                    points.pop_back();
                }
                if (!points.empty()) {
                    contours.push_back(std::move(points));
                }
            }
            return contours;
        }

        void UpdateGlyphBounds(Glyph& glyph) {
            bool hasPoint = false;
            for (const auto& contour : glyph.contours) {
                for (const auto& point : contour) {
                    if (!hasPoint) {
                        glyph.xMin = glyph.xMax = point.x;
                        glyph.yMin = glyph.yMax = point.y;
                        hasPoint = true;
                    } else {
                        glyph.xMin = std::min(glyph.xMin, point.x);
                        glyph.yMin = std::min(glyph.yMin, point.y);
                        glyph.xMax = std::max(glyph.xMax, point.x);
                        glyph.yMax = std::max(glyph.yMax, point.y);
                    }
                }
            }
        }

        std::optional<ParsedFont> ParseFontTag(std::span<const std::uint8_t> data, std::uint16_t tagCode,
                                               const std::string& sourceName) {
            ByteReader reader(data);
            ParsedFont font;
            font.id = reader.ReadU16();
            const auto flags = reader.ReadU8();
            const auto hasLayout = (flags & 0x80) != 0;
            const auto wideOffsets = (flags & 0x08) != 0;
            const auto wideCodes = (flags & 0x04) != 0;
            font.italic = (flags & 0x02) != 0;
            font.bold = (flags & 0x01) != 0;
            reader.Skip(1);  // LanguageCode

            const auto nameLength = reader.ReadU8();
            const auto nameBytes = reader.ReadBytes(nameLength);
            font.tagName = TrimNulls(std::string(reinterpret_cast<const char*>(nameBytes.data()), nameBytes.size()));
            font.sourceName = sourceName;
            const auto glyphCount = reader.ReadU16();
            font.supplementalLanguage = GetSupplementalGlyphLanguage(sourceName, font.tagName, font.bold, font.italic);
            if (!IsRegularFuturaCondensed(font.tagName, font.bold, font.italic) &&
                font.supplementalLanguage == SupplementalGlyphLanguage::None) {
                return std::nullopt;
            }
            const auto offsetBase = reader.Position();

            std::vector<std::uint32_t> offsets(glyphCount);
            for (auto& offset : offsets) {
                offset = wideOffsets ? reader.ReadU32() : reader.ReadU16();
            }
            const auto codeTableOffset = wideOffsets ? reader.ReadU32() : reader.ReadU16();
            if (codeTableOffset > data.size() - offsetBase) {
                throw ParseError("font code table exceeds tag");
            }

            const auto divider = tagCode == DEFINE_FONT_3 ? 20.0 : 1.0;
            font.glyphs.resize(glyphCount);
            for (std::size_t index = 0; index < glyphCount; ++index) {
                const auto start = static_cast<std::size_t>(offsets[index]);
                const auto end = index + 1 < glyphCount ? static_cast<std::size_t>(offsets[index + 1])
                                                        : static_cast<std::size_t>(codeTableOffset);
                if (start > end || end > codeTableOffset) {
                    throw ParseError("invalid glyph shape offsets");
                }
                font.glyphs[index].contours = ParseGlyphShape(data.subspan(offsetBase + start, end - start), divider);
                UpdateGlyphBounds(font.glyphs[index]);
            }

            reader.Seek(offsetBase + codeTableOffset);
            for (auto& glyph : font.glyphs) {
                glyph.code = wideCodes ? reader.ReadU16() : reader.ReadU8();
            }

            if (hasLayout) {
                font.ascent = static_cast<std::int16_t>(std::min<std::int32_t>(
                    static_cast<std::int32_t>(std::lround(reader.ReadU16() / divider)), TRUE_TYPE_UNITS_PER_EM));
                font.descent = static_cast<std::int16_t>(-std::min<std::int32_t>(
                    static_cast<std::int32_t>(std::lround(reader.ReadU16() / divider)), TRUE_TYPE_UNITS_PER_EM));
                font.leading = static_cast<std::int16_t>(std::lround(reader.ReadS16() / divider));
                for (auto& glyph : font.glyphs) {
                    glyph.advance = static_cast<std::uint16_t>(
                        std::clamp<std::int32_t>(static_cast<std::int32_t>(std::lround(reader.ReadU16() / divider)), 0,
                                                 std::numeric_limits<std::uint16_t>::max()));
                }
                for (std::size_t index = 0; index < glyphCount; ++index) {
                    reader.Skip(ReadRectSize(data.subspan(reader.Position())));
                }
            } else {
                for (auto& glyph : font.glyphs) {
                    glyph.advance = static_cast<std::uint16_t>(
                        std::clamp<std::int32_t>(static_cast<std::int32_t>(glyph.xMax) - glyph.xMin + 100, 0,
                                                 std::numeric_limits<std::uint16_t>::max()));
                }
            }
            return font;
        }

        void WriteU16(std::vector<std::uint8_t>& output, std::uint16_t value) {
            output.push_back(static_cast<std::uint8_t>(value >> 8));
            output.push_back(static_cast<std::uint8_t>(value));
        }

        void WriteS16(std::vector<std::uint8_t>& output, std::int16_t value) {
            WriteU16(output, static_cast<std::uint16_t>(value));
        }

        void WriteU32(std::vector<std::uint8_t>& output, std::uint32_t value) {
            output.push_back(static_cast<std::uint8_t>(value >> 24));
            output.push_back(static_cast<std::uint8_t>(value >> 16));
            output.push_back(static_cast<std::uint8_t>(value >> 8));
            output.push_back(static_cast<std::uint8_t>(value));
        }

        void PatchU32(std::vector<std::uint8_t>& output, std::size_t position, std::uint32_t value) {
            if (position + 4 > output.size()) {
                throw ParseError("TrueType patch exceeds output");
            }
            output[position] = static_cast<std::uint8_t>(value >> 24);
            output[position + 1] = static_cast<std::uint8_t>(value >> 16);
            output[position + 2] = static_cast<std::uint8_t>(value >> 8);
            output[position + 3] = static_cast<std::uint8_t>(value);
        }

        std::uint32_t Checksum(std::span<const std::uint8_t> data) {
            std::uint32_t result = 0;
            for (std::size_t position = 0; position < data.size(); position += 4) {
                std::uint32_t word = 0;
                for (std::size_t byte = 0; byte < 4; ++byte) {
                    word <<= 8;
                    if (position + byte < data.size()) {
                        word |= data[position + byte];
                    }
                }
                result += word;
            }
            return result;
        }

        std::vector<std::uint16_t> Utf8ToUtf16(std::string_view value) {
            std::vector<std::uint16_t> result;
            for (std::size_t index = 0; index < value.size();) {
                const auto first = static_cast<std::uint8_t>(value[index]);
                std::uint32_t codePoint = 0xFFFD;
                std::size_t count = 1;
                if ((first & 0x80) == 0) {
                    codePoint = first;
                } else if ((first & 0xE0) == 0xC0 && index + 1 < value.size()) {
                    codePoint = ((first & 0x1F) << 6) | (static_cast<std::uint8_t>(value[index + 1]) & 0x3F);
                    count = 2;
                } else if ((first & 0xF0) == 0xE0 && index + 2 < value.size()) {
                    codePoint = ((first & 0x0F) << 12) | ((static_cast<std::uint8_t>(value[index + 1]) & 0x3F) << 6) |
                                (static_cast<std::uint8_t>(value[index + 2]) & 0x3F);
                    count = 3;
                } else if ((first & 0xF8) == 0xF0 && index + 3 < value.size()) {
                    codePoint = ((first & 0x07) << 18) | ((static_cast<std::uint8_t>(value[index + 1]) & 0x3F) << 12) |
                                ((static_cast<std::uint8_t>(value[index + 2]) & 0x3F) << 6) |
                                (static_cast<std::uint8_t>(value[index + 3]) & 0x3F);
                    count = 4;
                }
                index += count;

                if (codePoint <= 0xFFFF) {
                    result.push_back(static_cast<std::uint16_t>(codePoint));
                } else if (codePoint <= 0x10FFFF) {
                    codePoint -= 0x10000;
                    result.push_back(static_cast<std::uint16_t>(0xD800 + (codePoint >> 10)));
                    result.push_back(static_cast<std::uint16_t>(0xDC00 + (codePoint & 0x3FF)));
                }
            }
            return result;
        }

        std::vector<std::uint8_t> MakeNameTable(const ParsedFont& font, const std::string& familyName) {
            const auto style = font.bold && font.italic ? "Bold Italic"
                               : font.bold              ? "Bold"
                               : font.italic            ? "Italic"
                                                        : "Regular";
            auto postScriptName = familyName;
            std::erase_if(postScriptName,
                          [](unsigned char character) { return !std::isalnum(character) && character != '-'; });
            if (postScriptName.empty()) {
                postScriptName = "SkyrimFont";
            }

            const std::array<std::pair<std::uint16_t, std::string>, 5> names = {
                {{1, familyName}, {2, style}, {4, familyName}, {5, "Version 1.0"}, {6, postScriptName}}};
            std::vector<std::uint8_t> strings;
            struct NameRecord {
                std::uint16_t id;
                std::uint16_t length;
                std::uint16_t offset;
            };
            std::vector<NameRecord> records;
            for (const auto& [id, value] : names) {
                const auto utf16 = Utf8ToUtf16(value);
                const auto offset = static_cast<std::uint16_t>(strings.size());
                for (const auto character : utf16) {
                    WriteU16(strings, character);
                }
                records.push_back({id, static_cast<std::uint16_t>(strings.size() - offset), offset});
            }

            std::vector<std::uint8_t> result;
            WriteU16(result, 0);
            WriteU16(result, static_cast<std::uint16_t>(records.size()));
            WriteU16(result, static_cast<std::uint16_t>(6 + records.size() * 12));
            for (const auto& record : records) {
                WriteU16(result, 3);
                WriteU16(result, 1);
                WriteU16(result, 0x0409);
                WriteU16(result, record.id);
                WriteU16(result, record.length);
                WriteU16(result, record.offset);
            }
            result.insert(result.end(), strings.begin(), strings.end());
            return result;
        }

        std::vector<std::uint8_t> MakeCmapTable(const std::vector<std::pair<std::uint16_t, std::uint16_t>>& mappings) {
            struct Group {
                std::uint16_t start;
                std::uint16_t end;
                std::uint16_t firstGlyph;
            };
            std::vector<Group> groups;
            for (const auto& [code, glyph] : mappings) {
                if (code == 0xFFFF) {
                    continue;
                }
                if (!groups.empty() && code == groups.back().end + 1 &&
                    glyph == groups.back().firstGlyph + (code - groups.back().start)) {
                    groups.back().end = code;
                } else {
                    groups.push_back({code, code, glyph});
                }
            }

            std::vector<std::uint8_t> format4;
            const auto segCount = static_cast<std::uint16_t>(groups.size() + 1);
            const auto format4Length = static_cast<std::size_t>(16 + segCount * 8);
            if (format4Length <= std::numeric_limits<std::uint16_t>::max()) {
                WriteU16(format4, 4);
                WriteU16(format4, static_cast<std::uint16_t>(format4Length));
                WriteU16(format4, 0);
                WriteU16(format4, static_cast<std::uint16_t>(segCount * 2));
                const auto power = static_cast<std::uint16_t>(std::bit_floor(segCount));
                WriteU16(format4, static_cast<std::uint16_t>(power * 2));
                WriteU16(format4, static_cast<std::uint16_t>(std::bit_width(power) - 1));
                WriteU16(format4, static_cast<std::uint16_t>(segCount * 2 - power * 2));
                for (const auto& group : groups) WriteU16(format4, group.end);
                WriteU16(format4, 0xFFFF);
                WriteU16(format4, 0);
                for (const auto& group : groups) WriteU16(format4, group.start);
                WriteU16(format4, 0xFFFF);
                for (const auto& group : groups) {
                    WriteU16(format4, static_cast<std::uint16_t>(group.firstGlyph - group.start));
                }
                WriteU16(format4, 1);
                for (std::uint16_t index = 0; index < segCount; ++index) WriteU16(format4, 0);
            }

            std::vector<std::uint8_t> format12;
            WriteU16(format12, 12);
            WriteU16(format12, 0);
            WriteU32(format12, static_cast<std::uint32_t>(16 + groups.size() * 12));
            WriteU32(format12, 0);
            WriteU32(format12, static_cast<std::uint32_t>(groups.size()));
            for (const auto& group : groups) {
                WriteU32(format12, group.start);
                WriteU32(format12, group.end);
                WriteU32(format12, group.firstGlyph);
            }

            const auto recordCount = static_cast<std::uint16_t>(format4.empty() ? 1 : 2);
            std::vector<std::uint8_t> result;
            WriteU16(result, 0);
            WriteU16(result, recordCount);
            auto tableOffset = static_cast<std::uint32_t>(4 + recordCount * 8);
            if (!format4.empty()) {
                WriteU16(result, 3);
                WriteU16(result, 1);
                WriteU32(result, tableOffset);
                tableOffset += static_cast<std::uint32_t>(format4.size());
            }
            WriteU16(result, 3);
            WriteU16(result, 10);
            WriteU32(result, tableOffset);
            result.insert(result.end(), format4.begin(), format4.end());
            result.insert(result.end(), format12.begin(), format12.end());
            return result;
        }

        std::vector<std::uint8_t> BuildTrueType(ParsedFont font) {
            std::ranges::sort(font.glyphs, {}, &Glyph::code);
            const auto duplicate = std::ranges::unique(font.glyphs, {}, &Glyph::code);
            font.glyphs.erase(duplicate.begin(), duplicate.end());
            if (font.glyphs.empty()) {
                return {};
            }

            std::vector<std::uint8_t> glyf;
            std::vector<std::uint32_t> locations{0};
            std::vector<std::pair<std::uint16_t, std::uint16_t>> mappings;
            std::uint16_t maxPoints = 0;
            std::uint16_t maxContours = 0;
            std::int16_t globalXMin = 0;
            std::int16_t globalYMin = 0;
            std::int16_t globalXMax = 0;
            std::int16_t globalYMax = 0;
            bool hasBounds = false;

            // Glyph zero is the required .notdef glyph. It is intentionally empty.
            locations.push_back(0);
            std::uint16_t glyphIndex = 1;
            for (const auto& glyph : font.glyphs) {
                mappings.emplace_back(glyph.code, glyphIndex++);
                std::size_t pointCount = 0;
                for (const auto& contour : glyph.contours) pointCount += contour.size();
                maxPoints = std::max(maxPoints, static_cast<std::uint16_t>(std::min<std::size_t>(
                                                    pointCount, std::numeric_limits<std::uint16_t>::max())));
                maxContours = std::max(maxContours, static_cast<std::uint16_t>(glyph.contours.size()));

                if (glyph.contours.empty() || pointCount == 0) {
                    locations.push_back(static_cast<std::uint32_t>(glyf.size()));
                    continue;
                }

                WriteS16(glyf, static_cast<std::int16_t>(glyph.contours.size()));
                WriteS16(glyf, glyph.xMin);
                WriteS16(glyf, glyph.yMin);
                WriteS16(glyf, glyph.xMax);
                WriteS16(glyf, glyph.yMax);
                std::uint16_t endpoint = 0;
                for (const auto& contour : glyph.contours) {
                    endpoint = static_cast<std::uint16_t>(endpoint + contour.size());
                    WriteU16(glyf, static_cast<std::uint16_t>(endpoint - 1));
                }
                WriteU16(glyf, 0);  // instruction length
                for (const auto& contour : glyph.contours) {
                    for (const auto& point : contour) glyf.push_back(point.onCurve ? 1 : 0);
                }
                std::int16_t previous = 0;
                for (const auto& contour : glyph.contours) {
                    for (const auto& point : contour) {
                        WriteS16(glyf, static_cast<std::int16_t>(point.x - previous));
                        previous = point.x;
                    }
                }
                previous = 0;
                for (const auto& contour : glyph.contours) {
                    for (const auto& point : contour) {
                        WriteS16(glyf, static_cast<std::int16_t>(point.y - previous));
                        previous = point.y;
                    }
                }
                while (glyf.size() % 4 != 0) glyf.push_back(0);
                locations.push_back(static_cast<std::uint32_t>(glyf.size()));

                if (!hasBounds) {
                    globalXMin = glyph.xMin;
                    globalYMin = glyph.yMin;
                    globalXMax = glyph.xMax;
                    globalYMax = glyph.yMax;
                    hasBounds = true;
                } else {
                    globalXMin = std::min(globalXMin, glyph.xMin);
                    globalYMin = std::min(globalYMin, glyph.yMin);
                    globalXMax = std::max(globalXMax, glyph.xMax);
                    globalYMax = std::max(globalYMax, glyph.yMax);
                }
            }

            const auto numberOfGlyphs = static_cast<std::uint16_t>(font.glyphs.size() + 1);
            const auto ascent = font.ascent != 0 ? font.ascent : globalYMax;
            const auto descent = font.descent != 0 ? font.descent : globalYMin;

            std::vector<std::uint8_t> head;
            WriteU32(head, 0x00010000);
            WriteU32(head, 0x00010000);
            WriteU32(head, 0);
            WriteU32(head, 0x5F0F3CF5);
            WriteU16(head, 0);
            WriteU16(head, TRUE_TYPE_UNITS_PER_EM);
            for (int index = 0; index < 4; ++index) WriteU32(head, 0);
            WriteS16(head, globalXMin);
            WriteS16(head, globalYMin);
            WriteS16(head, globalXMax);
            WriteS16(head, globalYMax);
            WriteU16(head, static_cast<std::uint16_t>((font.bold ? 1 : 0) | (font.italic ? 2 : 0)));
            WriteU16(head, 8);
            WriteS16(head, 2);
            WriteS16(head, 1);  // long loca offsets
            WriteS16(head, 0);

            std::uint16_t advanceMax = 0;
            std::int16_t minLeftBearing = 0;
            std::int16_t minRightBearing = 0;
            std::int16_t maxExtent = 0;
            bool hasMetrics = false;
            std::vector<std::uint8_t> hmtx;
            WriteU16(hmtx, 0);
            WriteS16(hmtx, 0);
            std::uint64_t advanceTotal = 0;
            for (const auto& glyph : font.glyphs) {
                WriteU16(hmtx, glyph.advance);
                WriteS16(hmtx, glyph.xMin);
                advanceMax = std::max(advanceMax, glyph.advance);
                const auto rightBearing =
                    static_cast<std::int16_t>(static_cast<std::int32_t>(glyph.advance) - glyph.xMax);
                if (!hasMetrics) {
                    minLeftBearing = glyph.xMin;
                    minRightBearing = rightBearing;
                    maxExtent = glyph.xMax;
                    hasMetrics = true;
                } else {
                    minLeftBearing = std::min(minLeftBearing, glyph.xMin);
                    minRightBearing = std::min(minRightBearing, rightBearing);
                    maxExtent = std::max(maxExtent, glyph.xMax);
                }
                advanceTotal += glyph.advance;
            }

            std::vector<std::uint8_t> hhea;
            WriteU32(hhea, 0x00010000);
            WriteS16(hhea, ascent);
            WriteS16(hhea, descent);
            WriteS16(hhea, font.leading);
            WriteU16(hhea, advanceMax);
            WriteS16(hhea, minLeftBearing);
            WriteS16(hhea, minRightBearing);
            WriteS16(hhea, maxExtent);
            WriteS16(hhea, 1);
            WriteS16(hhea, 0);
            WriteS16(hhea, 0);
            for (int index = 0; index < 4; ++index) WriteS16(hhea, 0);
            WriteS16(hhea, 0);
            WriteU16(hhea, numberOfGlyphs);

            std::vector<std::uint8_t> loca;
            for (const auto location : locations) WriteU32(loca, location);

            std::vector<std::uint8_t> maxp;
            WriteU32(maxp, 0x00010000);
            WriteU16(maxp, numberOfGlyphs);
            WriteU16(maxp, maxPoints);
            WriteU16(maxp, maxContours);
            WriteU16(maxp, 0);
            WriteU16(maxp, 0);
            WriteU16(maxp, 1);
            for (int index = 0; index < 8; ++index) WriteU16(maxp, 0);

            std::vector<std::uint8_t> os2;
            WriteU16(os2, 0);
            WriteS16(os2, static_cast<std::int16_t>(advanceTotal / std::max<std::size_t>(1, font.glyphs.size())));
            WriteU16(os2, font.bold ? 700 : 400);
            WriteU16(os2, 5);
            WriteU16(os2, 0);
            WriteS16(os2, 650);
            WriteS16(os2, 600);
            WriteS16(os2, 0);
            WriteS16(os2, 75);
            WriteS16(os2, 650);
            WriteS16(os2, 600);
            WriteS16(os2, 0);
            WriteS16(os2, 350);
            WriteS16(os2, 50);
            WriteS16(os2, 250);
            WriteS16(os2, 0);
            for (int index = 0; index < 10; ++index) os2.push_back(0);
            for (int index = 0; index < 4; ++index) WriteU32(os2, 0);
            os2.insert(os2.end(), {'S', 'M', 'F', ' '});
            WriteU16(os2, static_cast<std::uint16_t>((font.italic ? 1 : 0) | (font.bold ? 0x20 : 0) |
                                                     (!font.bold && !font.italic ? 0x40 : 0)));
            WriteU16(os2, mappings.front().first);
            WriteU16(os2, mappings.back().first);
            WriteS16(os2, ascent);
            WriteS16(os2, descent);
            WriteS16(os2, font.leading);
            WriteU16(os2, static_cast<std::uint16_t>(std::max<std::int32_t>(0, ascent)));
            WriteU16(os2, static_cast<std::uint16_t>(std::max<std::int32_t>(0, -descent)));

            std::vector<std::uint8_t> post;
            WriteU32(post, 0x00030000);
            WriteU32(post, 0);
            WriteS16(post, 0);
            WriteS16(post, 0);
            WriteU32(post, 0);
            for (int index = 0; index < 4; ++index) WriteU32(post, 0);

            const auto familyName = !font.fullName.empty() ? font.fullName : font.tagName;
            std::map<std::string, std::vector<std::uint8_t>> tables;
            tables["OS/2"] = std::move(os2);
            tables["cmap"] = MakeCmapTable(mappings);
            tables["glyf"] = std::move(glyf);
            tables["head"] = std::move(head);
            tables["hhea"] = std::move(hhea);
            tables["hmtx"] = std::move(hmtx);
            tables["loca"] = std::move(loca);
            tables["maxp"] = std::move(maxp);
            tables["name"] = MakeNameTable(font, familyName);
            tables["post"] = std::move(post);

            const auto tableCount = static_cast<std::uint16_t>(tables.size());
            const auto power = static_cast<std::uint16_t>(std::bit_floor(tableCount));
            std::vector<std::uint8_t> result;
            WriteU32(result, 0x00010000);
            WriteU16(result, tableCount);
            WriteU16(result, static_cast<std::uint16_t>(power * 16));
            WriteU16(result, static_cast<std::uint16_t>(std::bit_width(power) - 1));
            WriteU16(result, static_cast<std::uint16_t>(tableCount * 16 - power * 16));

            const auto directoryStart = result.size();
            result.resize(result.size() + tableCount * 16);
            auto tableOffset = static_cast<std::uint32_t>(result.size());
            std::size_t directoryIndex = 0;
            std::size_t headOffset = 0;
            for (const auto& [tag, table] : tables) {
                while (tableOffset % 4 != 0) {
                    result.push_back(0);
                    ++tableOffset;
                }
                const auto entry = directoryStart + directoryIndex++ * 16;
                std::copy_n(tag.begin(), 4, result.begin() + entry);
                PatchU32(result, entry + 4, Checksum(table));
                PatchU32(result, entry + 8, tableOffset);
                PatchU32(result, entry + 12, static_cast<std::uint32_t>(table.size()));
                if (tag == "head") headOffset = tableOffset;
                result.insert(result.end(), table.begin(), table.end());
                tableOffset += static_cast<std::uint32_t>(table.size());
            }
            while (result.size() % 4 != 0) result.push_back(0);
            PatchU32(result, headOffset + 8, 0xB1B0AFBA - Checksum(result));
            return result;
        }

        std::vector<ParsedFont> ParseSwf(std::span<const std::uint8_t> file, const std::string& sourceName) {
            const auto body = DecompressSwf(file, sourceName);
            ByteReader reader(body);
            reader.Skip(ReadRectSize(body));
            reader.Skip(4);  // FrameRate and FrameCount

            std::vector<ParsedFont> fonts;
            while (reader.Remaining() >= 2) {
                const auto recordHeader = reader.ReadU16();
                const auto tagCode = static_cast<std::uint16_t>(recordHeader >> 6);
                auto tagLength = static_cast<std::uint32_t>(recordHeader & 0x3F);
                if (tagLength == 0x3F) tagLength = reader.ReadU32();
                const auto tagData = reader.ReadBytes(tagLength);

                if (tagCode == DEFINE_FONT_2 || tagCode == DEFINE_FONT_3) {
                    auto font = ParseFontTag(tagData, tagCode, sourceName);
                    if (font) {
                        fonts.push_back(std::move(*font));
                    }
                } else if (tagCode == 0) {
                    break;
                }
            }

            for (auto& font : fonts) {
                if (IsRegularFuturaCondensed(font.tagName, font.bold, font.italic)) {
                    font.fullName = std::format("Futura Condensed ({})", GetLanguageName(sourceName));
                }
            }
            return fonts;
        }

        std::vector<std::string> GetFontResourceNames() {
            // The canonical files cover archive-backed vanilla/AE resources. Loose
            // fonts_*.swf files are added below so font mods can contribute faces.
            std::set<std::string> names = {"fonts_en.swf", "fonts_cn.swf", "fonts_ja.swf", "fonts_pl.swf",
                                           "fonts_ru.swf"};

            std::error_code error;
            const std::filesystem::path interfaceDirectory = "Data/Interface";
            for (std::filesystem::directory_iterator iterator(interfaceDirectory, error), end;
                 !error && iterator != end; iterator.increment(error)) {
                if (!iterator->is_regular_file(error)) {
                    continue;
                }
                const auto filename = iterator->path().filename().string();
                const auto normalized = ToLower(filename);
                if (normalized.starts_with("fonts_") && normalized.ends_with(".swf")) {
                    names.insert(filename);
                }
            }
            if (error && error != std::errc::no_such_file_or_directory) {
                logger::warn("SWF font reader: Could not enumerate '{}': {}.", interfaceDirectory.string(),
                             error.message());
            }

            std::vector<std::string> result(names.begin(), names.end());
            std::ranges::stable_sort(result, [](const std::string& left, const std::string& right) {
                const auto leftName = ToLower(left);
                const auto rightName = ToLower(right);
                return std::tuple(leftName != "fonts_en.swf", leftName) <
                       std::tuple(rightName != "fonts_en.swf", rightName);
            });
            return result;
        }
    }

    std::vector<FontData> LoadRegularFuturaFonts() {
        std::vector<FontData> result;
        std::set<std::string> registeredNames;
        for (const auto& resourceName : GetFontResourceNames()) {
            const auto file = ReadGameResource(resourceName);
            if (!file) {
                continue;
            }

            try {
                auto parsedFonts = ParseSwf(*file, resourceName);
                const auto supplemental = std::ranges::find_if(parsedFonts, [](const ParsedFont& font) {
                    return font.supplementalLanguage != SupplementalGlyphLanguage::None;
                });
                std::string supplementalName;
                SupplementalGlyphLanguage supplementalLanguage = SupplementalGlyphLanguage::None;
                std::vector<std::uint8_t> supplementalTrueTypeData;
                if (supplemental != parsedFonts.end()) {
                    supplementalName = supplemental->tagName;
                    supplementalLanguage = supplemental->supplementalLanguage;
                    supplementalTrueTypeData = BuildTrueType(std::move(*supplemental));
                    if (supplementalTrueTypeData.empty()) {
                        logger::warn("SWF font reader: Supplemental face '{}' in '{}' contains no usable glyphs.",
                                     supplementalName, resourceName);
                        supplementalLanguage = SupplementalGlyphLanguage::None;
                    }
                }

                std::size_t added = 0;
                for (auto& parsed : parsedFonts) {
                    if (!IsRegularFuturaCondensed(parsed.tagName, parsed.bold, parsed.italic)) {
                        continue;
                    }
                    const auto displayName = !parsed.fullName.empty() ? parsed.fullName : parsed.tagName;
                    const auto normalizedName = ToLower(displayName);
                    if (displayName.empty() || registeredNames.contains(normalizedName)) {
                        continue;
                    }

                    auto trueTypeData = BuildTrueType(parsed);
                    if (trueTypeData.empty()) {
                        logger::warn("SWF font reader: '{}' in '{}' contains no usable glyphs.", displayName,
                                     resourceName);
                        continue;
                    }

                    FontData font;
                    font.name = displayName;
                    font.sourceName = resourceName;
                    font.bold = parsed.bold;
                    font.italic = parsed.italic;
                    font.trueTypeData = std::move(trueTypeData);
                    font.supplementalName = supplementalName;
                    font.supplementalLanguage = supplementalLanguage;
                    font.supplementalTrueTypeData = std::move(supplementalTrueTypeData);
                    font.aliases.push_back(displayName);
                    if (!parsed.tagName.empty() && ToLower(parsed.tagName) != normalizedName) {
                        font.aliases.push_back(parsed.tagName);
                    }
                    result.push_back(std::move(font));
                    registeredNames.insert(normalizedName);
                    ++added;
                }
                if (supplementalLanguage != SupplementalGlyphLanguage::None) {
                    logger::info("SWF font reader: Attached supplemental face '{}' from Interface\\{}.",
                                 supplementalName, resourceName);
                }
                logger::info("SWF font reader: Loaded {} faces from Interface\\{}.", added, resourceName);
            } catch (const std::exception& exception) {
                logger::error("SWF font reader: Could not parse Interface\\{}: {}", resourceName, exception.what());
            }
        }
        return result;
    }
}
