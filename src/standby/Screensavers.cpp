#include "standby/Screensaver.h"

#include <algorithm>

#include "standby/LifeGrid.h"

namespace standby {
namespace {

// ---------------------------------------------------------------------------
// Conway's Game of Life: random soup seeded with a few classic patterns.
// ---------------------------------------------------------------------------
constexpr LifePoint kGlider[] = {
    {1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2},
};
constexpr LifePoint kLightweightSpaceship[] = {
    {1, 0}, {4, 0}, {0, 1}, {0, 2}, {4, 2}, {0, 3}, {1, 3}, {2, 3}, {3, 3},
};
constexpr LifePoint kPentadecathlon[] = {
    {2, 0}, {2, 1}, {1, 2}, {3, 2}, {2, 3}, {2, 4},
    {2, 5}, {2, 6}, {1, 7}, {3, 7}, {2, 8}, {2, 9},
};
constexpr LifePoint kPulsar[] = {
    {2, 0},  {3, 0},  {4, 0},  {8, 0},  {9, 0},  {10, 0}, {0, 2},  {5, 2},
    {7, 2},  {12, 2}, {0, 3},  {5, 3},  {7, 3},  {12, 3}, {0, 4},  {5, 4},
    {7, 4},  {12, 4}, {2, 5},  {3, 5},  {4, 5},  {8, 5},  {9, 5},  {10, 5},
    {2, 7},  {3, 7},  {4, 7},  {8, 7},  {9, 7},  {10, 7}, {0, 8},  {5, 8},
    {7, 8},  {12, 8}, {0, 9},  {5, 9},  {7, 9},  {12, 9}, {0, 10}, {5, 10},
    {7, 10}, {12, 10}, {2, 12}, {3, 12}, {4, 12}, {8, 12}, {9, 12}, {10, 12},
};
constexpr LifePoint kGosperGliderGun[] = {
    {24, 0}, {22, 1}, {24, 1}, {12, 2}, {13, 2}, {20, 2}, {21, 2}, {34, 2}, {35, 2},
    {11, 3}, {15, 3}, {20, 3}, {21, 3}, {34, 3}, {35, 3}, {0, 4},  {1, 4},
    {10, 4}, {16, 4}, {20, 4}, {21, 4}, {0, 5},  {1, 5},  {10, 5}, {14, 5},
    {16, 5}, {17, 5}, {22, 5}, {24, 5}, {10, 6}, {16, 6}, {24, 6}, {11, 7},
    {15, 7}, {12, 8}, {13, 8},
};

template <typename T, size_t N>
constexpr size_t count(const T (&)[N]) {
  return N;
}

class LifeScreensaver : public Screensaver {
 public:
  LifeScreensaver(uint16_t columns, uint16_t rows) : columns_(columns), rows_(rows) {}

  void seed(uint32_t rngSeed) override {
    rng_ = rngSeed;
    const size_t cellCount = static_cast<size_t>(columns_) * rows_;
    cells_.assign(packedWordCount(cellCount), 0);
    generation_ = 0;

    for (size_t i = 0; i < cellCount; ++i) {
      setCell(cells_, i, (advanceRng(rng_) >> 24) < 12);
    }

    clearAndStampPattern(cells_, columns_, rows_, kGosperGliderGun, count(kGosperGliderGun), 18, 18,
                         36, 9);
    clearAndStampPattern(cells_, columns_, rows_, kGosperGliderGun, count(kGosperGliderGun),
                         static_cast<int>(columns_) - 62, static_cast<int>(rows_) - 34, 36, 9);
    clearAndStampPattern(cells_, columns_, rows_, kPulsar, count(kPulsar),
                         static_cast<int>(columns_ / 2) - 7, static_cast<int>(rows_ / 2) - 7, 13,
                         13);
    clearAndStampPattern(cells_, columns_, rows_, kPentadecathlon, count(kPentadecathlon),
                         static_cast<int>(columns_ / 3), static_cast<int>(rows_) - 42, 5, 10);
    clearAndStampPattern(cells_, columns_, rows_, kLightweightSpaceship,
                         count(kLightweightSpaceship), static_cast<int>((columns_ * 2) / 3),
                         static_cast<int>(rows_ / 3), 5, 4);

    for (uint8_t i = 0; i < 10; ++i) {
      const int x = static_cast<int>((advanceRng(rng_) >> 8) % std::max<uint16_t>(1, columns_ - 6));
      const int y = static_cast<int>((advanceRng(rng_) >> 8) % std::max<uint16_t>(1, rows_ - 6));
      clearAndStampPattern(cells_, columns_, rows_, kGlider, count(kGlider), x, y, 3, 3);
    }
  }

  void step() override {
    const size_t cellCount = static_cast<size_t>(columns_) * rows_;
    if (cells_.size() != packedWordCount(cellCount)) {
      seed(advanceRng(rng_));
      return;
    }
    const size_t aliveCount = lifeStep(cells_, next_, columns_, rows_);
    cells_.swap(next_);
    ++generation_;
    if (aliveCount == 0 || aliveCount > (cellCount * 3) / 4) {
      seed(advanceRng(rng_));
    }
  }

  Frame frame() const override { return Frame{&cells_, nullptr, generation_}; }

 private:
  uint16_t columns_;
  uint16_t rows_;
  uint32_t rng_ = 1;
  uint32_t generation_ = 0;
  std::vector<uint32_t> cells_;
  std::vector<uint32_t> next_;
};

// ---------------------------------------------------------------------------
// Depth-first maze carve, rendered into the same packed grid.
// ---------------------------------------------------------------------------
class MazeScreensaver : public Screensaver {
 public:
  MazeScreensaver(uint16_t columns, uint16_t rows) : columns_(columns), rows_(rows) {}

  void seed(uint32_t rngSeed) override {
    rng_ = rngSeed;
    const size_t cellCount = static_cast<size_t>(columns_) * rows_;
    const uint16_t mazeColumns = std::max<uint16_t>(1, (columns_ - 1) / 2);
    const uint16_t mazeRows = std::max<uint16_t>(1, (rows_ - 1) / 2);
    cells_.assign(packedWordCount(cellCount), 0);
    visited_.assign(static_cast<size_t>(mazeColumns) * mazeRows, 0);
    stack_.clear();
    generation_ = 0;

    const uint16_t startX = static_cast<uint16_t>((advanceRng(rng_) >> 8) % mazeColumns);
    const uint16_t startY = static_cast<uint16_t>((advanceRng(rng_) >> 8) % mazeRows);
    visited_[static_cast<size_t>(startY) * mazeColumns + startX] = 1;
    stack_.push_back(static_cast<uint16_t>(startY * mazeColumns + startX));
    setCellAt(cells_, columns_, rows_, static_cast<int>(startX) * 2 + 1,
              static_cast<int>(startY) * 2 + 1, true);
  }

  void step() override {
    const uint16_t mazeColumns = std::max<uint16_t>(1, (columns_ - 1) / 2);
    const uint16_t mazeRows = std::max<uint16_t>(1, (rows_ - 1) / 2);
    const size_t mazeCellCount = static_cast<size_t>(mazeColumns) * mazeRows;
    if (visited_.size() != mazeCellCount || stack_.empty()) {
      if (stack_.empty() && generation_ < 600) {
        ++generation_;
        return;
      }
      seed(advanceRng(rng_));
      return;
    }

    constexpr uint8_t kStepsPerFrame = 32;
    for (uint8_t step = 0; step < kStepsPerFrame && !stack_.empty(); ++step) {
      const uint16_t current = stack_.back();
      const uint16_t cx = current % mazeColumns;
      const uint16_t cy = current / mazeColumns;
      uint16_t candidates[4];
      uint8_t candidateCount = 0;

      auto addCandidate = [&](int nx, int ny) {
        if (nx < 0 || ny < 0 || nx >= static_cast<int>(mazeColumns) ||
            ny >= static_cast<int>(mazeRows)) {
          return;
        }
        const uint16_t encoded = static_cast<uint16_t>(ny * mazeColumns + nx);
        if (visited_[encoded] == 0) {
          candidates[candidateCount++] = encoded;
        }
      };

      addCandidate(static_cast<int>(cx) + 1, cy);
      addCandidate(static_cast<int>(cx) - 1, cy);
      addCandidate(cx, static_cast<int>(cy) + 1);
      addCandidate(cx, static_cast<int>(cy) - 1);

      if (candidateCount == 0) {
        stack_.pop_back();
        continue;
      }

      const uint16_t next = candidates[(advanceRng(rng_) >> 16) % candidateCount];
      const uint16_t nx = next % mazeColumns;
      const uint16_t ny = next / mazeColumns;
      visited_[next] = 1;
      stack_.push_back(next);

      const int displayCx = static_cast<int>(cx) * 2 + 1;
      const int displayCy = static_cast<int>(cy) * 2 + 1;
      const int displayNx = static_cast<int>(nx) * 2 + 1;
      const int displayNy = static_cast<int>(ny) * 2 + 1;
      setCellAt(cells_, columns_, rows_, displayNx, displayNy, true);
      setCellAt(cells_, columns_, rows_, (displayCx + displayNx) / 2, (displayCy + displayNy) / 2,
                true);
    }

    if (stack_.empty()) {
      generation_ = 0;
    } else {
      ++generation_;
    }
  }

  Frame frame() const override { return Frame{&cells_, nullptr, generation_}; }

 private:
  uint16_t columns_;
  uint16_t rows_;
  uint32_t rng_ = 1;
  uint32_t generation_ = 0;
  std::vector<uint32_t> cells_;
  std::vector<uint8_t> visited_;
  std::vector<uint16_t> stack_;
};

// ---------------------------------------------------------------------------
// Moving Voronoi sites; cell edges are lit, near-edges dimmed.
// ---------------------------------------------------------------------------
constexpr size_t kVoronoiSiteCount = 15;

class VoronoiScreensaver : public Screensaver {
 public:
  VoronoiScreensaver(uint16_t columns, uint16_t rows) : columns_(columns), rows_(rows) {}

  void seed(uint32_t rngSeed) override {
    rng_ = rngSeed;
    generation_ = 0;
    vx_.assign(kVoronoiSiteCount, 0);
    vy_.assign(kVoronoiSiteCount, 0);
    vdx_.assign(kVoronoiSiteCount, 0);
    vdy_.assign(kVoronoiSiteCount, 0);
    for (size_t i = 0; i < kVoronoiSiteCount; ++i) {
      vx_[i] = static_cast<int16_t>(((advanceRng(rng_) >> 8) % columns_) * 16);
      vy_[i] = static_cast<int16_t>(((advanceRng(rng_) >> 8) % rows_) * 16);
      const int16_t dx = static_cast<int16_t>(4 + ((advanceRng(rng_) >> 24) % 7));
      const int16_t dy = static_cast<int16_t>(3 + ((advanceRng(rng_) >> 24) % 6));
      vdx_[i] = (advanceRng(rng_) & 1U) != 0 ? dx : static_cast<int16_t>(-dx);
      vdy_[i] = (advanceRng(rng_) & 1U) != 0 ? dy : static_cast<int16_t>(-dy);
    }
    render();
  }

  void step() override {
    if (vx_.size() != kVoronoiSiteCount) {
      seed(advanceRng(rng_));
      return;
    }

    const int16_t maxX = static_cast<int16_t>((columns_ - 1) * 16);
    const int16_t maxY = static_cast<int16_t>((rows_ - 1) * 16);
    for (size_t i = 0; i < vx_.size(); ++i) {
      int16_t nextX = static_cast<int16_t>(vx_[i] + vdx_[i]);
      int16_t nextY = static_cast<int16_t>(vy_[i] + vdy_[i]);
      if (nextX < 0 || nextX > maxX) {
        vdx_[i] = static_cast<int16_t>(-vdx_[i]);
        nextX = std::max<int16_t>(0, std::min<int16_t>(maxX, nextX));
      }
      if (nextY < 0 || nextY > maxY) {
        vdy_[i] = static_cast<int16_t>(-vdy_[i]);
        nextY = std::max<int16_t>(0, std::min<int16_t>(maxY, nextY));
      }
      vx_[i] = nextX;
      vy_[i] = nextY;
    }

    ++generation_;
    if (generation_ > 2400) {
      seed(advanceRng(rng_));
      return;
    }
    render();
  }

  Frame frame() const override { return Frame{&cells_, &dimCells_, generation_}; }

 private:
  void render() {
    const size_t cellCount = static_cast<size_t>(columns_) * rows_;
    const size_t wordCount = packedWordCount(cellCount);
    cells_.assign(wordCount, 0);
    dimCells_.assign(wordCount, 0);
    if (vx_.empty()) {
      return;
    }

    for (uint16_t y = 0; y < rows_; ++y) {
      const int32_t cellY = static_cast<int32_t>(y) * 16 + 8;
      for (uint16_t x = 0; x < columns_; ++x) {
        const int32_t cellX = static_cast<int32_t>(x) * 16 + 8;
        int32_t nearest = INT32_MAX;
        int32_t secondNearest = INT32_MAX;
        for (size_t i = 0; i < vx_.size(); ++i) {
          const int32_t dx = cellX - vx_[i];
          const int32_t dy = cellY - vy_[i];
          const int32_t distance = dx * dx + dy * dy;
          if (distance < nearest) {
            secondNearest = nearest;
            nearest = distance;
          } else if (distance < secondNearest) {
            secondNearest = distance;
          }
        }

        const size_t index = static_cast<size_t>(y) * columns_ + x;
        const int32_t gap = secondNearest - nearest;
        if (nearest < 1200 || gap < 190) {
          setCell(cells_, index, true);
        } else if (gap < 580 + nearest / 180) {
          setCell(dimCells_, index, true);
        }
      }
    }
  }

  uint16_t columns_;
  uint16_t rows_;
  uint32_t rng_ = 1;
  uint32_t generation_ = 0;
  std::vector<uint32_t> cells_;
  std::vector<uint32_t> dimCells_;
  std::vector<int16_t> vx_;
  std::vector<int16_t> vy_;
  std::vector<int16_t> vdx_;
  std::vector<int16_t> vdy_;
};

}  // namespace

std::unique_ptr<Screensaver> makeScreensaver(Kind kind, uint16_t columns, uint16_t rows) {
  switch (kind) {
    case Kind::Maze:
      return std::unique_ptr<Screensaver>(new MazeScreensaver(columns, rows));
    case Kind::Voronoi:
      return std::unique_ptr<Screensaver>(new VoronoiScreensaver(columns, rows));
    case Kind::Life:
    default:
      return std::unique_ptr<Screensaver>(new LifeScreensaver(columns, rows));
  }
}

}  // namespace standby
