#include <unity.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "fonts/AlphaFont.h"
#include "fonts/RFont4Format.h"
#include "text/TextShaping.h"
#include "text/UnicodeText.h"

namespace {

    std::vector<uint8_t> readFont(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>{file}, {}};
    }

    template<typename Record>
    Record recordAt(const std::vector<uint8_t>& bytes, uint32_t offset, uint32_t index = 0) {
        Record record;
        std::memcpy(&record, bytes.data() + offset + static_cast<size_t>(index) * sizeof(record), sizeof(record));
        return record;
    }

    std::array<RFont4::StrikeRecord, RFont4::kSizeCount> readStrikes(const std::vector<uint8_t>& bytes,
                                                                     const RFont4::Header& header) {
        std::array<RFont4::StrikeRecord, RFont4::kSizeCount> strikes;
        std::memcpy(strikes.data(), bytes.data() + header.strikesOffset, sizeof(strikes));
        return strikes;
    }

    std::array<RFont4::LayoutTableRecord, RFont4::kMaximumLayoutTableCount> readTables(const std::vector<uint8_t>&
                                                                                           bytes,
                                                                                       const RFont4::Header& header) {
        std::array<RFont4::LayoutTableRecord, RFont4::kMaximumLayoutTableCount> tables{};
        std::memcpy(tables.data(), bytes.data() + header.layoutTablesOffset,
                    static_cast<size_t>(header.layoutTableCount) * sizeof(tables.front()));
        return tables;
    }

    uint32_t validateFamilyLookup(const std::vector<uint8_t>& bytes, const RFont4::Header& header) {
        uint32_t scriptMask = 0;
        for (uint32_t high = 0; high < RFont4::kPageMapBytes; ++high) {
            const uint8_t page = bytes[header.pageMapOffset + high];
            if (page >= header.pageTableCount)
                continue;
            for (uint32_t low = 0; low < RFont4::kPageTableEntries; ++low) {
                const uint32_t entry = page * RFont4::kPageTableEntries + low;
                const uint16_t glyphIndex = recordAt<uint16_t>(bytes, header.pageTablesOffset, entry);
                if (glyphIndex == UINT16_MAX)
                    continue;
                TEST_ASSERT_LESS_THAN(header.glyphCount, glyphIndex);
                scriptMask |= UnicodeText::scriptMask((high << 8U) | low);
            }
        }
        uint32_t previousCodepoint = UINT16_MAX;
        for (uint16_t index = 0; index < header.supplementaryCount; ++index) {
            const auto record = recordAt<RFont4::SupplementaryRecord>(bytes, header.supplementaryOffset, index);
            TEST_ASSERT_GREATER_THAN(previousCodepoint, record.codepoint);
            TEST_ASSERT_LESS_THAN(header.glyphCount, record.glyphIndex);
            scriptMask |= UnicodeText::scriptMask(record.codepoint);
            previousCodepoint = record.codepoint;
        }

        for (uint32_t glyphId = 0; glyphId < header.sourceGlyphCount; ++glyphId) {
            const uint16_t glyphIndex = recordAt<uint16_t>(bytes, header.glyphMapOffset, glyphId);
            if (glyphIndex == UINT16_MAX)
                continue;
            TEST_ASSERT_TRUE(glyphIndex < header.glyphCount);
            TEST_ASSERT_EQUAL_UINT16(glyphId, recordAt<uint16_t>(bytes, header.glyphIdsOffset, glyphIndex));
        }
        uint32_t previousVerticalCodepoint = 0;
        for (uint16_t index = 0; index < header.verticalRuleCount; ++index) {
            const auto rule = recordAt<RFont4::VerticalRule>(bytes, header.verticalRulesOffset, index);
            TEST_ASSERT_TRUE(index == 0 || rule.codepoint > previousVerticalCodepoint);
            TEST_ASSERT_LESS_OR_EQUAL_HEX32(0x10FFFF, rule.codepoint);
            TEST_ASSERT_TRUE(rule.alternateIndex == RFont4::kRotateVerticalGlyph
                             || rule.alternateIndex < header.glyphCount);
            previousVerticalCodepoint = rule.codepoint;
        }
        return scriptMask;
    }

    void validateStrike(const std::vector<uint8_t>& bytes, const RFont4::StrikeRecord& strike, uint32_t glyphCount) {
        TEST_ASSERT_TRUE(strike.bitsPerPixel == 1 || strike.bitsPerPixel == 4);
        TEST_ASSERT_TRUE(strike.bitmapEncoding == RFont4::BitmapEncoding::raw
                         || strike.bitmapEncoding == RFont4::BitmapEncoding::lz4);
        std::unordered_map<std::string_view, uint32_t> bitmapOffsets;
        std::unordered_map<std::string_view, uint32_t> kerningOffsets;
        for (uint32_t glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex) {
            const auto glyph = recordAt<RFont4::GlyphRecord>(bytes, strike.glyphsOffset, glyphIndex);
            const size_t decodedBytes = static_cast<size_t>(glyph.rowStride) * glyph.height;
            const size_t storedBytes = RFont4::bitmapBytes(glyph);
            TEST_ASSERT_TRUE(static_cast<uint64_t>(glyph.bitmapOffset) + storedBytes <= strike.bitmapSize);
            TEST_ASSERT_EQUAL_UINT8((glyph.width * strike.bitsPerPixel + 7U) / 8U, glyph.rowStride);
            if (RFont4::bitmapStoredRaw(strike, glyph)) {
                TEST_ASSERT_EQUAL_UINT32(decodedBytes, storedBytes);
            } else {
                std::vector<uint8_t> decoded(decodedBytes);
                const auto encoded =
                    std::span<const uint8_t>{bytes}.subspan(strike.bitmapOffset + glyph.bitmapOffset, storedBytes);
                TEST_ASSERT_TRUE(RFont4::decompressLz4Block(encoded, decoded));
            }
            TEST_ASSERT_TRUE(static_cast<uint64_t>(glyph.kernOffset) + glyph.kernCount <= strike.kerningPairCount);
            const std::string_view storedBitmap{reinterpret_cast<const char*>(bytes.data() + strike.bitmapOffset
                                                                              + glyph.bitmapOffset),
                                                storedBytes};
            const auto [bitmap, insertedBitmap] = bitmapOffsets.try_emplace(storedBitmap, glyph.bitmapOffset);
            TEST_ASSERT_TRUE(insertedBitmap || bitmap->second == glyph.bitmapOffset);
            const std::string_view storedKerning{reinterpret_cast<const char*>(bytes.data() + strike.kerningOffset
                                                                               + glyph.kernOffset
                                                                                     * sizeof(RFont4::KerningRecord)),
                                                 static_cast<size_t>(glyph.kernCount) * sizeof(RFont4::KerningRecord)};
            const auto [kerning, insertedKerning] = kerningOffsets.try_emplace(storedKerning, glyph.kernOffset);
            TEST_ASSERT_TRUE(insertedKerning || kerning->second == glyph.kernOffset);

            uint32_t previousRight = 0;
            for (uint32_t index = 0; index < glyph.kernCount; ++index) {
                const auto pair =
                    recordAt<RFont4::KerningRecord>(bytes, strike.kerningOffset, glyph.kernOffset + index);
                TEST_ASSERT_TRUE(index == 0 || pair.rightCodepoint > previousRight);
                previousRight = pair.rightCodepoint;
            }
        }
    }

    void validateFont(const std::filesystem::path& path) {
        const auto bytes = readFont(path);
        TEST_ASSERT_GREATER_OR_EQUAL(sizeof(RFont4::Header), bytes.size());
        const auto header = recordAt<RFont4::Header>(bytes, 0);
        const auto strikes = readStrikes(bytes, header);
        const auto tables = readTables(bytes, header);
        TEST_ASSERT_TRUE(RFont4::layoutValid(header, strikes, std::span{tables}.first(header.layoutTableCount),
                                             bytes.size()));
        TEST_ASSERT_EQUAL_UINT8('\0', bytes[header.nameOffset + header.nameSize - 1]);
        const uint32_t scriptMask = validateFamilyLookup(bytes, header);
        TEST_ASSERT_BITS_HIGH(header.scriptMask & ~UnicodeText::ScriptMath, scriptMask);
        if ((header.scriptMask & UnicodeText::ScriptMath) != 0)
            TEST_ASSERT_BITS_HIGH(UnicodeText::ScriptMath, scriptMask);
        for (size_t index = 0; index < strikes.size(); ++index) {
            TEST_ASSERT_EQUAL_UINT8(index == RFont4::kCompactStrikeIndex ? 1 : 4, strikes[index].bitsPerPixel);
            TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(index == RFont4::kCompactStrikeIndex
                                                             ? RFont4::BitmapEncoding::raw
                                                             : RFont4::BitmapEncoding::lz4),
                                    static_cast<uint8_t>(strikes[index].bitmapEncoding));
            validateStrike(bytes, strikes[index], header.glyphCount);
        }
        if (header.sourceGlyphCount == 0) {
            std::unordered_set<std::string> renderRecords;
            for (uint32_t glyphIndex = 0; glyphIndex < header.glyphCount; ++glyphIndex) {
                std::string key;
                key.reserve(strikes.size() * sizeof(RFont4::GlyphRecord));
                for (const auto& strike: strikes) {
                    const auto* record = bytes.data() + strike.glyphsOffset + glyphIndex * sizeof(RFont4::GlyphRecord);
                    key.append(reinterpret_cast<const char*>(record), sizeof(RFont4::GlyphRecord));
                }
                TEST_ASSERT_TRUE_MESSAGE(renderRecords.insert(std::move(key)).second, path.string().c_str());
            }
        }
    }

    ui::fonts::AlphaFont residentStrike(const std::vector<uint8_t>& bytes, size_t strikeIndex) {
        const auto header = recordAt<RFont4::Header>(bytes, 0);
        const auto strikes = readStrikes(bytes, header);
        const auto& strike = strikes[strikeIndex];
        return {
            .name =
                std::string_view{reinterpret_cast<const char*>(bytes.data() + header.nameOffset), header.nameSize - 1U},
            .bitmap = bytes.data() + strike.bitmapOffset,
            .glyphs = reinterpret_cast<const ui::fonts::AlphaGlyph*>(bytes.data() + strike.glyphsOffset),
            .supplementary =
                reinterpret_cast<const RFont4::SupplementaryRecord*>(bytes.data() + header.supplementaryOffset),
            .verticalRules = reinterpret_cast<const RFont4::VerticalRule*>(bytes.data() + header.verticalRulesOffset),
            .glyphCount = header.glyphCount,
            .supplementaryCount = header.supplementaryCount,
            .verticalRuleCount = header.verticalRuleCount,
            .yAdvance = strike.yAdvance,
            .ascent = strike.ascent,
            .descent = strike.descent,
            .pageMap = bytes.data() + header.pageMapOffset,
            .pageTableCount = header.pageTableCount,
            .kerningPairs = reinterpret_cast<const ui::fonts::AlphaKerningPair*>(bytes.data() + strike.kerningOffset),
            .kerningPairCount = strike.kerningPairCount,
            .wordInkTop = strike.wordInkTop,
            .wordInkBottom = strike.wordInkBottom,
            .scriptMask = header.scriptMask,
            .pixelsPerEm = strike.pixelsPerEm,
            .bitsPerPixel = strike.bitsPerPixel,
            .bitmapEncoding = strike.bitmapEncoding,
            .pageTableData = bytes.data() + header.pageTablesOffset,
        };
    }

    void test_file_cache_coalesces_small_reads_into_blocks() {
        std::string bytes(2048, '\0');
        for (size_t index = 0; index < bytes.size(); ++index)
            bytes[index] = static_cast<char>(index);
        File file{bytes};
        ui::fonts::RFontFileCache cache;
        std::array<uint8_t, 600> output{};

        TEST_ASSERT_TRUE(cache.read(file, bytes.size(), 100, output.data(), 24));
        TEST_ASSERT_EQUAL_MEMORY(bytes.data() + 100, output.data(), 24);
        TEST_ASSERT_TRUE(cache.read(file, bytes.size(), 200, output.data(), 24));
        TEST_ASSERT_TRUE(cache.read(file, bytes.size(), 600, output.data(), 24));
        TEST_ASSERT_TRUE(cache.read(file, bytes.size(), 120, output.data(), output.size()));
        TEST_ASSERT_EQUAL_MEMORY(bytes.data() + 120, output.data(), output.size());
        TEST_ASSERT_EQUAL_UINT32(1, file.readCount());

        File other{std::string(2048, static_cast<char>(0xA5))};
        TEST_ASSERT_TRUE(cache.read(other, 2048, 100, output.data(), 24));
        TEST_ASSERT_EQUAL_MEMORY(std::string(24, static_cast<char>(0xA5)).data(), output.data(), 24);
        TEST_ASSERT_EQUAL_UINT32(1, other.readCount());
    }

    void test_file_cache_prefetch_respects_the_read_budget() {
        constexpr size_t blockSize = ui::fonts::RFontFileCache::kBlockSize;
        File file{std::string(blockSize * 3, static_cast<char>(0x5A))};
        ui::fonts::RFontFileCache cache;
        size_t remainingReads = 1;

        TEST_ASSERT_TRUE(cache.prefetch(file, file.size(), 100, blockSize * 2, remainingReads));
        TEST_ASSERT_EQUAL_UINT32(1, file.readCount());
        TEST_ASSERT_EQUAL_UINT32(0, remainingReads);

        uint8_t byte = 0;
        TEST_ASSERT_TRUE(cache.read(file, file.size(), 100, &byte, 1));
        TEST_ASSERT_EQUAL_HEX8(0x5A, byte);
        TEST_ASSERT_EQUAL_UINT32(1, file.readCount());
        TEST_ASSERT_TRUE(cache.read(file, file.size(), blockSize + 100, &byte, 1));
        TEST_ASSERT_EQUAL_UINT32(2, file.readCount());
    }

    void test_resident_metrics_avoid_file_reads_until_bitmap_rendering() {
        const std::array glyphs{
            ui::fonts::AlphaGlyph{.width = 3, .height = 1, .rowStride = 1, .xAdvance = 4, .bitmapBytes = 1},
        };
        const std::array identities{ui::fonts::AlphaGlyphIdentity{'x', 7}};
        std::array<uint16_t, 8> glyphMap;
        glyphMap.fill(UINT16_MAX);
        glyphMap[7] = 0;
        std::array<uint8_t, RFont4::kPageMapBytes> pageMap;
        pageMap.fill(UINT8_MAX);
        pageMap[0] = 0;
        std::array<uint8_t, RFont4::kPageTableEntries * sizeof(uint16_t)> pageTable;
        pageTable.fill(UINT8_MAX);
        const uint16_t glyphIndex = 0;
        std::memcpy(pageTable.data() + static_cast<size_t>('x') * sizeof(glyphIndex), &glyphIndex, sizeof(glyphIndex));
        File file{std::string(1, static_cast<char>(0xA0))};
        const ui::fonts::AlphaFont font{
            .glyphs = glyphs.data(),
            .identities = identities.data(),
            .glyphCount = glyphs.size(),
            .yAdvance = 2,
            .ascent = 1,
            .pageMap = pageMap.data(),
            .pageTableCount = 1,
            .glyphMap = reinterpret_cast<const uint8_t*>(glyphMap.data()),
            .glyphMapCount = glyphMap.size(),
            .pixelsPerEm = 2,
            .file = &file,
            .fileSize = 1,
            .fileStrike = {.bitmapSize = 1},
            .bitsPerPixel = 1,
            .pageTableData = pageTable.data(),
        };
        Arduino_GFX gfx{8, 4};
        ui::fonts::AlphaTextRenderer<8> renderer{gfx};
        TEST_ASSERT_TRUE(renderer.begin());
        renderer.setFont(font);

        uint32_t resolvedGlyphId = 0;
        uint16_t resolvedGlyphIndex = UINT16_MAX;
        TEST_ASSERT_TRUE(renderer.nominalGlyph('x', resolvedGlyphId));
        TEST_ASSERT_EQUAL_UINT32(7, resolvedGlyphId);
        TEST_ASSERT_TRUE(renderer.resolveGlyphId(7, resolvedGlyphIndex));
        TEST_ASSERT_EQUAL_UINT16(0, resolvedGlyphIndex);
        TEST_ASSERT_EQUAL_UINT32(0, file.readCount());
        TEST_ASSERT_EQUAL_INT16(4, renderer.drawCodepoint('x', 0, 1));
        TEST_ASSERT_EQUAL_UINT32(1, file.readCount());
        TEST_ASSERT_EQUAL_INT16(4, renderer.drawString("x", 0, 1));
        TEST_ASSERT_EQUAL_INT16(4, renderer.drawString("x", 0, 1));
        TEST_ASSERT_EQUAL_UINT32(2, file.readCount());

        Arduino_GFX strip{8, 2};
        const size_t originalWrites = gfx.bitmapWrites;
        TEST_ASSERT_EQUAL_PTR(&gfx, &renderer.setOutput(strip));
        TEST_ASSERT_EQUAL_INT16(4, renderer.drawString("x", 1, 0));
        TEST_ASSERT_GREATER_THAN(0, strip.bitmapWrites);
        TEST_ASSERT_EQUAL(originalWrites, gfx.bitmapWrites);
        TEST_ASSERT_EQUAL_UINT32(2, file.readCount());
        TEST_ASSERT_EQUAL_PTR(&strip, &renderer.setOutput(gfx));
        TEST_ASSERT_EQUAL_INT16(4, renderer.drawString("x", 1, 1));
        TEST_ASSERT_GREATER_THAN(originalWrites, gfx.bitmapWrites);
        TEST_ASSERT_EQUAL_UINT32(2, file.readCount());
    }

    void test_lz4_block_decoder_checks_bounds_and_overlap() {
        constexpr std::array<uint8_t, 6> encoded{0x32, 'a', 'b', 'c', 0x03, 0x00};
        std::array<uint8_t, 9> decoded{};
        TEST_ASSERT_TRUE(RFont4::decompressLz4Block(encoded, decoded));
        TEST_ASSERT_EQUAL_STRING_LEN("abcabcabc", reinterpret_cast<const char*>(decoded.data()), decoded.size());

        constexpr std::array<uint8_t, 3> invalidOffset{0x00, 0x00, 0x00};
        TEST_ASSERT_FALSE(RFont4::decompressLz4Block(invalidOffset, decoded));
        TEST_ASSERT_FALSE(RFont4::decompressLz4Block(encoded, std::span<uint8_t>{decoded}.first(8)));
    }

    void test_oversized_compressed_run_reads_each_glyph_once() {
        constexpr size_t glyphCount = 9;
        constexpr size_t glyphBytes = 1024;
        constexpr std::array<uint8_t, 8> encoded{0x1F, 0xFF, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xEF};
        std::string fileBytes;
        fileBytes.reserve(glyphCount * encoded.size());
        std::array<ui::fonts::AlphaGlyph, glyphCount> glyphs{};
        std::array<ui::fonts::AlphaGlyphIdentity, glyphCount> identities{};
        std::array<uint8_t, RFont4::kPageMapBytes> pageMap;
        std::array<uint8_t, RFont4::kPageTableEntries * sizeof(uint16_t)> pageTable;
        pageMap.fill(UINT8_MAX);
        pageMap[0] = 0;
        pageTable.fill(UINT8_MAX);
        std::string text;
        for (size_t index = 0; index < glyphCount; ++index) {
            fileBytes.append(reinterpret_cast<const char*>(encoded.data()), encoded.size());
            glyphs[index] = {.bitmapOffset = static_cast<uint32_t>(index * encoded.size()),
                             .width = 32,
                             .height = 64,
                             .rowStride = 16,
                             .xAdvance = 32,
                             .bitmapBytes = encoded.size()};
            const uint32_t codepoint = static_cast<uint32_t>('A' + index);
            identities[index] = {codepoint, static_cast<uint16_t>(index)};
            const uint16_t glyphIndex = static_cast<uint16_t>(index);
            std::memcpy(pageTable.data() + codepoint * sizeof(glyphIndex), &glyphIndex, sizeof(glyphIndex));
            text.push_back(static_cast<char>(codepoint));
        }

        File file{std::move(fileBytes)};
        const ui::fonts::AlphaFont font{
            .glyphs = glyphs.data(),
            .identities = identities.data(),
            .glyphCount = glyphs.size(),
            .yAdvance = 64,
            .ascent = 64,
            .pageMap = pageMap.data(),
            .pageTableCount = 1,
            .pixelsPerEm = 64,
            .file = &file,
            .fileSize = static_cast<uint32_t>(file.size()),
            .fileStrike = {.bitmapEncoding = RFont4::BitmapEncoding::lz4,
                           .bitmapSize = static_cast<uint32_t>(file.size())},
            .bitsPerPixel = 4,
            .bitmapEncoding = RFont4::BitmapEncoding::lz4,
            .pageTableData = pageTable.data(),
        };
        Arduino_GFX gfx{320, 80};
        ui::fonts::AlphaTextRenderer<320> renderer{gfx};
        TEST_ASSERT_TRUE(renderer.begin());
        renderer.setFont(font);
        renderer.setTextColor(0xFFFF, 0);

        TEST_ASSERT_EQUAL_INT16(glyphCount * 32, renderer.drawString(text, 0, 64));
        TEST_ASSERT_EQUAL_UINT32(glyphCount, file.readCount());

        Arduino_GFX clippedGfx{64, 80};
        ui::fonts::AlphaTextRenderer<64> clippedRenderer{clippedGfx};
        TEST_ASSERT_TRUE(clippedRenderer.begin());
        clippedRenderer.setFont(font);
        clippedRenderer.setTextColor(0xFFFF, 0);
        TEST_ASSERT_EQUAL_INT16(glyphCount * 32, clippedRenderer.drawString(text, -224, 64));
        TEST_ASSERT_EQUAL_UINT32(glyphCount + 2, file.readCount());
    }

} // namespace

void setUp() {}
void tearDown() {}

void test_all_rfont4_assets_are_fully_validated_off_device() {
    size_t count = 0;
    for (const auto& entry: std::filesystem::recursive_directory_iterator{"fonts"}) {
        if (entry.path().extension() != RFont4::kExtension)
            continue;
        validateFont(entry.path());
        ++count;
    }
    TEST_ASSERT_TRUE(count > 0);

    const auto bytes = readFont("fonts/Andika/font.rfont4");
    const auto header = recordAt<RFont4::Header>(bytes, 0);
    auto strikes = readStrikes(bytes, header);
    const auto tables = readTables(bytes, header);
    const auto tableSpan = std::span{tables}.first(header.layoutTableCount);
    TEST_ASSERT_FALSE(RFont4::layoutValid(header, strikes, tableSpan, bytes.size() - 1));
    ++strikes.front().bitmapOffset;
    TEST_ASSERT_FALSE(RFont4::layoutValid(header, strikes, tableSpan, bytes.size()));
    auto unknownScript = header;
    unknownScript.scriptMask |= 1UL << 31U;
    TEST_ASSERT_FALSE(RFont4::layoutValid(unknownScript, strikes, tableSpan, bytes.size()));

    const auto shapedBytes = readFont("fonts/Noto Serif Hebrew/font.rfont4");
    const auto shapedHeader = recordAt<RFont4::Header>(shapedBytes, 0);
    const auto shapedStrikes = readStrikes(shapedBytes, shapedHeader);
    auto shapedTables = readTables(shapedBytes, shapedHeader);
    const auto shapedTableSpan = std::span{shapedTables}.first(shapedHeader.layoutTableCount);
    TEST_ASSERT_GREATER_THAN(1, shapedTableSpan.size());
    shapedTables.front().tag = 0;
    TEST_ASSERT_FALSE(RFont4::layoutValid(shapedHeader, shapedStrikes, shapedTableSpan, shapedBytes.size()));
    shapedTables = readTables(shapedBytes, shapedHeader);
    shapedTables[1].tag = shapedTables[0].tag;
    TEST_ASSERT_FALSE(RFont4::layoutValid(shapedHeader, shapedStrikes, shapedTableSpan, shapedBytes.size()));
}

void test_shaper_reuses_rfont4_nominal_glyphs_and_advances() {
    RFont4::Header header{.unitsPerEm = 1000, .sourceGlyphCount = 4};
    const std::array tableBytes{uint8_t{0}, uint8_t{0}, uint8_t{0}, uint8_t{0}};
    RFont4::LayoutTableRecord table{.tag = HB_TAG('G', 'D', 'E', 'F'), .size = tableBytes.size()};
    std::string bytes(tableBytes.size(), '\0');
    File file{std::move(bytes)};

    const std::array glyphs{
        ui::fonts::AlphaGlyph{.xAdvance = 7},
        ui::fonts::AlphaGlyph{.xAdvance = 8},
    };
    const std::array identities{
        ui::fonts::AlphaGlyphIdentity{'A', 2},
        ui::fonts::AlphaGlyphIdentity{'B', 3},
    };
    const std::array<uint16_t, 4> glyphMap{UINT16_MAX, UINT16_MAX, 0, 1};
    const ui::fonts::AlphaFont font{
        .glyphs = glyphs.data(),
        .identities = identities.data(),
        .glyphCount = glyphs.size(),
        .yAdvance = 16,
        .glyphMap = reinterpret_cast<const uint8_t*>(glyphMap.data()),
        .glyphMapCount = glyphMap.size(),
        .pixelsPerEm = 16,
    };
    Arduino_GFX gfx;
    ui::fonts::AlphaTextRenderer<640> renderer{gfx};
    TEST_ASSERT_TRUE(renderer.begin());
    renderer.setFont(font);

    TextShaping::Shaper shaping;
    auto opened = shaping.open(file, header, std::span{&table, 1});
    TEST_ASSERT_TRUE_MESSAGE(opened.has_value(), opened ? "" : opened.error().c_str());
    std::vector<ui::fonts::PositionedGlyph> output;
    auto shaped = shaping.shape("xABy", 1, 2, false, "en", renderer, output);
    TEST_ASSERT_TRUE_MESSAGE(shaped.has_value(), shaped ? "" : shaped.error().c_str());
    TEST_ASSERT_EQUAL_INT16(15, *shaped);
    TEST_ASSERT_EQUAL_UINT32(2, output.size());
    TEST_ASSERT_EQUAL_UINT32(0, output[0].glyphIndex);
    TEST_ASSERT_EQUAL_UINT32(1, output[0].cluster);
    TEST_ASSERT_EQUAL_INT32(7, output[0].xAdvance);
    TEST_ASSERT_EQUAL_UINT32(1, output[1].glyphIndex);
    TEST_ASSERT_EQUAL_UINT32(2, output[1].cluster);
    TEST_ASSERT_EQUAL_INT32(8, output[1].xAdvance);

    shaped = shaping.shape("xABy", 1, 2, false, "en", renderer, output);
    TEST_ASSERT_TRUE_MESSAGE(shaped.has_value(), shaped ? "" : shaped.error().c_str());
    TEST_ASSERT_EQUAL_INT16(15, *shaped);
    TEST_ASSERT_EQUAL_UINT32(4, output.size());
}

void test_compact_strike_renders_one_bit_rows() {
    constexpr uint8_t bitmap[]{0xA0};
    constexpr ui::fonts::AlphaGlyph glyphs[]{
        {.width = 3, .height = 1, .rowStride = 1, .xAdvance = 4},
    };
    constexpr ui::fonts::AlphaGlyphIdentity identities[]{{'x', 0}};
    const ui::fonts::AlphaFont font{
        .bitmap = bitmap,
        .glyphs = glyphs,
        .identities = identities,
        .glyphCount = 1,
        .yAdvance = 2,
        .ascent = 1,
        .pixelsPerEm = 2,
        .bitsPerPixel = 1,
    };
    Arduino_GFX gfx{8, 4};
    ui::fonts::AlphaTextRenderer<8> renderer{gfx};
    TEST_ASSERT_TRUE(renderer.begin());
    renderer.setFont(font);
    renderer.setTextColor(0xFFFF, 0);

    TEST_ASSERT_EQUAL_INT16(4, renderer.drawCodepoint('x', 0, 1));
    TEST_ASSERT_EQUAL(2, gfx.writes);
}

class BitmapBoundsGfx final : public Arduino_GFX {
public:
    using Arduino_GFX::Arduino_GFX;

    void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t*, int16_t width, int16_t height) override {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max<int16_t>(maxX, x + width);
        maxY = std::max<int16_t>(maxY, y + height);
        Arduino_GFX::draw16bitRGBBitmap(x, y, nullptr, width, height);
    }

    void resetBounds() {
        minX = minY = INT16_MAX;
        maxX = maxY = INT16_MIN;
    }

    int16_t inkWidth() const {
        return static_cast<int16_t>(maxX - minX);
    }

    int16_t inkHeight() const {
        return static_cast<int16_t>(maxY - minY);
    }

    int16_t minX = INT16_MAX;
    int16_t minY = INT16_MAX;
    int16_t maxX = INT16_MIN;
    int16_t maxY = INT16_MIN;
};

void test_vertical_glyphs_are_counter_rotated_for_the_portrait_panel() {
    constexpr uint8_t bitmap[]{0x80, 0x80, 0xC0};
    constexpr ui::fonts::AlphaGlyph glyphs[]{
        {.bitmapOffset = 0, .width = 2, .height = 3, .rowStride = 1, .xAdvance = 4, .bitmapBytes = 3},
        {.bitmapOffset = 0, .width = 2, .height = 3, .rowStride = 1, .xAdvance = 4, .bitmapBytes = 3},
    };
    constexpr ui::fonts::AlphaGlyphIdentity identities[]{{'A', 0}, {0x65E5, 0}};
    constexpr RFont4::VerticalRule rules[]{{'A', RFont4::kRotateVerticalGlyph}};
    const ui::fonts::AlphaFont font{
        .bitmap = bitmap,
        .glyphs = glyphs,
        .identities = identities,
        .verticalRules = rules,
        .glyphCount = 2,
        .verticalRuleCount = 1,
        .yAdvance = 4,
        .ascent = 3,
        .pixelsPerEm = 4,
        .bitsPerPixel = 1,
    };
    BitmapBoundsGfx gfx{20, 20};
    ui::fonts::AlphaTextRenderer<20> renderer{gfx};
    TEST_ASSERT_TRUE(renderer.begin());
    renderer.setFont(font);
    renderer.setTextColor(0xFFFF, 0);

    ui::fonts::AlphaTextRenderer<20>::Bounds measured;
    TEST_ASSERT_TRUE(renderer.measureVertical("\xE6\x97\xA5", 4, 10, measured));
    TEST_ASSERT_EQUAL_INT16(4, measured.x1);
    TEST_ASSERT_EQUAL_INT16(9, measured.y1);
    TEST_ASSERT_EQUAL_UINT16(3, measured.w);
    TEST_ASSERT_EQUAL_UINT16(2, measured.h);
    TEST_ASSERT_FALSE(renderer.measureVertical("?", 4, 10, measured));
    const std::array missingGlyph{ui::fonts::PositionedGlyph{.glyphIndex = 2}};
    TEST_ASSERT_FALSE(renderer.measure(missingGlyph, 4, 10, measured));

    TEST_ASSERT_EQUAL_INT16(4, renderer.drawVerticalCodepoint(0x65E5, 4, 10));
    TEST_ASSERT_EQUAL_UINT8(2, renderer.verticalInkHeight("\xE6\x97\xA5"));
    TEST_ASSERT_EQUAL_INT16(3, gfx.inkWidth());
    TEST_ASSERT_EQUAL_INT16(2, gfx.inkHeight());

    gfx.resetBounds();
    TEST_ASSERT_EQUAL_INT16(4, renderer.drawVerticalCodepoint('A', 4, 10));
    TEST_ASSERT_EQUAL_UINT8(3, renderer.verticalInkHeight("A"));
    TEST_ASSERT_EQUAL_INT16(2, gfx.inkWidth());
    TEST_ASSERT_EQUAL_INT16(3, gfx.inkHeight());
}

void test_rotated_glyph_clips_source_rows_before_reading_them() {
    constexpr ui::fonts::AlphaGlyph glyphs[]{
        {.width = 8, .height = 12, .rowStride = 1, .xAdvance = 12, .bitmapBytes = 12},
    };
    constexpr ui::fonts::AlphaGlyphIdentity identities[]{{0x65E5, 0}};
    File file{std::string(12, static_cast<char>(0xFF))};
    const ui::fonts::AlphaFont font{
        .glyphs = glyphs,
        .identities = identities,
        .glyphCount = 1,
        .yAdvance = 12,
        .ascent = 12,
        .pixelsPerEm = 12,
        .file = &file,
        .fileSize = 12,
        .fileStrike = {.bitmapSize = 12},
        .bitsPerPixel = 1,
    };
    Arduino_Canvas canvas{2, 2, nullptr};
    ui::fonts::AlphaTextRenderer<20> renderer{canvas};
    TEST_ASSERT_TRUE(renderer.begin());
    renderer.setFont(font);
    renderer.setTextColor(0xFFFF, 0);
    TEST_ASSERT_EQUAL_INT16(12, renderer.drawVerticalCodepoint(0x65E5, -4, 4));
    TEST_ASSERT_EQUAL_UINT32(2, file.readCount());
    for (int pixel = 0; pixel < 4; ++pixel)
        TEST_ASSERT_EQUAL_HEX16(0xFFFF, canvas.getFramebuffer()[pixel]);
}

void test_palette_matches_rgb565_blending_for_every_coverage() {
    constexpr uint8_t bitmap[]{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    constexpr ui::fonts::AlphaGlyph glyphs[]{
        {.width = 16, .height = 1, .rowStride = 8, .xAdvance = 16, .bitmapBytes = 8},
    };
    constexpr ui::fonts::AlphaGlyphIdentity identities[]{{'x', 0}};
    const ui::fonts::AlphaFont font{
        .bitmap = bitmap,
        .glyphs = glyphs,
        .identities = identities,
        .glyphCount = 1,
        .yAdvance = 1,
        .ascent = 1,
        .pixelsPerEm = 1,
    };
    Arduino_Canvas canvas{16, 1, nullptr};
    ui::fonts::AlphaTextRenderer<20> renderer{canvas};
    TEST_ASSERT_TRUE(renderer.begin());
    renderer.setFont(font);
    for (const auto [fg, bg]: {std::pair<uint16_t, uint16_t>{0xFFFF, 0}, {0xF800, 0x07E0}, {0x001F, 0x8410}}) {
        canvas.fillScreen(bg);
        renderer.setTextColor(fg, bg);
        renderer.drawCodepoint('x', 0, 0);
        for (int coverage = 0; coverage < 16; ++coverage) {
            const auto channel = [&](int shift, int mask) {
                return ((((bg >> shift) & mask) * (15 - coverage) + ((fg >> shift) & mask) * coverage + 7) / 15)
                    << shift;
            };
            const uint16_t expected = channel(11, 31) | channel(5, 63) | channel(0, 31);
            TEST_ASSERT_EQUAL_HEX16(expected, canvas.getFramebuffer()[coverage]);
        }
    }
}

void test_vertical_cjk_fixtures_render_from_sparse_rules() {
    constexpr std::array fixtures{
        std::pair{"fonts/Noto Serif Japanese/font.rfont4", std::string_view{"日本語「縦書き。」"}},
        std::pair{"fonts/Noto Serif Simplified Chinese/font.rfont4", std::string_view{"中文《竖排。》"}},
    };
    for (const auto& [path, text]: fixtures) {
        const auto bytes = readFont(path);
        const auto header = recordAt<RFont4::Header>(bytes, 0);
        TEST_ASSERT_GREATER_THAN(0, header.verticalRuleCount);
        const auto font = residentStrike(bytes, RFont4::kCompactStrikeIndex);
        Arduino_GFX gfx{172, 96};
        ui::fonts::AlphaTextRenderer<172> renderer{gfx};
        TEST_ASSERT_TRUE(renderer.begin());
        renderer.setFont(font);
        renderer.setTextColor(0xFFFF, 0);

        int16_t x = 0;
        std::string_view remaining = text;
        uint32_t codepoint = 0;
        size_t count = 0;
        while (Utf8Text::next(remaining, codepoint)) {
            const int16_t advance = renderer.drawVerticalCodepoint(codepoint, x, 48);
            TEST_ASSERT_GREATER_THAN(0, advance);
            x = static_cast<int16_t>(x + advance);
            ++count;
        }
        TEST_ASSERT_GREATER_THAN(5, count);
        TEST_ASSERT_GREATER_THAN(0, gfx.bitmapWrites);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_file_cache_coalesces_small_reads_into_blocks);
    RUN_TEST(test_file_cache_prefetch_respects_the_read_budget);
    RUN_TEST(test_resident_metrics_avoid_file_reads_until_bitmap_rendering);
    RUN_TEST(test_lz4_block_decoder_checks_bounds_and_overlap);
    RUN_TEST(test_oversized_compressed_run_reads_each_glyph_once);
    RUN_TEST(test_all_rfont4_assets_are_fully_validated_off_device);
    RUN_TEST(test_shaper_reuses_rfont4_nominal_glyphs_and_advances);
    RUN_TEST(test_compact_strike_renders_one_bit_rows);
    RUN_TEST(test_vertical_glyphs_are_counter_rotated_for_the_portrait_panel);
    RUN_TEST(test_rotated_glyph_clips_source_rows_before_reading_them);
    RUN_TEST(test_palette_matches_rgb565_blending_for_every_coverage);
    RUN_TEST(test_vertical_cjk_fixtures_render_from_sparse_rules);
    return UNITY_END();
}
