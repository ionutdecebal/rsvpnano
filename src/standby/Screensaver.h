#pragma once

#include <cstdint>
#include <memory>
#include <vector>

// A standby screensaver: an automaton that produces a packed-bit cell grid each
// frame. App seeds it (with entropy it gathers), steps it on a timer, and hands
// the resulting grid to DisplayManager::renderLifeScreensaver. Adding a new
// screensaver means adding an adapter here, not editing App.
namespace standby {

enum class Kind {
  Life,
  Maze,
  Voronoi,
};

struct Frame {
  const std::vector<uint32_t> *cells;     // live cells (packed bits)
  const std::vector<uint32_t> *dimCells;  // dim cells, or null
  uint32_t generation;
};

class Screensaver {
 public:
  virtual ~Screensaver() = default;

  // (Re)initialize from a seed. The caller supplies entropy; the screensaver
  // owns all further randomness, so step() needs no outside state.
  virtual void seed(uint32_t rngSeed) = 0;

  // Advance one frame. May reseed itself when the pattern stagnates.
  virtual void step() = 0;

  // The current grid to render. Pointers stay valid until the next seed/step.
  virtual Frame frame() const = 0;
};

// Builds the screensaver for a kind, sized to the given grid. Never returns null
// for a known kind.
std::unique_ptr<Screensaver> makeScreensaver(Kind kind, uint16_t columns, uint16_t rows);

}  // namespace standby
