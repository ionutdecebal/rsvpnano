#include "standby/LifeGrid.h"

#include <algorithm>

namespace standby {
namespace {

void clearRect(std::vector<uint32_t> &cells, uint16_t columns, uint16_t rows, int x, int y,
               int width, int height) {
  const int xEnd = std::min(static_cast<int>(columns), x + width);
  const int yEnd = std::min(static_cast<int>(rows), y + height);
  for (int cy = std::max(0, y); cy < yEnd; ++cy) {
    for (int cx = std::max(0, x); cx < xEnd; ++cx) {
      setCellAt(cells, columns, rows, cx, cy, false);
    }
  }
}

void stampPattern(std::vector<uint32_t> &cells, uint16_t columns, uint16_t rows,
                  const LifePoint *points, size_t pointCount, int originX, int originY) {
  for (size_t i = 0; i < pointCount; ++i) {
    setCellAt(cells, columns, rows, originX + points[i].x, originY + points[i].y, true);
  }
}

}  // namespace

uint32_t advanceRng(uint32_t &rng) {
  rng = (rng * 1664525UL) + 1013904223UL;
  return rng;
}

size_t packedWordCount(size_t cellCount) { return (cellCount + 31U) / 32U; }

bool cellAlive(const std::vector<uint32_t> &cells, size_t index) {
  const size_t word = index / 32U;
  if (word >= cells.size()) {
    return false;
  }
  return (cells[word] & (1UL << (index % 32U))) != 0;
}

void setCell(std::vector<uint32_t> &cells, size_t index, bool alive) {
  const size_t word = index / 32U;
  if (word >= cells.size()) {
    return;
  }
  const uint32_t mask = 1UL << (index % 32U);
  if (alive) {
    cells[word] |= mask;
  } else {
    cells[word] &= ~mask;
  }
}

void setCellAt(std::vector<uint32_t> &cells, uint16_t columns, uint16_t rows, int x, int y,
               bool alive) {
  if (x < 0 || y < 0 || x >= static_cast<int>(columns) || y >= static_cast<int>(rows)) {
    return;
  }
  setCell(cells, static_cast<size_t>(y) * columns + static_cast<size_t>(x), alive);
}

void clearAndStampPattern(std::vector<uint32_t> &cells, uint16_t columns, uint16_t rows,
                          const LifePoint *points, size_t pointCount, int originX, int originY,
                          int width, int height) {
  if (originX < 0 || originY < 0 || originX + width > static_cast<int>(columns) ||
      originY + height > static_cast<int>(rows)) {
    return;
  }
  constexpr int kPatternMargin = 5;
  clearRect(cells, columns, rows, originX - kPatternMargin, originY - kPatternMargin,
            width + kPatternMargin * 2, height + kPatternMargin * 2);
  stampPattern(cells, columns, rows, points, pointCount, originX, originY);
}

size_t lifeStep(const std::vector<uint32_t> &cur, std::vector<uint32_t> &next, uint16_t columns,
                uint16_t rows) {
  const size_t cellCount = static_cast<size_t>(columns) * static_cast<size_t>(rows);
  next.assign(packedWordCount(cellCount), 0);

  size_t aliveCount = 0;
  for (uint16_t y = 0; y < rows; ++y) {
    for (uint16_t x = 0; x < columns; ++x) {
      uint8_t neighbours = 0;
      for (int8_t dy = -1; dy <= 1; ++dy) {
        for (int8_t dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) {
            continue;
          }
          const uint16_t nx =
              static_cast<uint16_t>((static_cast<int>(x) + dx + columns) % columns);
          const uint16_t ny = static_cast<uint16_t>((static_cast<int>(y) + dy + rows) % rows);
          neighbours +=
              cellAlive(cur, static_cast<size_t>(ny) * columns + nx) ? 1 : 0;
        }
      }

      const size_t index = static_cast<size_t>(y) * columns + x;
      const bool alive = cellAlive(cur, index);
      const bool nextAlive = alive ? (neighbours == 2 || neighbours == 3) : (neighbours == 3);
      setCell(next, index, nextAlive);
      if (nextAlive) {
        ++aliveCount;
      }
    }
  }
  return aliveCount;
}

}  // namespace standby
