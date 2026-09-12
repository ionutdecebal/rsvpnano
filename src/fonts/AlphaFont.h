#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <FS.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

#include "fonts/RFont4Format.h"
#include "fonts/UiFont6x9.h"
#include "text/Utf8Text.h"

namespace ui::fonts {

    inline constexpr uint16_t kMissingGlyphIndex = UINT16_MAX;
    inline constexpr uint8_t kMissingPageIndex = UINT8_MAX;

    struct RFontFileCache {
        static constexpr size_t kBlockSize = 4096;
#if defined(BOARD_HAS_PSRAM)
        static constexpr size_t kBlockCount = 20;
#else
        static constexpr size_t kBlockCount = 2;
#endif

        struct Stats {
            uint32_t logicalReads = 0;
            uint32_t blockReads = 0;
            uint32_t seeks = 0;
            uint32_t requestedBytes = 0;
            uint32_t loadedBytes = 0;
        };

        bool read(File& file, uint32_t fileSize, uint32_t offset, void* output, size_t size) {
#if defined(RSVP_BENCHMARK_MODE)
            ++stats_.logicalReads;
            stats_.requestedBytes += static_cast<uint32_t>(size);
#endif
            if (size > fileSize || offset > fileSize - size)
                return false;

            auto* destination = static_cast<uint8_t*>(output);
            while (size > 0) {
                const uint32_t blockOffset = offset & ~(static_cast<uint32_t>(kBlockSize) - 1U);
                Block* block = load(file, fileSize, blockOffset);
                if (block == nullptr)
                    return false;
                const size_t within = offset - blockOffset;
                const size_t chunk = std::min(size, static_cast<size_t>(block->size) - within);
                std::memcpy(destination, block->bytes.data() + within, chunk);
                destination += chunk;
                offset += static_cast<uint32_t>(chunk);
                size -= chunk;
            }
            return true;
        }

        bool prefetch(File& file, uint32_t fileSize, uint32_t offset, size_t size, size_t& remainingBlockReads) {
#if defined(RSVP_BENCHMARK_MODE)
            ++stats_.logicalReads;
            stats_.requestedBytes += static_cast<uint32_t>(size);
#endif
            if (size > fileSize || offset > fileSize - size)
                return false;

            while (size > 0) {
                const uint32_t blockOffset = offset & ~(static_cast<uint32_t>(kBlockSize) - 1U);
                Block* block = find(file, blockOffset);
                if (block == nullptr && remainingBlockReads > 0) {
                    block = load(file, fileSize, blockOffset);
                    if (block == nullptr)
                        return false;
                    --remainingBlockReads;
                } else if (block != nullptr) {
                    block->used = ++clock_;
                }

                const size_t within = offset - blockOffset;
                const size_t available = std::min<size_t>(kBlockSize, fileSize - blockOffset);
                const size_t chunk = std::min(size, available - within);
                offset += static_cast<uint32_t>(chunk);
                size -= chunk;
            }
            return true;
        }

        void clear() {
            for (Block& block: blocks_) {
                block.file = nullptr;
                block.offset = UINT32_MAX;
                block.used = 0;
            }
            clock_ = 0;
#if defined(RSVP_BENCHMARK_MODE)
            stats_ = {};
#endif
        }

#if defined(RSVP_BENCHMARK_MODE)
        void resetStats() {
            stats_ = {};
        }
        const Stats& stats() const {
            return stats_;
        }
#endif

    private:
        struct Block {
            alignas(4) std::array<uint8_t, kBlockSize> bytes{};
            const File* file = nullptr;
            uint32_t offset = UINT32_MAX;
            uint32_t used = 0;
            uint16_t size = 0;
        };

        Block* find(const File& file, uint32_t offset) {
            const auto found = std::ranges::find_if(blocks_, [&](const Block& block) {
                return block.file == &file && block.offset == offset;
            });
            return found == blocks_.end() ? nullptr : &*found;
        }

        Block& oldest() {
            return *std::ranges::min_element(blocks_, {}, &Block::used);
        }

        Block* load(File& file, uint32_t fileSize, uint32_t blockOffset) {
            if (Block* block = find(file, blockOffset)) {
                block->used = ++clock_;
                return block;
            }

            Block& block = oldest();
            const size_t available = std::min<size_t>(kBlockSize, fileSize - blockOffset);
            if ((file.position() != blockOffset && !seek(file, blockOffset))
                || file.read(block.bytes.data(), available) != available) {
                block.offset = UINT32_MAX;
                return nullptr;
            }
            block.file = &file;
            block.offset = blockOffset;
            block.used = ++clock_;
            block.size = static_cast<uint16_t>(available);
#if defined(RSVP_BENCHMARK_MODE)
            ++stats_.blockReads;
            stats_.loadedBytes += static_cast<uint32_t>(available);
#endif
            return &block;
        }

        bool seek(File& file, uint32_t offset) {
            if (!file.seek(offset))
                return false;
#if defined(RSVP_BENCHMARK_MODE)
            ++stats_.seeks;
#endif
            return true;
        }

        std::array<Block, kBlockCount> blocks_{};
        uint32_t clock_ = 0;
#if defined(RSVP_BENCHMARK_MODE)
        Stats stats_{};
#endif
    };

    using AlphaGlyph = RFont4::GlyphRecord;
    struct __attribute__((packed)) AlphaGlyphIdentity {
        uint32_t codepoint = 0;
        uint16_t glyphId = 0;
    };
    using AlphaKerningPair = RFont4::KerningRecord;

    struct PositionedGlyph {
        uint32_t cluster = 0;
        uint16_t glyphIndex = 0;
        int16_t xAdvance = 0;
        int16_t xOffset = 0;
        int16_t yOffset = 0;
    };
    static_assert(sizeof(PositionedGlyph) == 12);

    struct AlphaFont {
        std::string_view name;
        const uint8_t* bitmap = nullptr;
        const AlphaGlyph* glyphs = nullptr;
        const AlphaGlyphIdentity* identities = nullptr;
        const RFont4::SupplementaryRecord* supplementary = nullptr;
        const uint8_t* glyphIds = nullptr;
        const RFont4::VerticalRule* verticalRules = nullptr;
        uint32_t glyphCount = 0;
        uint16_t supplementaryCount = 0;
        uint16_t verticalRuleCount = 0;
        uint8_t yAdvance = 0;
        uint8_t ascent = 0;
        uint8_t descent = 0;
        const uint8_t* pageMap = nullptr;
        const uint16_t* const* pageTables = nullptr;
        uint32_t pageTableCount = 0;
        const AlphaKerningPair* kerningPairs = nullptr;
        uint32_t kerningPairCount = 0;
        int8_t wordInkTop = 0;
        int8_t wordInkBottom = -1;
        const uint8_t* glyphMap = nullptr;
        uint32_t glyphMapCount = 0;
        uint32_t scriptMask = 0;
        uint8_t pixelsPerEm = 0;
        File* file = nullptr;
        RFontFileCache* fileCache = nullptr;
        uint32_t fileSize = 0;
        uint32_t generation = 0;
        RFont4::Header fileHeader;
        RFont4::StrikeRecord fileStrike;
        uint8_t bitsPerPixel = 4;
        RFont4::BitmapEncoding bitmapEncoding = RFont4::BitmapEncoding::raw;
        const uint8_t* pageTableData = nullptr;
    };

    inline bool residentGlyphIndex(const AlphaFont& font, uint32_t codepoint, uint16_t& glyphIndex) {
        if (codepoint > UINT16_MAX || font.pageMap == nullptr || font.pageTableCount == 0
            || (font.pageTableData == nullptr && font.pageTables == nullptr))
            return false;

        const uint8_t pageIndex = pgm_read_byte(font.pageMap + (codepoint >> 8U));
        if (pageIndex == kMissingPageIndex || pageIndex >= font.pageTableCount) {
            glyphIndex = kMissingGlyphIndex;
            return true;
        }

        if (font.pageTableData != nullptr) {
            const size_t entry = static_cast<size_t>(pageIndex) * RFont4::kPageTableEntries + (codepoint & 0xFFU);
            std::memcpy(&glyphIndex, font.pageTableData + entry * sizeof(glyphIndex), sizeof(glyphIndex));
        } else {
            const auto* page = reinterpret_cast<const uint16_t*>(pgm_read_ptr(font.pageTables + pageIndex));
            glyphIndex = page == nullptr ? kMissingGlyphIndex : pgm_read_word(page + (codepoint & 0xFFU));
        }
        return true;
    }

    inline bool residentVerticalRule(const AlphaFont& font, uint32_t codepoint, RFont4::VerticalRule& rule) {
        if (font.verticalRules == nullptr)
            return false;
        uint16_t first = 0;
        uint16_t last = font.verticalRuleCount;
        while (first < last) {
            const uint16_t middle = static_cast<uint16_t>(first + (last - first) / 2);
            std::memcpy(&rule, font.verticalRules + middle, sizeof(rule));
            if (rule.codepoint < codepoint)
                first = static_cast<uint16_t>(middle + 1);
            else
                last = middle;
        }
        if (first >= font.verticalRuleCount)
            return false;
        std::memcpy(&rule, font.verticalRules + first, sizeof(rule));
        return rule.codepoint == codepoint;
    }

    inline size_t prefetchGlyphBitmaps(const AlphaFont& font, std::string_view text, size_t maxBlockReads,
                                       bool vertical = false) {
        if (maxBlockReads == 0 || font.bitmap != nullptr || font.glyphs == nullptr || !font.file || !font.fileCache)
            return 0;

        size_t remaining = maxBlockReads;
        std::string_view cursor = text;
        uint32_t codepoint = 0;
        while (Utf8Text::next(cursor, codepoint)) {
            uint16_t glyphIndex = kMissingGlyphIndex;
            if (!residentGlyphIndex(font, codepoint, glyphIndex) || glyphIndex == kMissingGlyphIndex
                || glyphIndex >= font.glyphCount)
                continue;

            RFont4::VerticalRule rule;
            if (vertical && residentVerticalRule(font, codepoint, rule)
                && rule.alternateIndex != RFont4::kRotateVerticalGlyph)
                glyphIndex = rule.alternateIndex;

            AlphaGlyph glyph;
            std::memcpy(&glyph, font.glyphs + glyphIndex, sizeof(glyph));
            const size_t bytes = RFont4::bitmapBytes(glyph);
            if (bytes != 0
                && !font.fileCache->prefetch(*font.file, font.fileSize,
                                             font.fileStrike.bitmapOffset + glyph.bitmapOffset, bytes, remaining))
                break;
        }
        return maxBlockReads - remaining;
    }

    struct AlphaByteInfo {
        uint8_t left = 0;
        uint8_t right = 0;
        bool hasInk = false;
        bool isSolid = false;
    };

    constexpr std::array<AlphaByteInfo, 256> makeAlphaByteInfoTable() {
        std::array<AlphaByteInfo, 256> table{};
        for (std::size_t i = 0; i < table.size(); ++i) {
            const auto packed = static_cast<uint8_t>(i);
            table[i] = AlphaByteInfo{
                static_cast<uint8_t>(packed >> 4U),
                static_cast<uint8_t>(packed & 0x0FU),
                packed != 0x00,
                packed == 0xFF,
            };
        }
        return table;
    }

    inline constexpr auto kAlphaByteInfo = makeAlphaByteInfoTable();

    template<int16_t MaxRowWidth, uint8_t MaxStripRows = 8>
    class AlphaTextRenderer {
        static_assert(MaxRowWidth > 0);
        static_assert(MaxStripRows > 0);

    public:
        struct Bounds {
            int16_t x1 = 0;
            int16_t y1 = 0;
            uint16_t w = 0;
            uint16_t h = 0;
            int16_t advance = 0;
        };

        explicit AlphaTextRenderer(Arduino_GFX& output) : output_(&output) {}

        Arduino_GFX& setOutput(Arduino_GFX& output) {
            Arduino_GFX& previous = *output_;
            output_ = &output;
            return previous;
        }

        AlphaTextRenderer(const AlphaTextRenderer&) = delete;
        AlphaTextRenderer& operator=(const AlphaTextRenderer&) = delete;

        bool begin() {
            ready_ = true;
            return ready_;
        }

        void setFont(const AlphaFont& font) {
            if (font_ == &font && fontGeneration_ == font.generation)
                return;
            font_ = &font;
            fontGeneration_ = font.generation;
            for (auto& entry: fileGlyphCache_)
                entry.index = UINT32_MAX;
            fileKerningKeys_.fill(UINT64_MAX);
            preparedBitmapCount_ = 0;
            preparedBitmapBytes_ = 0;
        }

        void setTextColor(uint16_t fg, uint16_t bg) {
            if (fg_ == fg && bg_ == bg && blendTableValid_) {
                return;
            }

            fg_ = fg;
            bg_ = bg;
            rebuildBlendTable();
        }

        int16_t drawString(std::string_view text, int16_t x, int16_t baseline, int8_t tracking = 0) {
            if (!ready_ || font_ == nullptr)
                return -1;
            Bounds bounds;
            if (!measure(text, x, baseline, bounds, tracking))
                return drawMixedString(text, x, baseline, tracking);

            if (bounds.w == 0 || bounds.h == 0 || bounds.x1 >= output_->width() || bounds.y1 >= output_->height()
                || bounds.x1 + bounds.w <= 0 || bounds.y1 + bounds.h <= 0) {
                return bounds.advance;
            }

            if (!prepareVisibleBitmaps(text, x, baseline, tracking)
                || !drawGlyphsToStrips(text, x, baseline, bounds, tracking)) {
                drawGlyphs(text, x, baseline, tracking);
            }
            return bounds.advance;
        }

        int16_t drawCodepoint(uint32_t codepoint, int16_t x, int16_t baseline) {
            if (codepoint == '\n' || codepoint == '\r')
                return 0;
            const AlphaGlyph* glyph = findGlyph(codepoint);
            if (glyph == nullptr)
                return drawU8g2(codepoint, u8g2Style(), x, baseline);
            return drawAlpha(*glyph, x, baseline);
        }

        int16_t drawVerticalCodepoint(uint32_t codepoint, int16_t x, int16_t centerY) {
            uint16_t glyphIndex = 0;
            if (!findGlyphIndex(codepoint, glyphIndex))
                return 0;
            const AlphaGlyph* canonical = glyphAt(glyphIndex);
            if (canonical == nullptr)
                return 0;
            const int16_t advance = readGlyph(*canonical).xAdvance;
            return drawVerticalGlyph(glyphIndex, codepoint, advance, x, centerY);
        }

        int16_t drawVerticalGlyph(const PositionedGlyph& positioned, uint32_t codepoint, int16_t x, int16_t centerY) {
            return drawVerticalGlyph(positioned.glyphIndex, codepoint, positioned.xAdvance,
                                     static_cast<int16_t>(x + positioned.xOffset), centerY);
        }

        int16_t drawVerticalString(std::string_view text, int16_t x, int16_t centerY, int8_t tracking = 0) {
            constexpr size_t kBatchGlyphs = 4;
            const int16_t start = x;
            const int16_t margin = font_ == nullptr ? 0 : font_->yAdvance;
            while (!text.empty()) {
                std::string_view remaining = text;
                uint32_t codepoint = 0;
                Utf8Text::next(remaining, codepoint);
                const int16_t advance = glyphAdvance(codepoint);
                if (x + advance + margin <= 0) {
                    text = remaining;
                    x = static_cast<int16_t>(x + advance + (text.empty() ? 0 : tracking));
                    continue;
                }
                if (x - margin >= output_->width())
                    break;

                std::string_view batch = text;
                size_t bytes = 0;
                int16_t batchX = x;
                for (size_t count = 0; count < kBatchGlyphs && !batch.empty(); ++count) {
                    const size_t before = batch.size();
                    Utf8Text::next(batch, codepoint);
                    bytes += before - batch.size();
                    batchX = static_cast<int16_t>(batchX + glyphAdvance(codepoint) + (batch.empty() ? 0 : tracking));
                    if (batchX - margin >= output_->width())
                        break;
                }
                const std::string_view visible{text.data(), bytes};
                prepareVertical(visible);
                while (text.data() < visible.data() + visible.size()) {
                    Utf8Text::next(text, codepoint);
                    x = static_cast<int16_t>(x + drawVerticalCodepoint(codepoint, x, centerY)
                                             + (text.empty() ? 0 : tracking));
                }
            }
            return static_cast<int16_t>(x - start);
        }

    private:
        int16_t drawVerticalGlyph(uint16_t glyphIndex, uint32_t codepoint, int16_t advance, int16_t x,
                                  int16_t centerY) {
            bool sideways = false;
            glyphIndex = verticalGlyphIndex(glyphIndex, codepoint, sideways);
            const AlphaGlyph* glyph = glyphAt(glyphIndex);
            if (glyph == nullptr)
                return 0;
            const AlphaGlyph metrics = readGlyph(*glyph);
            const int16_t drawX = static_cast<int16_t>(x + (advance - (sideways ? metrics.width : metrics.height)) / 2);
            const int16_t drawY = static_cast<int16_t>(centerY - (sideways ? metrics.height : metrics.width) / 2);
            const int16_t drawW = sideways ? metrics.width : metrics.height;
            const int16_t drawH = sideways ? metrics.height : metrics.width;
            if (drawX >= output_->width() || drawX + drawW <= 0 || drawY >= output_->height() || drawY + drawH <= 0)
                return advance;
            if (sideways)
                drawGlyph(metrics, drawX, drawY);
            else
                drawCounterRotatedGlyph(metrics, drawX, drawY);
            return advance;
        }

    public:
        void prepare(std::string_view text) {
            if (ready_ && font_ != nullptr)
                prepareBitmaps(text);
        }

        void prepare(std::span<const PositionedGlyph> glyphs) {
            if (ready_ && font_ != nullptr)
                prepareBitmaps(glyphs);
        }

        void prepareVertical(std::string_view text) {
            if (ready_ && font_ != nullptr)
                prepareVerticalBitmaps(text);
        }

        int16_t glyphAdvance(uint32_t codepoint) const {
            if (codepoint == '\n' || codepoint == '\r')
                return 0;
            const AlphaGlyph* glyph = findGlyph(codepoint);
            if (glyph != nullptr)
                return readGlyph(*glyph).xAdvance;
            const U8g2Style fallback = u8g2Style();
            return static_cast<int16_t>(fallback.cellWidth * fallback.scale);
        }

        int16_t drawGlyphIndex(uint32_t glyphIndex, int16_t x, int16_t baseline) {
            const AlphaGlyph* glyph = glyphAt(glyphIndex);
            if (glyph == nullptr)
                return 0;
            const AlphaGlyph metrics = readGlyph(*glyph);
            if (metrics.width > 0 && metrics.height > 0)
                drawGlyph(metrics, static_cast<int16_t>(x + metrics.xOffset),
                          static_cast<int16_t>(baseline + metrics.yOffset));
            return metrics.xAdvance;
        }

        int16_t drawGlyphs(std::span<const PositionedGlyph> glyphs, int16_t x, int16_t baseline) {
            if (!ready_ || font_ == nullptr)
                return -1;
            Bounds bounds;
            measure(glyphs, x, baseline, bounds);
            if (bounds.w == 0 || bounds.h == 0 || bounds.x1 >= output_->width() || bounds.y1 >= output_->height()
                || bounds.x1 + bounds.w <= 0 || bounds.y1 + bounds.h <= 0)
                return bounds.advance;
            if (!prepareVisibleBitmaps(glyphs, x, baseline) || !drawGlyphsToStrips(glyphs, x, baseline, bounds)) {
                int16_t cursor = x;
                for (const PositionedGlyph& positioned: glyphs) {
                    drawGlyphIndex(positioned.glyphIndex, static_cast<int16_t>(cursor + positioned.xOffset),
                                   static_cast<int16_t>(baseline - positioned.yOffset));
                    cursor = static_cast<int16_t>(cursor + positioned.xAdvance);
                }
            }
            return bounds.advance;
        }

#if defined(RSVP_BENCHMARK_MODE)
        void clearBitmapCache() {
            preparedBitmapCount_ = 0;
            preparedBitmapBytes_ = 0;
        }
#endif

        int16_t glyphIdAdvance(uint32_t glyphId) const {
            uint16_t glyphIndex = 0;
            const AlphaGlyph* glyph = resolveGlyphId(glyphId, glyphIndex) ? glyphAt(glyphIndex) : nullptr;
            return glyph == nullptr ? 0 : readGlyph(*glyph).xAdvance;
        }

        bool resolveGlyphId(uint32_t glyphId, uint16_t& glyphIndex) const {
            if (font_ == nullptr || glyphId >= font_->glyphMapCount || (font_->glyphMap == nullptr && !font_->file))
                return false;
            if (font_->glyphMap != nullptr)
                std::memcpy(&glyphIndex, font_->glyphMap + glyphId * sizeof(glyphIndex), sizeof(glyphIndex));
            else if (!readFile(font_->fileHeader.glyphMapOffset + glyphId * sizeof(glyphIndex), &glyphIndex,
                               sizeof(glyphIndex)))
                return false;
            return glyphIndex != kMissingGlyphIndex && glyphIndex < font_->glyphCount;
        }

        uint8_t pixelsPerEm() const {
            return font_ == nullptr ? 0 : font_->pixelsPerEm;
        }

        uint8_t verticalInkHeight(std::string_view text) const {
            uint8_t height = 0;
            uint32_t codepoint = 0;
            while (Utf8Text::next(text, codepoint)) {
                uint16_t glyphIndex = 0;
                if (!findGlyphIndex(codepoint, glyphIndex))
                    continue;
                bool sideways = false;
                const AlphaGlyph* glyph = glyphAt(verticalGlyphIndex(glyphIndex, codepoint, sideways));
                if (glyph != nullptr) {
                    const AlphaGlyph metrics = readGlyph(*glyph);
                    height = std::max(height, static_cast<uint8_t>(sideways ? metrics.height : metrics.width));
                }
            }
            return height;
        }

        bool nominalGlyph(uint32_t codepoint, uint32_t& glyphId) const {
            uint16_t glyphIndex = 0;
            if (!findGlyphIndex(codepoint, glyphIndex))
                return false;
            uint16_t sourceGlyph = 0;
            if (font_->glyphIds != nullptr)
                std::memcpy(&sourceGlyph, font_->glyphIds + glyphIndex * sizeof(sourceGlyph), sizeof(sourceGlyph));
            else if (font_->identities != nullptr) {
                AlphaGlyphIdentity identity;
                if (!identityAt(glyphIndex, identity))
                    return false;
                sourceGlyph = identity.glyphId;
            } else if (font_->file && font_->fileHeader.sourceGlyphCount != 0) {
                if (!readFile(font_->fileHeader.glyphIdsOffset + glyphIndex * sizeof(sourceGlyph), &sourceGlyph,
                              sizeof(sourceGlyph)))
                    return false;
            } else {
                return false;
            }
            glyphId = sourceGlyph;
            return glyphId != 0;
        }

        bool hasGlyph(uint32_t codepoint) const {
            return findGlyph(codepoint) != nullptr;
        }

        int16_t kerningAdjust(uint32_t leftCodepoint, uint32_t rightCodepoint) const {
            if (font_ == nullptr || font_->kerningPairCount == 0 || (font_->kerningPairs == nullptr && !font_->file)) {
                return 0;
            }

            const AlphaGlyph* leftGlyph = findGlyph(leftCodepoint);
            if (leftGlyph == nullptr) {
                return 0;
            }

            const AlphaGlyph left = readGlyph(*leftGlyph);
            if (left.kernCount == 0) {
                return 0;
            }

            if (font_->kerningPairs == nullptr)
                return fileKerningAdjust(left, leftCodepoint, rightCodepoint);

            const std::span pairs{font_->kerningPairs + left.kernOffset, static_cast<size_t>(left.kernCount)};
            const auto pair =
                std::ranges::lower_bound(pairs, rightCodepoint, {}, [](const AlphaKerningPair& candidate) {
                    return readPacked(candidate).rightCodepoint;
                });
            if (pair == pairs.end())
                return 0;
            const AlphaKerningPair resolved = readPacked(*pair);
            return resolved.rightCodepoint == rightCodepoint ? resolved.xAdjust : 0;
        }

        int16_t textAdvance(std::string_view text, int8_t tracking = 0) const {
            if (font_ == nullptr)
                return 0;
            return measureAdvance(text, tracking);
        }

    private:
        struct FileGlyphCacheEntry {
            uint32_t index = UINT32_MAX;
            AlphaGlyph glyph;
        };

        struct PreparedBitmap {
            uint32_t fontOffset = 0;
            uint16_t bufferOffset = 0;
            uint16_t size = 0;
            uint16_t storageBytes = 0;
            bool storedRaw = false;
        };

        struct U8g2Style {
            const uint8_t* font = nullptr;
            uint8_t scale = 1;
            uint8_t cellWidth = 6;
        };

        U8g2Style u8g2Style() const {
            constexpr uint8_t builtInCellWidth = 6;
            constexpr uint8_t builtInHeight = 9;
            const uint8_t targetHeight = font_ != nullptr ? font_->yAdvance : builtInHeight;
            return {u8g2_font_rsvpnano_ui_6x9_tf, static_cast<uint8_t>(std::max<int>(1, targetHeight / builtInHeight)),
                    builtInCellWidth};
        }

        int16_t drawU8g2(std::string_view text, size_t codepoints, const U8g2Style& style, int16_t x,
                         int16_t baseline) {
            output_->setFont(style.font);
            output_->setUTF8Print(true);
            output_->setTextSize(style.scale);
            output_->setTextWrap(false);
            output_->setTextColor(fg_, bg_);
            output_->setCursor(x, baseline);
            for (const unsigned char byte: text)
                output_->write(byte);
            return static_cast<int16_t>(codepoints * style.cellWidth * style.scale);
        }

        int16_t drawU8g2(uint32_t codepoint, const U8g2Style& style, int16_t x, int16_t baseline) {
            std::array<char, 4> encoded{};
            const size_t size = Utf8Text::encode(codepoint, encoded);
            return drawU8g2({encoded.data(), size}, 1, style, x, baseline);
        }

        int16_t drawAlpha(const AlphaGlyph& glyph, int16_t x, int16_t baseline) {
            const AlphaGlyph metrics = readGlyph(glyph);
            if (metrics.width > 0 && metrics.height > 0)
                drawGlyph(metrics, static_cast<int16_t>(x + metrics.xOffset),
                          static_cast<int16_t>(baseline + metrics.yOffset));
            return metrics.xAdvance;
        }

        int16_t measureAdvance(std::string_view text, int8_t tracking) const {
            int16_t advance = 0;
            uint32_t previous = 0;
            bool previousWasAlpha = false;
            const U8g2Style fallback = u8g2Style();
            while (!text.empty()) {
                uint32_t codepoint = 0;
                Utf8Text::next(text, codepoint);
                const AlphaGlyph* glyph = findGlyph(codepoint);
                const bool currentIsAlpha = glyph != nullptr;
                if (currentIsAlpha && previousWasAlpha)
                    advance = static_cast<int16_t>(advance + kerningAdjust(previous, codepoint));
                int16_t glyphWidth = 0;
                if (glyph != nullptr)
                    glyphWidth = readGlyph(*glyph).xAdvance;
                else if (codepoint != '\n' && codepoint != '\r')
                    glyphWidth = static_cast<int16_t>(fallback.cellWidth * fallback.scale);
                advance = static_cast<int16_t>(advance + glyphWidth + (text.empty() ? 0 : tracking));
                previous = codepoint;
                previousWasAlpha = currentIsAlpha;
            }
            return advance;
        }

        int16_t drawMixedString(std::string_view text, int16_t x, int16_t baseline, int8_t tracking) {
            const int16_t start = x;
            uint32_t previous = 0;
            bool previousWasAlpha = false;
            const U8g2Style fallback = u8g2Style();
            while (!text.empty()) {
                const char* runStart = text.data();
                uint32_t codepoint = 0;
                Utf8Text::next(text, codepoint);
                const AlphaGlyph* glyph = findGlyph(codepoint);
                const bool currentIsAlpha = glyph != nullptr;
                if (currentIsAlpha && previousWasAlpha)
                    x = static_cast<int16_t>(x + kerningAdjust(previous, codepoint));
                if (glyph != nullptr) {
                    x = static_cast<int16_t>(x + drawAlpha(*glyph, x, baseline));
                } else if (codepoint != '\n' && codepoint != '\r') {
                    size_t codepoints = 1;
                    if (tracking == 0) {
                        while (!text.empty()) {
                            std::string_view next = text;
                            uint32_t nextCodepoint = 0;
                            Utf8Text::next(next, nextCodepoint);
                            if (nextCodepoint == '\n' || nextCodepoint == '\r' || findGlyph(nextCodepoint) != nullptr)
                                break;
                            text = next;
                            ++codepoints;
                        }
                    }
                    x = static_cast<int16_t>(x
                                             + drawU8g2({runStart, static_cast<size_t>(text.data() - runStart)},
                                                        codepoints, fallback, x, baseline));
                }
                if (!text.empty())
                    x = static_cast<int16_t>(x + tracking);
                previous = codepoint;
                previousWasAlpha = currentIsAlpha;
            }
            return static_cast<int16_t>(x - start);
        }

    public:
        bool measure(std::string_view text, int16_t x, int16_t baseline, Bounds& bounds, int8_t tracking = 0) const {
            if (!ready_ || font_ == nullptr) {
                bounds = {};
                return false;
            }

            int16_t cursorX = x;
            int16_t minX = INT16_MAX;
            int16_t minY = INT16_MAX;
            int16_t maxX = INT16_MIN;
            int16_t maxY = INT16_MIN;

            std::string_view cursor = text;
            uint32_t previousCodepoint = 0;
            bool hasPrevious = false;
            uint32_t codepoint = 0;
            while (Utf8Text::next(cursor, codepoint)) {
                if (hasPrevious) {
                    cursorX = static_cast<int16_t>(cursorX + kerningAdjust(previousCodepoint, codepoint));
                }

                if (codepoint == '\n' || codepoint == '\r') {
                    if (!cursor.empty())
                        cursorX = static_cast<int16_t>(cursorX + tracking);
                    previousCodepoint = codepoint;
                    hasPrevious = true;
                    continue;
                }
                const AlphaGlyph* glyph = findGlyph(codepoint);
                if (glyph == nullptr)
                    return false;

                const AlphaGlyph metrics = readGlyph(*glyph);
                if (metrics.width > 0 && metrics.height > 0) {
                    const int16_t x1 = static_cast<int16_t>(cursorX + metrics.xOffset);
                    const int16_t y1 = static_cast<int16_t>(baseline + metrics.yOffset);
                    const int16_t x2 = static_cast<int16_t>(x1 + metrics.width - 1);
                    const int16_t y2 = static_cast<int16_t>(y1 + metrics.height - 1);

                    minX = std::min(minX, x1);
                    minY = std::min(minY, y1);
                    maxX = std::max(maxX, x2);
                    maxY = std::max(maxY, y2);
                }

                cursorX = static_cast<int16_t>(cursorX + metrics.xAdvance);
                if (!cursor.empty())
                    cursorX = static_cast<int16_t>(cursorX + tracking);
                previousCodepoint = codepoint;
                hasPrevious = true;
            }

            bounds.x1 = x;
            bounds.y1 = baseline;
            bounds.w = 0;
            bounds.h = 0;
            bounds.advance = static_cast<int16_t>(cursorX - x);

            if (maxX >= minX) {
                bounds.x1 = minX;
                bounds.w = static_cast<uint16_t>(maxX - minX + 1);
            }
            if (maxY >= minY) {
                bounds.y1 = minY;
                bounds.h = static_cast<uint16_t>(maxY - minY + 1);
            }

            return true;
        }

        bool measure(std::span<const PositionedGlyph> glyphs, int16_t x, int16_t baseline, Bounds& bounds) const {
            bool complete = true;
            int16_t cursorX = x;
            int16_t minX = INT16_MAX;
            int16_t minY = INT16_MAX;
            int16_t maxX = INT16_MIN;
            int16_t maxY = INT16_MIN;
            for (const PositionedGlyph& positioned: glyphs) {
                const AlphaGlyph* glyph = glyphAt(positioned.glyphIndex);
                if (glyph != nullptr) {
                    const AlphaGlyph metrics = readGlyph(*glyph);
                    if (metrics.width > 0 && metrics.height > 0) {
                        const int16_t x1 = static_cast<int16_t>(cursorX + positioned.xOffset + metrics.xOffset);
                        const int16_t y1 = static_cast<int16_t>(baseline - positioned.yOffset + metrics.yOffset);
                        minX = std::min(minX, x1);
                        minY = std::min(minY, y1);
                        maxX = std::max<int16_t>(maxX, static_cast<int16_t>(x1 + metrics.width - 1));
                        maxY = std::max<int16_t>(maxY, static_cast<int16_t>(y1 + metrics.height - 1));
                    }
                } else
                    complete = false;
                cursorX = static_cast<int16_t>(cursorX + positioned.xAdvance);
            }
            bounds = {.x1 = minX,
                      .y1 = minY,
                      .w = maxX >= minX ? static_cast<uint16_t>(maxX - minX + 1) : uint16_t{0},
                      .h = maxY >= minY ? static_cast<uint16_t>(maxY - minY + 1) : uint16_t{0},
                      .advance = static_cast<int16_t>(cursorX - x)};
            return complete;
        }

        bool measureVertical(std::string_view text, int16_t x, int16_t centerY, Bounds& bounds) const {
            // A failed file-backed rule lookup is indistinguishable from "no alternate".
            bool complete = font_ != nullptr && (font_->verticalRuleCount == 0 || font_->verticalRules != nullptr);
            const int16_t start = x;
            int16_t minX = INT16_MAX, minY = INT16_MAX, maxX = INT16_MIN, maxY = INT16_MIN;
            uint32_t codepoint = 0;
            while (Utf8Text::next(text, codepoint)) {
                uint16_t index = 0;
                if (!findGlyphIndex(codepoint, index)) {
                    complete = false;
                    continue;
                }
                const AlphaGlyph* canonical = glyphAt(index);
                if (canonical == nullptr) {
                    complete = false;
                    continue;
                }
                const int16_t advance = readGlyph(*canonical).xAdvance;
                bool sideways = false;
                const AlphaGlyph* glyph = glyphAt(verticalGlyphIndex(index, codepoint, sideways));
                if (glyph == nullptr) {
                    complete = false;
                    continue;
                }
                const AlphaGlyph metrics = readGlyph(*glyph);
                const int16_t w = sideways ? metrics.width : metrics.height;
                const int16_t h = sideways ? metrics.height : metrics.width;
                if (w > 0 && h > 0) {
                    const int16_t left = static_cast<int16_t>(x + (advance - w) / 2);
                    const int16_t top = static_cast<int16_t>(centerY - h / 2);
                    minX = std::min(minX, left);
                    minY = std::min(minY, top);
                    maxX = std::max<int16_t>(maxX, left + w);
                    maxY = std::max<int16_t>(maxY, top + h);
                }
                x = static_cast<int16_t>(x + advance);
            }
            bounds = {.x1 = minX,
                      .y1 = minY,
                      .w = maxX > minX ? static_cast<uint16_t>(maxX - minX) : uint16_t{0},
                      .h = maxY > minY ? static_cast<uint16_t>(maxY - minY) : uint16_t{0},
                      .advance = static_cast<int16_t>(x - start)};
            return complete;
        }

    private:
        void drawGlyphs(std::string_view text, int16_t x, int16_t baseline, int8_t tracking) {
            if (font_ == nullptr) {
                return;
            }

            int16_t cursorX = x;
            std::string_view cursor = text;
            uint32_t previousCodepoint = 0;
            bool hasPrevious = false;
            uint32_t codepoint = 0;
            while (Utf8Text::next(cursor, codepoint)) {
                if (hasPrevious) {
                    cursorX = static_cast<int16_t>(cursorX + kerningAdjust(previousCodepoint, codepoint));
                }

                const AlphaGlyph* glyph = findGlyph(codepoint);
                if (glyph == nullptr) {
                    if (!cursor.empty())
                        cursorX = static_cast<int16_t>(cursorX + tracking);
                    previousCodepoint = codepoint;
                    hasPrevious = true;
                    continue;
                }

                const AlphaGlyph metrics = readGlyph(*glyph);
                if (metrics.width > 0 && metrics.height > 0) {
                    drawGlyph(metrics, static_cast<int16_t>(cursorX + metrics.xOffset),
                              static_cast<int16_t>(baseline + metrics.yOffset));
                }

                cursorX = static_cast<int16_t>(cursorX + metrics.xAdvance);
                if (!cursor.empty())
                    cursorX = static_cast<int16_t>(cursorX + tracking);
                previousCodepoint = codepoint;
                hasPrevious = true;
            }
        }

        bool drawGlyphsToStrips(std::string_view text, int16_t x, int16_t baseline, const Bounds& bounds,
                                int8_t tracking) {
            return drawToStrips(bounds, [&](int16_t stripX, int16_t stripY, uint16_t stripWidth, uint8_t stripRows) {
                compositeGlyphsIntoStrip(text, x, baseline, stripX, stripY, stripWidth, stripRows, tracking);
            });
        }

        bool drawGlyphsToStrips(std::span<const PositionedGlyph> glyphs, int16_t x, int16_t baseline,
                                const Bounds& bounds) {
            return drawToStrips(bounds, [&](int16_t stripX, int16_t stripY, uint16_t stripWidth, uint8_t stripRows) {
                compositeGlyphsIntoStrip(glyphs, x, baseline, stripX, stripY, stripWidth, stripRows);
            });
        }

        template<typename Composite>
        bool drawToStrips(const Bounds& bounds, Composite&& composite) {
            if (font_ == nullptr || bounds.w == 0 || bounds.h == 0)
                return false;
            const int16_t displayW = output_->width();
            const int16_t displayH = output_->height();
            const int16_t boundsX2 = static_cast<int16_t>(bounds.x1 + bounds.w);
            const int16_t boundsY2 = static_cast<int16_t>(bounds.y1 + bounds.h);
            const int16_t visibleX0 = std::max<int16_t>(bounds.x1, 0);
            const int16_t visibleX1 = std::min<int16_t>(boundsX2, displayW);
            const int16_t visibleY0 = std::max<int16_t>(bounds.y1, 0);
            const int16_t visibleY1 = std::min<int16_t>(boundsY2, displayH);

            if (visibleX1 <= visibleX0 || visibleY1 <= visibleY0) {
                return true;
            }

            const uint16_t stripWidth = static_cast<uint16_t>(visibleX1 - visibleX0);
            if (stripWidth > MaxRowWidth) {
                return false;
            }

            for (int16_t stripY = visibleY0; stripY < visibleY1;) {
                const uint8_t stripRows =
                    static_cast<uint8_t>(std::min<int16_t>(MaxStripRows, static_cast<int16_t>(visibleY1 - stripY)));

                clearStrip(stripWidth, stripRows);
                composite(visibleX0, stripY, stripWidth, stripRows);
                flushStrip(visibleX0, stripY, stripWidth, stripRows);

                stripY = static_cast<int16_t>(stripY + stripRows);
            }

            return true;
        }

        void clearStrip(uint16_t width, uint8_t rows) {
            for (uint8_t row = 0; row < rows; ++row) {
                std::ranges::fill_n(strip_[row], width, bg_);
                stripFirstInk_[row] = width;
                stripLastInk_[row] = 0;
            }
        }

        void compositeGlyphsIntoStrip(std::string_view text, int16_t x, int16_t baseline, int16_t stripX,
                                      int16_t stripY, uint16_t stripWidth, uint8_t stripRows, int8_t tracking) {
            int16_t cursorX = x;
            std::string_view cursor = text;
            uint32_t previousCodepoint = 0;
            bool hasPrevious = false;
            uint32_t codepoint = 0;

            while (Utf8Text::next(cursor, codepoint)) {
                if (hasPrevious) {
                    cursorX = static_cast<int16_t>(cursorX + kerningAdjust(previousCodepoint, codepoint));
                }

                const AlphaGlyph* glyph = findGlyph(codepoint);
                if (glyph != nullptr) {
                    const AlphaGlyph metrics = readGlyph(*glyph);
                    if (metrics.width > 0 && metrics.height > 0) {
                        compositeGlyphIntoStrip(metrics, static_cast<int16_t>(cursorX + metrics.xOffset),
                                                static_cast<int16_t>(baseline + metrics.yOffset), stripX, stripY,
                                                stripWidth, stripRows);
                    }
                    cursorX = static_cast<int16_t>(cursorX + metrics.xAdvance);
                }
                if (!cursor.empty())
                    cursorX = static_cast<int16_t>(cursorX + tracking);

                previousCodepoint = codepoint;
                hasPrevious = true;
            }
        }

        void compositeGlyphsIntoStrip(std::span<const PositionedGlyph> glyphs, int16_t x, int16_t baseline,
                                      int16_t stripX, int16_t stripY, uint16_t stripWidth, uint8_t stripRows) {
            int16_t cursorX = x;
            for (const PositionedGlyph& positioned: glyphs) {
                const AlphaGlyph* glyph = glyphAt(positioned.glyphIndex);
                if (glyph != nullptr) {
                    const AlphaGlyph metrics = readGlyph(*glyph);
                    compositeGlyphIntoStrip(metrics,
                                            static_cast<int16_t>(cursorX + positioned.xOffset + metrics.xOffset),
                                            static_cast<int16_t>(baseline - positioned.yOffset + metrics.yOffset),
                                            stripX, stripY, stripWidth, stripRows);
                }
                cursorX = static_cast<int16_t>(cursorX + positioned.xAdvance);
            }
        }

        void compositeGlyphIntoStrip(const AlphaGlyph& glyph, int16_t glyphX, int16_t glyphY, int16_t stripX,
                                     int16_t stripY, uint16_t stripWidth, uint8_t stripRows) {
            if (glyph.width == 0 || glyph.height == 0 || (!font_->file && font_->bitmap == nullptr)) {
                return;
            }

            const int16_t glyphX2 = static_cast<int16_t>(glyphX + glyph.width);
            const int16_t glyphY2 = static_cast<int16_t>(glyphY + glyph.height);
            const int16_t stripX2 = static_cast<int16_t>(stripX + stripWidth);
            const int16_t stripY2 = static_cast<int16_t>(stripY + stripRows);

            const int16_t overlapX0 = std::max<int16_t>(glyphX, stripX);
            const int16_t overlapX1 = std::min<int16_t>(glyphX2, stripX2);
            const int16_t overlapY0 = std::max<int16_t>(glyphY, stripY);
            const int16_t overlapY1 = std::min<int16_t>(glyphY2, stripY2);

            if (overlapX1 <= overlapX0 || overlapY1 <= overlapY0) {
                return;
            }

            const int16_t overlapSrcX0 = static_cast<int16_t>(overlapX0 - glyphX);
            const int16_t overlapSrcX1 = static_cast<int16_t>(overlapX1 - glyphX);

            for (int16_t dstY = overlapY0; dstY < overlapY1; ++dstY) {
                const uint8_t srcY = static_cast<uint8_t>(dstY - glyphY);
                const uint8_t stripRow = static_cast<uint8_t>(dstY - stripY);
                const uint8_t* packedRow = nullptr;
                if (!prepareRow(glyph, srcY, packedRow)) {
                    continue;
                }

                if (font_->bitsPerPixel == 1) {
                    forEachVisibleSpan(packedRow, overlapSrcX0, overlapSrcX1, [&](int16_t srcX0, int16_t srcX1) {
                        const uint16_t dstCol = static_cast<uint16_t>(glyphX + srcX0 - stripX);
                        const uint16_t count = static_cast<uint16_t>(srcX1 - srcX0);
                        compositePackedRowSpanIntoStrip(packedRow, srcX0, stripRow, dstCol, count);
                    });
                } else {
                    compositePackedRowSpanIntoStrip(packedRow, overlapSrcX0, stripRow,
                                                    static_cast<uint16_t>(overlapX0 - stripX),
                                                    static_cast<uint16_t>(overlapSrcX1 - overlapSrcX0));
                }
            }
        }

        void compositePackedRowSpanIntoStrip(const uint8_t* packedRow, int16_t srcX, uint8_t stripRow, uint16_t dstCol,
                                             uint16_t count) {
            if (font_->bitsPerPixel == 1) {
                std::ranges::fill_n(strip_[stripRow] + dstCol, count, fg_);
                includeInk(stripRow, dstCol, count);
                return;
            }
            uint16_t out = dstCol;
            uint16_t remaining = count;
            uint16_t firstInk = UINT16_MAX;
            uint16_t lastInk = 0;

            if ((srcX & 1) != 0 && remaining > 0) {
                const uint8_t coverage = coverageAt(packedRow, static_cast<uint8_t>(srcX));
                if (coverage != 0) {
                    strip_[stripRow][out] = blend_[coverage];
                    firstInk = out;
                    lastInk = static_cast<uint16_t>(out + 1);
                }
                ++srcX;
                ++out;
                --remaining;
            }

            while (remaining >= 2) {
                const uint8_t packed = pgm_read_byte(packedRow + (srcX >> 1));
                const AlphaByteInfo info = kAlphaByteInfo[packed];
                if (info.isSolid) {
                    strip_[stripRow][out] = fg_;
                    strip_[stripRow][out + 1] = fg_;
                    if (firstInk == UINT16_MAX)
                        firstInk = out;
                    lastInk = static_cast<uint16_t>(out + 2);
                } else if (info.hasInk) {
                    if (info.left != 0) {
                        strip_[stripRow][out] = blend_[packed >> 4U];
                        if (firstInk == UINT16_MAX)
                            firstInk = out;
                        lastInk = static_cast<uint16_t>(out + 1);
                    }
                    if (info.right != 0) {
                        strip_[stripRow][out + 1] = blend_[packed & 0x0FU];
                        if (firstInk == UINT16_MAX)
                            firstInk = static_cast<uint16_t>(out + 1);
                        lastInk = static_cast<uint16_t>(out + 2);
                    }
                }

                srcX = static_cast<int16_t>(srcX + 2);
                out = static_cast<uint16_t>(out + 2);
                remaining = static_cast<uint16_t>(remaining - 2);
            }

            if (remaining > 0) {
                const uint8_t coverage = coverageAt(packedRow, static_cast<uint8_t>(srcX));
                if (coverage != 0) {
                    strip_[stripRow][out] = blend_[coverage];
                    if (firstInk == UINT16_MAX)
                        firstInk = out;
                    lastInk = static_cast<uint16_t>(out + 1);
                }
            }
            if (firstInk != UINT16_MAX)
                includeInk(stripRow, firstInk, static_cast<uint16_t>(lastInk - firstInk));
        }

        void includeInk(uint8_t row, uint16_t first, uint16_t count) {
            stripFirstInk_[row] = std::min(stripFirstInk_[row], first);
            stripLastInk_[row] = std::max(stripLastInk_[row], static_cast<uint16_t>(first + count));
        }

        void flushStrip(int16_t stripX, int16_t stripY, uint16_t stripWidth, uint8_t stripRows) {
            for (uint8_t row = 0; row < stripRows; ++row) {
                const uint16_t first = stripFirstInk_[row];
                const uint16_t last = stripLastInk_[row];
                if (first >= last || last > stripWidth)
                    continue;

                // The strip already contains the union of every overlapping glyph. Sending its
                // opaque span once avoids a display transaction for every disconnected ink run.
                output_->draw16bitRGBBitmap(static_cast<int16_t>(stripX + first), static_cast<int16_t>(stripY + row),
                                            strip_[row] + first, static_cast<int16_t>(last - first), 1);
            }
        }

        void drawGlyph(const AlphaGlyph& glyph, int16_t x, int16_t y) {
            if (glyph.width == 0 || glyph.height == 0 || glyph.width > MaxRowWidth
                || (!font_->file && font_->bitmap == nullptr)) {
                return;
            }

            const int16_t displayW = output_->width();
            const int16_t displayH = output_->height();
            for (uint8_t row = 0; row < glyph.height; ++row) {
                drawPackedRow(glyph, row, x, static_cast<int16_t>(y + row), displayW, displayH);
            }
        }

        void drawCounterRotatedGlyph(const AlphaGlyph& glyph, int16_t x, int16_t y) {
            if (glyph.width == 0 || glyph.height == 0 || glyph.height > MaxRowWidth)
                return;
            const int16_t sourceY0 = std::max<int16_t>(0, -x);
            const int16_t sourceY1 = std::min<int16_t>(glyph.height, output_->width() - x);
            const int16_t sourceX0 = std::max<int16_t>(0, y + glyph.width - output_->height());
            const int16_t sourceX1 = std::min<int16_t>(glyph.width, y + glyph.width);
            if (sourceY1 <= sourceY0 || sourceX1 <= sourceX0)
                return;
            const int16_t width = sourceY1 - sourceY0;
            auto* rotated = strip_[0];
            for (uint8_t sourceStart = static_cast<uint8_t>(sourceX0); sourceStart < sourceX1;) {
                const uint8_t sourceEnd = static_cast<uint8_t>(std::min<int>(sourceX1, sourceStart + MaxStripRows));
                const uint8_t rows = static_cast<uint8_t>(sourceEnd - sourceStart);
                for (uint8_t sourceY = static_cast<uint8_t>(sourceY0); sourceY < sourceY1; ++sourceY) {
                    const uint8_t* packedRow = nullptr;
                    if (!prepareRow(glyph, sourceY, packedRow))
                        return;
                    for (uint8_t sourceX = sourceStart; sourceX < sourceEnd; ++sourceX)
                        rotated[static_cast<size_t>(sourceEnd - sourceX - 1) * width + sourceY - sourceY0] =
                            blend_[coverageAt(packedRow, sourceX)];
                }
                output_->draw16bitRGBBitmap(static_cast<int16_t>(x + sourceY0),
                                            static_cast<int16_t>(y + glyph.width - sourceEnd), rotated, width, rows);
                sourceStart = sourceEnd;
            }
        }

        void drawPackedRow(const AlphaGlyph& glyph, uint8_t row, int16_t dstX, int16_t dstY, int16_t displayW,
                           int16_t displayH) {
            if (dstY < 0 || dstY >= displayH) {
                return;
            }

            int16_t srcStart = 0;
            int16_t drawX = dstX;
            int16_t drawW = glyph.width;
            if (drawX < 0) {
                srcStart = static_cast<int16_t>(-drawX);
                drawW = static_cast<int16_t>(drawW - srcStart);
                drawX = 0;
            }
            if (drawX + drawW > displayW) {
                drawW = static_cast<int16_t>(displayW - drawX);
            }
            if (drawW <= 0) {
                return;
            }

            const int16_t srcEnd = static_cast<int16_t>(srcStart + drawW);
            const uint8_t* packedRow = nullptr;
            if (!prepareRow(glyph, row, packedRow)) {
                return;
            }

            forEachVisibleSpan(packedRow, srcStart, srcEnd, [&](int16_t clippedX0, int16_t clippedX1) {
                const int16_t spanWidth = static_cast<int16_t>(clippedX1 - clippedX0);
                if (spanWidth > MaxRowWidth) {
                    return;
                }
                renderSpan(packedRow, clippedX0, spanWidth, strip_[0]);
                output_->draw16bitRGBBitmap(static_cast<int16_t>(dstX + clippedX0), dstY, strip_[0], spanWidth, 1);
            });
        }

        void renderSpan(const uint8_t* packedRow, int16_t srcStart, int16_t spanWidth, uint16_t* output) {
            if (font_->bitsPerPixel == 1) {
                std::ranges::fill_n(output, spanWidth, fg_);
                return;
            }
            int16_t src = srcStart;
            int16_t out = 0;

            if ((src & 1) != 0 && out < spanWidth) {
                const uint8_t coverage = coverageAt(packedRow, static_cast<uint8_t>(src));
                output[out++] = blend_[coverage];
                ++src;
            }

            while (out + 1 < spanWidth) {
                const uint8_t packed = pgm_read_byte(packedRow + (src >> 1));
                const AlphaByteInfo info = kAlphaByteInfo[packed];
                if (info.isSolid) {
                    output[out] = fg_;
                    output[out + 1] = fg_;
                } else {
                    output[out] = blend_[packed >> 4U];
                    output[out + 1] = blend_[packed & 0x0FU];
                }
                src = static_cast<int16_t>(src + 2);
                out = static_cast<int16_t>(out + 2);
            }

            if (out < spanWidth) {
                const uint8_t coverage = coverageAt(packedRow, static_cast<uint8_t>(src));
                output[out] = blend_[coverage];
            }
        }

        uint8_t coverageAt(const uint8_t* packedRow, uint8_t x) const {
            if (font_->bitsPerPixel == 1)
                return (pgm_read_byte(packedRow + (x >> 3U)) & (0x80U >> (x & 7U))) != 0 ? 15 : 0;
            const uint8_t packed = pgm_read_byte(packedRow + (x >> 1U));
            return (x & 1U) == 0 ? static_cast<uint8_t>(packed >> 4U) : static_cast<uint8_t>(packed & 0x0FU);
        }

        AlphaGlyph readGlyph(const AlphaGlyph& glyph) const {
            return readPacked(glyph);
        }

        template<typename T>
        static T readPacked(const T& value) {
            T copy;
            std::memcpy(&copy, &value, sizeof(copy));
            return copy;
        }

        bool readFile(uint32_t offset, void* out, size_t bytes) const {
            if (font_ == nullptr || !font_->file)
                return false;
            File& file = *font_->file;
            if (font_->fileCache)
                return font_->fileCache->read(file, font_->fileSize, offset, out, bytes);
            return (file.position() == offset || file.seek(offset))
                && file.read(static_cast<uint8_t*>(out), bytes) == bytes;
        }

        const AlphaGlyph* fileGlyph(uint32_t index) const {
            if (font_ == nullptr || !font_->file || index >= font_->glyphCount)
                return nullptr;
            FileGlyphCacheEntry& cached = fileGlyphCache_[index % fileGlyphCache_.size()];
            if (cached.index == index)
                return &cached.glyph;

            RFont4::GlyphRecord record;
            const uint32_t offset = font_->fileStrike.glyphsOffset + index * sizeof(record);
            if (!readFile(offset, &record, sizeof(record)))
                return nullptr;
            const size_t decodedBytes = static_cast<size_t>(record.rowStride) * record.height;
            const size_t storedBytes = RFont4::bitmapBytes(record);
            if (static_cast<uint64_t>(record.bitmapOffset) + storedBytes > font_->fileStrike.bitmapSize
                || (RFont4::bitmapStoredRaw(font_->fileStrike, record) ? storedBytes != decodedBytes
                                                                       : decodedBytes != 0 && storedBytes == 0)
                || static_cast<uint64_t>(record.kernOffset) + record.kernCount > font_->fileStrike.kerningPairCount)
                return nullptr;

            cached = {.index = index, .glyph = record};
            return &cached.glyph;
        }

        const AlphaGlyph* glyphAt(uint32_t index) const {
            if (font_ == nullptr || index >= font_->glyphCount)
                return nullptr;
            return font_->glyphs != nullptr ? font_->glyphs + index : fileGlyph(index);
        }

        bool prepareRow(const AlphaGlyph& glyph, uint8_t row, const uint8_t*& packedRow) {
            if (row >= glyph.height || glyph.rowStride > packedRow_.size())
                return false;
            if (font_->bitmap != nullptr && font_->bitmapEncoding == RFont4::BitmapEncoding::raw) {
                packedRow = font_->bitmap + glyph.bitmapOffset + static_cast<uint32_t>(row) * glyph.rowStride;
                return true;
            }

            for (size_t index = 0; index < preparedBitmapCount_; ++index) {
                const PreparedBitmap& prepared = preparedBitmaps_[index];
                if (prepared.fontOffset == glyph.bitmapOffset) {
                    packedRow =
                        preparedBitmapData_.data() + prepared.bufferOffset + static_cast<size_t>(row) * glyph.rowStride;
                    return true;
                }
            }

            if (RFont4::bitmapStoredRaw(font_->fileStrike, glyph)) {
                const uint32_t bitmapOffset = glyph.bitmapOffset + static_cast<uint32_t>(row) * glyph.rowStride;
                if (glyph.rowStride > 0 && !readBitmap(bitmapOffset, packedRow_.data(), glyph.rowStride))
                    return false;
                packedRow = packedRow_.data();
                return true;
            }

            preparedBitmapCount_ = 0;
            preparedBitmapBytes_ = 0;
            size_t used = 0;
            if (!appendPreparedBitmap(glyph, used) || !loadPreparedBitmaps(0))
                return false;
            preparedBitmapBytes_ = used;
            packedRow = preparedBitmapData_.data() + static_cast<size_t>(row) * glyph.rowStride;
            return true;
        }

        bool appendPreparedBitmap(const AlphaGlyph& glyph, size_t& used) {
            const size_t bytes = static_cast<size_t>(glyph.rowStride) * glyph.height;
            if (bytes == 0)
                return true;
            for (size_t index = 0; index < preparedBitmapCount_; ++index) {
                if (preparedBitmaps_[index].fontOffset == glyph.bitmapOffset)
                    return true;
            }
            if (preparedBitmapCount_ == preparedBitmaps_.size() || bytes > preparedBitmapData_.size() - used)
                return false;
            preparedBitmaps_[preparedBitmapCount_++] = {
                .fontOffset = glyph.bitmapOffset,
                .bufferOffset = static_cast<uint16_t>(used),
                .size = static_cast<uint16_t>(bytes),
                .storageBytes = static_cast<uint16_t>(RFont4::bitmapBytes(glyph)),
                .storedRaw = RFont4::bitmapStoredRaw(font_->fileStrike, glyph),
            };
            used += bytes;
            return true;
        }

        bool loadPreparedBitmaps(size_t first) {
            std::sort(preparedBitmaps_.begin() + first, preparedBitmaps_.begin() + preparedBitmapCount_,
                      [](const PreparedBitmap& left, const PreparedBitmap& right) {
                          return left.fontOffset < right.fontOffset;
                      });
            for (size_t index = first; index < preparedBitmapCount_; ++index) {
                const PreparedBitmap& prepared = preparedBitmaps_[index];
                uint8_t* destination = preparedBitmapData_.data() + prepared.bufferOffset;
                if (prepared.storedRaw) {
                    if (prepared.storageBytes != prepared.size
                        || !readBitmap(prepared.fontOffset, destination, prepared.size))
                        return false;
                    continue;
                }
                if (prepared.storageBytes > encodedBitmap_.size()
                    || !readBitmap(prepared.fontOffset, encodedBitmap_.data(), prepared.storageBytes)
                    || !RFont4::decompressLz4Block(std::span<const uint8_t>{encodedBitmap_.data(),
                                                                            prepared.storageBytes},
                                                   std::span<uint8_t>{destination, prepared.size}))
                    return false;
            }
            return true;
        }

        bool readBitmap(uint32_t offset, void* output, size_t bytes) const {
            if (bytes > font_->fileStrike.bitmapSize || offset > font_->fileStrike.bitmapSize - bytes)
                return false;
            if (font_->bitmap != nullptr) {
                std::memcpy(output, font_->bitmap + offset, bytes);
                return true;
            }
            return readFile(font_->fileStrike.bitmapOffset + offset, output, bytes);
        }

        template<typename Append>
        bool prepareBitmaps(Append&& append) {
            if ((font_->bitmap != nullptr && font_->bitmapEncoding == RFont4::BitmapEncoding::raw)
                || (font_->bitmap == nullptr && !font_->file))
                return true;

            size_t first = preparedBitmapCount_;
            size_t baseBytes = preparedBitmapBytes_;
            size_t used = baseBytes;
            if (!append(used)) {
                preparedBitmapCount_ = 0;
                first = 0;
                baseBytes = 0;
                used = 0;
                if (!append(used)) {
                    preparedBitmapCount_ = 0;
                    preparedBitmapBytes_ = 0;
                    return false;
                }
            }
            if (!loadPreparedBitmaps(first)) {
                preparedBitmapCount_ = first;
                preparedBitmapBytes_ = baseBytes;
                return false;
            }
            preparedBitmapBytes_ = used;
            return true;
        }

        bool prepareBitmaps(std::string_view text) {
            return prepareBitmaps([&](size_t& used) {
                std::string_view cursor = text;
                uint32_t codepoint = 0;
                while (Utf8Text::next(cursor, codepoint)) {
                    const AlphaGlyph* glyph = findGlyph(codepoint);
                    if (glyph != nullptr && !appendPreparedBitmap(readGlyph(*glyph), used))
                        return false;
                }
                return true;
            });
        }

        bool prepareBitmaps(std::span<const PositionedGlyph> glyphs) {
            return prepareBitmaps([&](size_t& used) {
                for (const PositionedGlyph& positioned: glyphs) {
                    const AlphaGlyph* glyph = glyphAt(positioned.glyphIndex);
                    if (glyph != nullptr && !appendPreparedBitmap(readGlyph(*glyph), used))
                        return false;
                }
                return true;
            });
        }

        bool prepareVerticalBitmaps(std::string_view text) {
            return prepareBitmaps([&](size_t& used) {
                uint32_t codepoint = 0;
                while (Utf8Text::next(text, codepoint)) {
                    uint16_t glyphIndex = 0;
                    if (!findGlyphIndex(codepoint, glyphIndex))
                        continue;
                    bool sideways = false;
                    const AlphaGlyph* glyph = glyphAt(verticalGlyphIndex(glyphIndex, codepoint, sideways));
                    if (glyph != nullptr && !appendPreparedBitmap(readGlyph(*glyph), used))
                        return false;
                }
                return true;
            });
        }

        bool prepareVisibleBitmaps(std::string_view text, int16_t x, int16_t baseline, int8_t tracking) {
            return prepareBitmaps([&](size_t& used) {
                int16_t cursorX = x;
                std::string_view cursor = text;
                uint32_t previousCodepoint = 0;
                bool hasPrevious = false;
                uint32_t codepoint = 0;
                while (Utf8Text::next(cursor, codepoint)) {
                    if (hasPrevious)
                        cursorX = static_cast<int16_t>(cursorX + kerningAdjust(previousCodepoint, codepoint));

                    const AlphaGlyph* glyph = findGlyph(codepoint);
                    if (glyph != nullptr) {
                        const AlphaGlyph metrics = readGlyph(*glyph);
                        const int16_t glyphX = static_cast<int16_t>(cursorX + metrics.xOffset);
                        const int16_t glyphY = static_cast<int16_t>(baseline + metrics.yOffset);
                        if (glyphX < output_->width() && glyphX + metrics.width > 0 && glyphY < output_->height()
                            && glyphY + metrics.height > 0 && !appendPreparedBitmap(metrics, used))
                            return false;
                        cursorX = static_cast<int16_t>(cursorX + metrics.xAdvance);
                    }
                    if (!cursor.empty())
                        cursorX = static_cast<int16_t>(cursorX + tracking);
                    previousCodepoint = codepoint;
                    hasPrevious = true;
                }
                return true;
            });
        }

        bool prepareVisibleBitmaps(std::span<const PositionedGlyph> glyphs, int16_t x, int16_t baseline) {
            return prepareBitmaps([&](size_t& used) {
                int16_t cursorX = x;
                for (const PositionedGlyph& positioned: glyphs) {
                    const AlphaGlyph* glyph = glyphAt(positioned.glyphIndex);
                    if (glyph != nullptr) {
                        const AlphaGlyph metrics = readGlyph(*glyph);
                        const int16_t glyphX = static_cast<int16_t>(cursorX + positioned.xOffset + metrics.xOffset);
                        const int16_t glyphY = static_cast<int16_t>(baseline - positioned.yOffset + metrics.yOffset);
                        if (glyphX < output_->width() && glyphX + metrics.width > 0 && glyphY < output_->height()
                            && glyphY + metrics.height > 0 && !appendPreparedBitmap(metrics, used))
                            return false;
                    }
                    cursorX = static_cast<int16_t>(cursorX + positioned.xAdvance);
                }
                return true;
            });
        }

        template<typename Function>
        void forEachVisibleSpan(const uint8_t* packedRow, int16_t first, int16_t last, Function&& function) const {
            int16_t x = first;
            while (x < last) {
                while (x < last && coverageAt(packedRow, static_cast<uint8_t>(x)) == 0)
                    ++x;
                const int16_t begin = x;
                while (x < last && coverageAt(packedRow, static_cast<uint8_t>(x)) != 0)
                    ++x;
                if (begin < x)
                    function(begin, x);
            }
        }

        const AlphaGlyph* findGlyph(uint32_t codepoint) const {
            uint16_t glyphIndex = 0;
            return findGlyphIndex(codepoint, glyphIndex) ? glyphAt(glyphIndex) : nullptr;
        }

        bool findGlyphIndex(uint32_t codepoint, uint16_t& glyphIndex) const {
            if (font_ == nullptr || font_->glyphCount == 0)
                return false;

            if (residentGlyphIndex(*font_, codepoint, glyphIndex))
                return glyphIndex != kMissingGlyphIndex && glyphIndex < font_->glyphCount;

            if (codepoint <= UINT16_MAX && font_->pageMap != nullptr && font_->pageTableCount > 0) {
                const uint8_t pageIndex = pgm_read_byte(font_->pageMap + (codepoint >> 8U));
                if (pageIndex == kMissingPageIndex || pageIndex >= font_->pageTableCount)
                    return false;

                glyphIndex = kMissingGlyphIndex;
                if (font_->file) {
                    if (!readFile(font_->fileHeader.pageTablesOffset
                                      + (static_cast<uint32_t>(pageIndex) * RFont4::kPageTableEntries
                                         + (codepoint & 0xFFU))
                                            * sizeof(glyphIndex),
                                  &glyphIndex, sizeof(glyphIndex)))
                        return false;
                } else {
                    return findGlyphIndexBinary(codepoint, glyphIndex);
                }
                return glyphIndex != kMissingGlyphIndex && glyphIndex < font_->glyphCount;
            }

            return codepoint > UINT16_MAX ? findSupplementaryGlyphIndex(codepoint, glyphIndex)
                                          : findGlyphIndexBinary(codepoint, glyphIndex);
        }

        bool identityAt(uint32_t index, AlphaGlyphIdentity& identity) const {
            if (index >= font_->glyphCount)
                return false;
            if (font_->identities != nullptr) {
                identity = readPacked(font_->identities[index]);
                return true;
            }
            return false;
        }

        bool supplementaryAt(uint16_t index, RFont4::SupplementaryRecord& record) const {
            if (index >= font_->supplementaryCount)
                return false;
            if (font_->supplementary != nullptr) {
                record = readPacked(font_->supplementary[index]);
                return true;
            }
            return font_->file
                && readFile(font_->fileHeader.supplementaryOffset + index * sizeof(record), &record, sizeof(record));
        }

        bool findSupplementaryGlyphIndex(uint32_t codepoint, uint16_t& glyphIndex) const {
            if (font_->supplementaryCount == 0)
                return findGlyphIndexBinary(codepoint, glyphIndex);
            uint16_t first = 0;
            uint16_t last = font_->supplementaryCount;
            RFont4::SupplementaryRecord record;
            while (first < last) {
                const uint16_t middle = static_cast<uint16_t>(first + (last - first) / 2);
                if (!supplementaryAt(middle, record))
                    return false;
                if (record.codepoint < codepoint)
                    first = static_cast<uint16_t>(middle + 1);
                else
                    last = middle;
            }
            if (!supplementaryAt(first, record) || record.codepoint != codepoint)
                return false;
            glyphIndex = record.glyphIndex;
            return glyphIndex < font_->glyphCount;
        }

        bool verticalRule(uint32_t codepoint, RFont4::VerticalRule& rule) const {
            if (font_->verticalRules != nullptr)
                return residentVerticalRule(*font_, codepoint, rule);
            uint16_t first = 0;
            uint16_t last = font_->verticalRuleCount;
            while (first < last) {
                const uint16_t middle = static_cast<uint16_t>(first + (last - first) / 2);
                if (!font_->file
                    || !readFile(font_->fileHeader.verticalRulesOffset + middle * sizeof(rule), &rule, sizeof(rule)))
                    return false;
                if (rule.codepoint < codepoint)
                    first = static_cast<uint16_t>(middle + 1);
                else
                    last = middle;
            }
            if (first >= font_->verticalRuleCount)
                return false;
            if (!readFile(font_->fileHeader.verticalRulesOffset + first * sizeof(rule), &rule, sizeof(rule)))
                return false;
            return rule.codepoint == codepoint;
        }

        uint16_t verticalGlyphIndex(uint16_t glyphIndex, uint32_t codepoint, bool& sideways) const {
            RFont4::VerticalRule rule;
            if (!verticalRule(codepoint, rule))
                return glyphIndex;
            if (rule.alternateIndex == RFont4::kRotateVerticalGlyph) {
                sideways = true;
                return glyphIndex;
            }
            return rule.alternateIndex;
        }

        bool findGlyphIndexBinary(uint32_t codepoint, uint16_t& glyphIndex) const {
            uint32_t first = 0;
            uint32_t last = font_->glyphCount;
            AlphaGlyphIdentity identity;
            while (first < last) {
                const uint32_t middle = first + (last - first) / 2;
                if (!identityAt(middle, identity))
                    return false;
                if (identity.codepoint < codepoint)
                    first = middle + 1;
                else
                    last = middle;
            }
            if (first >= font_->glyphCount || !identityAt(first, identity) || identity.codepoint != codepoint)
                return false;
            glyphIndex = static_cast<uint16_t>(first);
            return true;
        }

        int16_t fileKerningAdjust(const AlphaGlyph& left, uint32_t leftCodepoint, uint32_t rightCodepoint) const {
            const uint64_t key = static_cast<uint64_t>(leftCodepoint) << 32U | rightCodepoint;
            const size_t cacheIndex =
                static_cast<size_t>((leftCodepoint * 31U) ^ rightCodepoint) & (fileKerningKeys_.size() - 1U);
            if (fileKerningKeys_[cacheIndex] == key)
                return fileKerningValues_[cacheIndex];

            uint32_t first = left.kernOffset;
            uint32_t last = first + left.kernCount;
            RFont4::KerningRecord record;
            while (first < last) {
                const uint32_t middle = first + (last - first) / 2;
                if (!readFile(font_->fileStrike.kerningOffset + middle * sizeof(record), &record, sizeof(record)))
                    return 0;
                if (record.rightCodepoint < rightCodepoint)
                    first = middle + 1;
                else
                    last = middle;
            }
            const int8_t adjustment =
                first < left.kernOffset + left.kernCount
                        && readFile(font_->fileStrike.kerningOffset + first * sizeof(record), &record, sizeof(record))
                        && record.rightCodepoint == rightCodepoint
                    ? record.xAdjust
                    : 0;
            fileKerningKeys_[cacheIndex] = key;
            fileKerningValues_[cacheIndex] = adjustment;
            return adjustment;
        }

        void rebuildBlendTable() {
            const uint8_t fgR = static_cast<uint8_t>((fg_ >> 11U) & 0x1FU);
            const uint8_t fgG = static_cast<uint8_t>((fg_ >> 5U) & 0x3FU);
            const uint8_t fgB = static_cast<uint8_t>(fg_ & 0x1FU);
            const uint8_t bgR = static_cast<uint8_t>((bg_ >> 11U) & 0x1FU);
            const uint8_t bgG = static_cast<uint8_t>((bg_ >> 5U) & 0x3FU);
            const uint8_t bgB = static_cast<uint8_t>(bg_ & 0x1FU);

            for (uint8_t coverage = 0; coverage < 16; ++coverage) {
                const uint8_t r = blendChannel(bgR, fgR, coverage, 15);
                const uint8_t g = blendChannel(bgG, fgG, coverage, 15);
                const uint8_t b = blendChannel(bgB, fgB, coverage, 15);
                blend_[coverage] =
                    static_cast<uint16_t>((static_cast<uint16_t>(r) << 11U) | (static_cast<uint16_t>(g) << 5U) | b);
            }

            blendTableValid_ = true;
        }

        static uint8_t blendChannel(uint8_t bg, uint8_t fg, uint8_t coverage, uint8_t maxCoverage) {
            const uint16_t inv = static_cast<uint16_t>(maxCoverage - coverage);
            return static_cast<uint8_t>((bg * inv + fg * coverage + maxCoverage / 2) / maxCoverage);
        }

        Arduino_GFX* output_;
        const AlphaFont* font_ = nullptr;
        uint32_t fontGeneration_ = 0;
        mutable std::array<FileGlyphCacheEntry, 32> fileGlyphCache_{};
        mutable std::array<uint64_t, 16> fileKerningKeys_{};
        mutable std::array<int8_t, 16> fileKerningValues_{};
        std::array<PreparedBitmap, 32> preparedBitmaps_{};
        std::array<uint8_t, 8192> preparedBitmapData_{};
        std::array<uint8_t, 4096> encodedBitmap_{};
        size_t preparedBitmapCount_ = 0;
        size_t preparedBitmapBytes_ = 0;
        std::array<uint8_t, (MaxRowWidth + 1) / 2> packedRow_{};
        uint16_t strip_[MaxStripRows][MaxRowWidth]{};
        std::array<uint16_t, MaxStripRows> stripFirstInk_{};
        std::array<uint16_t, MaxStripRows> stripLastInk_{};
        uint16_t blend_[16]{};
        uint16_t fg_ = 0xFFFF;
        uint16_t bg_ = 0x0000;
        bool ready_ = false;
        bool blendTableValid_ = false;
    };

} // namespace ui::fonts
