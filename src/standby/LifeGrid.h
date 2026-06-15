#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Packed-bit cell grid shared by the standby screensavers. One bit per cell,
// 32 cells per word. Pure (no Arduino, no display) so the Game-of-Life rule and
// the bit packing can be unit tested on the host.
namespace standby {

struct LifePoint {
  int8_t x;
  int8_t y;
};

// Linear-congruential step; returns the new state and advances rng in place.
uint32_t advanceRng(uint32_t &rng);

size_t packedWordCount(size_t cellCount);
bool cellAlive(const std::vector<uint32_t> &cells, size_t index);
void setCell(std::vector<uint32_t> &cells, size_t index, bool alive);
void setCellAt(std::vector<uint32_t> &cells, uint16_t columns, uint16_t rows, int x, int y,
               bool alive);

// Clears a margin-padded rect then stamps a pattern, but only if the pattern
// fits within the grid. No-op when out of bounds.
void clearAndStampPattern(std::vector<uint32_t> &cells, uint16_t columns, uint16_t rows,
                          const LifePoint *points, size_t pointCount, int originX, int originY,
                          int width, int height);

// Advances one Conway's Game of Life generation on a toroidal grid: reads cur,
// writes next (resized as needed). Returns the number of live cells in next.
size_t lifeStep(const std::vector<uint32_t> &cur, std::vector<uint32_t> &next, uint16_t columns,
                uint16_t rows);

}  // namespace standby
