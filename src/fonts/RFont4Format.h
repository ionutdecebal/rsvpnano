#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "text/AsciiText.h"
#include "text/UnicodeText.h"

namespace RFont4 {

    constexpr char kExtension[] = ".rfont4";
    constexpr char kFilename[] = "font.rfont4";
    constexpr uint32_t kMagic = 0x34544652UL; // "RFT4", little endian on disk.
    constexpr uint16_t kVersion = 8;
    constexpr size_t kPageMapBytes = 256;
    constexpr size_t kPageTableEntries = 256;
    constexpr size_t kSizeCount = 5;
    constexpr size_t kCompactStrikeIndex = 3;
    constexpr size_t kExtraLargeStrikeIndex = 4;
    constexpr size_t kPreviewStrikeIndex = 5;
    constexpr size_t kStrikeCount = 6;
    constexpr size_t kMaximumLayoutTableCount = 3;
    constexpr uint32_t kShapedGlyphCodepoint = UINT32_MAX;
    constexpr uint16_t kRawBitmapFlag = 0x8000U;
    constexpr uint16_t kBitmapByteMask = 0x7FFFU;
    constexpr uint16_t kRotateVerticalGlyph = UINT16_MAX;

    enum class BitmapEncoding : uint8_t {
        raw,
        lz4,
    };
    constexpr uint32_t layoutTag(char a, char b, char c, char d) {
        return static_cast<uint32_t>(static_cast<uint8_t>(a)) << 24U
             | static_cast<uint32_t>(static_cast<uint8_t>(b)) << 16U
             | static_cast<uint32_t>(static_cast<uint8_t>(c)) << 8U
             | static_cast<uint8_t>(d);
    }
    constexpr std::array kLayoutTags = {
        layoutTag('G', 'D', 'E', 'F'),
        layoutTag('G', 'S', 'U', 'B'),
        layoutTag('G', 'P', 'O', 'S'),
    };
    constexpr std::array<const char*, kSizeCount> kSizeIds = {"large", "medium", "small", "compact", "extra-large"};
    constexpr std::array<const char*, kSizeCount> kSizeLabels = {"Large", "Medium", "Small", "Compact", "Extra Large"};
    constexpr uint32_t kKnownScriptMask = UnicodeText::ScriptLatin | UnicodeText::ScriptCyrillic
                                        | UnicodeText::ScriptGreek | UnicodeText::ScriptHebrew
                                        | UnicodeText::ScriptArabic | UnicodeText::ScriptHan
                                        | UnicodeText::ScriptHiragana | UnicodeText::ScriptKatakana
                                        | UnicodeText::ScriptHangul | UnicodeText::ScriptMath;

    struct __attribute__((packed)) Header {
        uint32_t magic = kMagic;
        uint16_t version = kVersion;
        uint16_t headerSize = 0;
        uint16_t strikeRecordSize = 0;
        uint16_t glyphRecordSize = 0;
        uint16_t kerningRecordSize = 0;
        uint16_t supplementaryRecordSize = 0;
        uint16_t verticalRuleRecordSize = 0;
        uint16_t layoutTableRecordSize = 0;
        uint8_t strikeCount = 0;
        uint8_t layoutTableCount = 0;
        uint16_t nameSize = 0;
        uint16_t unitsPerEm = 0;
        uint16_t localeSize = 0;
        uint16_t pageTableCount = 0;
        uint16_t supplementaryCount = 0;
        uint16_t verticalRuleCount = 0;
        uint32_t glyphCount = 0;
        uint32_t sourceGlyphCount = 0;
        uint32_t scriptMask = 0;
        uint32_t nameOffset = 0;
        uint32_t localeOffset = 0;
        uint32_t strikesOffset = 0;
        uint32_t supplementaryOffset = 0;
        uint32_t pageMapOffset = 0;
        uint32_t pageTablesOffset = 0;
        uint32_t glyphIdsOffset = 0;
        uint32_t glyphMapOffset = 0;
        uint32_t verticalRulesOffset = 0;
        uint32_t layoutTablesOffset = 0;
        uint32_t totalSize = 0;
    };

    struct __attribute__((packed)) StrikeRecord {
        uint32_t kerningPairCount = 0;
        uint8_t yAdvance = 0;
        uint8_t ascent = 0;
        uint8_t descent = 0;
        int8_t wordInkTop = 0;
        int8_t wordInkBottom = -1;
        uint8_t maxGlyphWidth = 0;
        uint8_t maxGlyphHeight = 0;
        uint8_t pixelsPerEm = 0;
        uint8_t bitsPerPixel = 4;
        BitmapEncoding bitmapEncoding = BitmapEncoding::raw;
        uint32_t bitmapSize = 0;
        uint32_t glyphsOffset = 0;
        uint32_t kerningOffset = 0;
        uint32_t bitmapOffset = 0;
    };

    struct __attribute__((packed)) GlyphRecord {
        uint32_t bitmapOffset = 0;
        uint32_t kernOffset = 0;
        uint8_t width = 0;
        uint8_t height = 0;
        uint8_t rowStride = 0;
        uint8_t xAdvance = 0;
        int8_t xOffset = 0;
        int8_t yOffset = 0;
        uint16_t bitmapBytes = 0;
        uint16_t kernCount = 0;
    };

    struct __attribute__((packed)) SupplementaryRecord {
        uint32_t codepoint = 0;
        uint16_t glyphIndex = 0;
    };

    struct __attribute__((packed)) VerticalRule {
        uint32_t codepoint = 0;
        uint16_t alternateIndex = kRotateVerticalGlyph;
    };

    struct __attribute__((packed)) KerningRecord {
        uint32_t rightCodepoint = 0;
        int8_t xAdjust = 0;
    };

    struct __attribute__((packed)) LayoutTableRecord {
        uint32_t tag = 0;
        uint32_t offset = 0;
        uint32_t size = 0;
    };

    struct Directory {
        Header header;
        std::array<StrikeRecord, kStrikeCount> strikes{};
        std::array<LayoutTableRecord, kMaximumLayoutTableCount> layoutTables{};
    };

    static_assert(sizeof(Header) == 90);
    static_assert(sizeof(StrikeRecord) == 30);
    static_assert(sizeof(GlyphRecord) == 18);
    static_assert(sizeof(SupplementaryRecord) == 6);
    static_assert(sizeof(VerticalRule) == 6);
    static_assert(sizeof(KerningRecord) == 5);
    static_assert(sizeof(LayoutTableRecord) == 12);

    constexpr bool headerValid(const Header& header, size_t fileSize) {
        const bool hasShaping = header.layoutTableCount != 0;
        return header.magic == kMagic && header.version == kVersion && header.headerSize == sizeof(Header)
            && header.strikeRecordSize == sizeof(StrikeRecord) && header.glyphRecordSize == sizeof(GlyphRecord)
            && header.kerningRecordSize == sizeof(KerningRecord)
            && header.supplementaryRecordSize == sizeof(SupplementaryRecord)
            && header.verticalRuleRecordSize == sizeof(VerticalRule)
            && header.layoutTableRecordSize == sizeof(LayoutTableRecord)
            && (header.strikeCount == 4 || header.strikeCount == kStrikeCount)
            && header.layoutTableCount <= kMaximumLayoutTableCount && header.nameSize > 1
            && header.glyphCount > 0 && header.glyphCount <= UINT16_MAX
            && header.pageTableCount > 0 && header.pageTableCount <= UINT8_MAX
            && (header.scriptMask & ~kKnownScriptMask) == 0 && header.nameOffset == sizeof(Header)
            && header.localeOffset == header.nameOffset + header.nameSize
            && header.strikesOffset == header.localeOffset + header.localeSize
            && header.supplementaryOffset == header.strikesOffset + header.strikeCount * sizeof(StrikeRecord)
            && header.pageMapOffset
                   == header.supplementaryOffset + header.supplementaryCount * sizeof(SupplementaryRecord)
            && header.pageTablesOffset == header.pageMapOffset + kPageMapBytes
            && header.glyphIdsOffset
                   == header.pageTablesOffset
                    + header.pageTableCount * kPageTableEntries * sizeof(uint16_t)
            && header.glyphMapOffset
                   == header.glyphIdsOffset + (hasShaping ? header.glyphCount * sizeof(uint16_t) : 0)
            && header.verticalRulesOffset
                   == header.glyphMapOffset + header.sourceGlyphCount * sizeof(uint16_t)
            && header.totalSize == fileSize
            && (hasShaping ? header.unitsPerEm > 0 && header.sourceGlyphCount > 0
                                  && header.sourceGlyphCount <= UINT16_MAX
                           : header.unitsPerEm == 0 && header.sourceGlyphCount == 0);
    }

    inline bool layoutValid(const Header& header, std::span<const StrikeRecord, kStrikeCount> strikes,
                            std::span<const LayoutTableRecord> tables, size_t fileSize) {
        if (!headerValid(header, fileSize) || tables.size() != header.layoutTableCount)
            return false;

        const bool hasShaping = header.layoutTableCount != 0;
        uint64_t cursor = header.supplementaryOffset;
        const auto section = [&cursor](uint32_t offset, uint64_t bytes) {
            if (offset != cursor || bytes > UINT32_MAX - cursor)
                return false;
            cursor += bytes;
            return true;
        };
        if (!section(header.supplementaryOffset,
                     static_cast<uint64_t>(header.supplementaryCount) * sizeof(SupplementaryRecord))
            || !section(header.pageMapOffset, kPageMapBytes)
            || !section(header.pageTablesOffset,
                        static_cast<uint64_t>(header.pageTableCount) * kPageTableEntries * sizeof(uint16_t))
            || !section(header.glyphIdsOffset,
                        hasShaping ? static_cast<uint64_t>(header.glyphCount) * sizeof(uint16_t) : 0)
            || !section(header.glyphMapOffset,
                        static_cast<uint64_t>(header.sourceGlyphCount) * sizeof(uint16_t))
            || !section(header.verticalRulesOffset,
                        static_cast<uint64_t>(header.verticalRuleCount) * sizeof(VerticalRule)))
            return false;
        for (const StrikeRecord& strike: strikes.first(header.strikeCount)) {
            if (strike.yAdvance == 0 || strike.pixelsPerEm == 0
                || (strike.bitsPerPixel != 1 && strike.bitsPerPixel != 4)
                || (strike.bitmapEncoding != BitmapEncoding::raw
                    && strike.bitmapEncoding != BitmapEncoding::lz4)
                || !section(strike.glyphsOffset,
                            static_cast<uint64_t>(header.glyphCount) * sizeof(GlyphRecord))
                || !section(strike.kerningOffset,
                            static_cast<uint64_t>(strike.kerningPairCount) * sizeof(KerningRecord))
                || !section(strike.bitmapOffset, strike.bitmapSize))
                return false;
        }
        if (header.layoutTablesOffset != cursor)
            return false;
        cursor += tables.size_bytes();
        for (size_t index = 0; index < tables.size(); ++index) {
            const LayoutTableRecord& table = tables[index];
            if (std::ranges::find(kLayoutTags, table.tag) == kLayoutTags.end()
                || std::ranges::any_of(tables.first(index), [&](const LayoutTableRecord& prior) {
                       return prior.tag == table.tag;
                   }))
                return false;
            cursor = (cursor + 3U) & ~uint64_t{3U};
            if (table.size == 0 || !section(table.offset, table.size))
                return false;
        }
        return cursor == header.totalSize;
    }

    constexpr size_t bitmapBytes(const GlyphRecord& glyph) {
        return glyph.bitmapBytes & kBitmapByteMask;
    }

    constexpr bool bitmapStoredRaw(const StrikeRecord& strike, const GlyphRecord& glyph) {
        return strike.bitmapEncoding == BitmapEncoding::raw
            || (glyph.bitmapBytes & kRawBitmapFlag) != 0;
    }

    inline bool decompressLz4Block(std::span<const uint8_t> input, std::span<uint8_t> output) {
        size_t source = 0;
        size_t destination = 0;
        const auto length = [&](size_t base, size_t& result) {
            result = base;
            if (base != 15)
                return true;
            uint8_t extension = 0;
            do {
                if (source == input.size())
                    return false;
                extension = input[source++];
                result += extension;
            } while (extension == 255);
            return true;
        };

        while (source < input.size()) {
            const uint8_t token = input[source++];
            size_t literalBytes = 0;
            if (!length(token >> 4U, literalBytes)
                || literalBytes > input.size() - source
                || literalBytes > output.size() - destination)
                return false;
            std::copy_n(input.data() + source, literalBytes, output.data() + destination);
            source += literalBytes;
            destination += literalBytes;
            if (source == input.size())
                return destination == output.size();
            if (source + 2 > input.size())
                return false;
            const size_t offset = static_cast<size_t>(input[source])
                                | static_cast<size_t>(input[source + 1]) << 8U;
            source += 2;
            if (offset == 0 || offset > destination)
                return false;
            size_t matchBytes = 0;
            if (!length(token & 0x0FU, matchBytes)
                || matchBytes + 4 > output.size() - destination)
                return false;
            matchBytes += 4;
            for (size_t index = 0; index < matchBytes; ++index)
                output[destination + index] = output[destination + index - offset];
            destination += matchBytes;
        }
        return destination == output.size();
    }

    inline bool equalIgnoreCase(std::string_view left, std::string_view right) {
        return std::ranges::equal(left, right, [](unsigned char a, unsigned char b) {
            return AsciiText::toLower(static_cast<char>(a)) == AsciiText::toLower(static_cast<char>(b));
        });
    }

    inline bool hasFontExtension(std::string_view path) {
        const std::string_view extension{kExtension};
        return path.size() >= extension.size()
            && equalIgnoreCase(path.substr(path.size() - extension.size()), extension);
    }

    inline size_t sizeIndexForId(std::string_view id) {
        while (!id.empty() && AsciiText::isWhitespace(id.front()))
            id.remove_prefix(1);
        while (!id.empty() && AsciiText::isWhitespace(id.back()))
            id.remove_suffix(1);
        const auto size = std::ranges::find_if(kSizeIds, [id](const char* candidate) {
            return equalIgnoreCase(id, candidate);
        });
        return static_cast<size_t>(std::distance(kSizeIds.begin(), size));
    }

    inline const char* sizeId(size_t index) {
        return index < kSizeIds.size() ? kSizeIds[index] : kSizeIds[0];
    }

    inline const char* sizeLabel(size_t index) {
        return index < kSizeLabels.size() ? kSizeLabels[index] : kSizeLabels[0];
    }

} // namespace RFont4
