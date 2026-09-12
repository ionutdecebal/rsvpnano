#include "ui/screens/StandbyScreen.h"

#include <algorithm>
#include <bit>

#include "screensavers/PackedGrid.h"
#include "ui/screens/Screens.h"

namespace screens {

    namespace {
        constexpr uint8_t kCellSize = 4;
        constexpr uint32_t kFrameMs = 160;
        constexpr uint32_t kVoronoiFrameMs = 120;
        constexpr uint16_t ceilDiv(uint16_t numerator, uint16_t denominator) {
            return static_cast<uint16_t>((numerator + denominator - 1U) / denominator);
        }
    } // namespace

    void StandbyScreen::begin(ui::Context& ui, uint32_t nowMs, size_t bookIndex, size_t wordIndex, standby::Kind kind) {
        kind_ = kind;
        columns_ =
            std::clamp<uint16_t>(ceilDiv(std::max<int16_t>(1, ui.width()), kCellSize), 1, standby::kMaxStandbyColumns);
        rows_ =
            std::clamp<uint16_t>(ceilDiv(std::max<int16_t>(1, ui.height()), kCellSize), 1, standby::kMaxStandbyRows);
        screensaver_.select(kind, columns_, rows_);
        const uint32_t seed =
            nowMs ^ (static_cast<uint32_t>(bookIndex) << 16U) ^ (static_cast<uint32_t>(wordIndex) * 2654435761UL);
        screensaver_.seed(seed == 0 ? 1U : seed);
        nextFrameMs_ = nowMs;
    }

    void StandbyScreen::reset() {
        screensaver_.reset();
    }

    void StandbyScreen::update(ui::Context& ui, uint32_t nowMs) {
        if (static_cast<int32_t>(nowMs - nextFrameMs_) < 0)
            return;
        const uint32_t frameMs = kind_ == standby::Kind::voronoi ? kVoronoiFrameMs : kFrameMs;
        screensaver_.step();
        nextFrameMs_ = nowMs + frameMs;
        draw(ui);
    }

    void StandbyScreen::draw(ui::Context& ui) {
        if (!screensaver_)
            return;
        const standby::Frame frame = screensaver_.frame();
        ui.beginFrame(static_cast<uint8_t>(Screen::Standby));
        if (frame.cells.empty() || columns_ == 0 || rows_ == 0) {
            ui.endFrame();
            return;
        }

        int16_t originX = static_cast<int16_t>((ui.width() - columns_ * kCellSize) / 2);
        int16_t originY = static_cast<int16_t>((ui.height() - rows_ * kCellSize) / 2);
        if constexpr (ui::Context::displayWriteAlignment() > 1) {
            // Four-pixel cells then own complete transfer pairs, including dirty runs.
            originX = static_cast<int16_t>(originX & ~1);
            originY = static_cast<int16_t>(originY & ~1);
        }
        const uint16_t dim = ui.blend(ui::themes::ColorRole::Foreground, 72);
        const uint16_t bright = kind_ == standby::Kind::life ? ui.color(ui::themes::ColorRole::Foreground)
                                                             : ui.color(ui::themes::ColorRole::Accent);
        const size_t cellCount = static_cast<size_t>(columns_) * rows_;
        const auto drawRun = [&](size_t first, size_t last, uint16_t color) {
            const uint16_t x = static_cast<uint16_t>(first % columns_);
            const uint16_t y = static_cast<uint16_t>(first / columns_);
            const ui::Rect rect{static_cast<int16_t>(originX + x * kCellSize),
                                static_cast<int16_t>(originY + y * kCellSize),
                                static_cast<int16_t>((last - first + 1U) * kCellSize), kCellSize};
            ui.paint(rect, [&](Arduino_GFX& output, ui::Rect area) {
                output.fillRect(area.x, area.y, area.w, area.h, color);
            });
        };
        if (frame.fullRedraw || frame.dirtyCells.empty()) {
            ui.paint({0, 0, ui.width(), ui.height()}, [&](Arduino_GFX& output, ui::Rect area) {
                output.fillRect(area.x, area.y, area.w, area.h, ui.color(ui::themes::ColorRole::Background));
            });
            const auto drawCells = [&](standby::PackedGridView cells, uint16_t color) {
                size_t runStart = cellCount;
                size_t runEnd = 0;
                for (size_t wordIndex = 0; wordIndex < cells.size(); ++wordIndex) {
                    uint32_t bits = cells[wordIndex];
                    while (bits != 0) {
                        const size_t index = wordIndex * standby::kPackedBitsPerWord + std::countr_zero(bits);
                        if (index >= cellCount)
                            break;
                        if (runStart < cellCount && (index != runEnd + 1U || index % columns_ == 0)) {
                            drawRun(runStart, runEnd, color);
                            runStart = cellCount;
                        }
                        if (runStart == cellCount)
                            runStart = index;
                        runEnd = index;
                        bits &= bits - 1U;
                    }
                }
                if (runStart < cellCount)
                    drawRun(runStart, runEnd, color);
            };
            if (!frame.dimCells.empty())
                drawCells(frame.dimCells, dim);
            drawCells(frame.cells, bright);
            ui.markDrawn();
        } else {
            size_t runStart = cellCount;
            size_t runEnd = 0;
            uint16_t runColor = 0;
            for (size_t wordIndex = 0; wordIndex < frame.dirtyCells.size(); ++wordIndex) {
                uint32_t bits = frame.dirtyCells[wordIndex];
                while (bits != 0) {
                    const unsigned bit = std::countr_zero(bits);
                    const uint32_t mask = 1UL << bit;
                    const size_t index = wordIndex * standby::kPackedBitsPerWord + bit;
                    if (index >= cellCount)
                        break;
                    const uint16_t color = (frame.cells[wordIndex] & mask) != 0 ? bright
                                         : !frame.dimCells.empty() && (frame.dimCells[wordIndex] & mask) != 0
                                             ? dim
                                             : ui.color(ui::themes::ColorRole::Background);
                    if (runStart < cellCount && (index != runEnd + 1U || index % columns_ == 0 || color != runColor)) {
                        drawRun(runStart, runEnd, runColor);
                        runStart = cellCount;
                    }
                    if (runStart == cellCount) {
                        runStart = index;
                        runColor = color;
                    }
                    runEnd = index;
                    bits &= bits - 1U;
                }
            }
            if (runStart < cellCount) {
                drawRun(runStart, runEnd, runColor);
                ui.markDrawn();
            }
        }
        ui.endFrame();
    }

} // namespace screens
