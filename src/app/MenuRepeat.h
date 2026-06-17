#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

namespace MenuRepeat {

struct MoveResult {
  size_t index = 0;
  bool changed = false;
};

constexpr uint16_t kDefaultDelayMs = 250;
constexpr uint16_t kDelayOptionsMs[] = {0, 150, 250, 350, 500};

inline uint16_t sanitizeDelayMs(uint16_t delayMs) {
  for (uint16_t option : kDelayOptionsMs) {
    if (delayMs == option) {
      return option;
    }
  }
  return kDefaultDelayMs;
}

inline uint16_t nextDelayMs(uint16_t delayMs) {
  const uint16_t current = sanitizeDelayMs(delayMs);
  constexpr size_t kOptionCount = sizeof(kDelayOptionsMs) / sizeof(kDelayOptionsMs[0]);
  for (size_t i = 0; i < kOptionCount; ++i) {
    if (kDelayOptionsMs[i] == current) {
      return kDelayOptionsMs[(i + 1) % kOptionCount];
    }
  }
  return kDefaultDelayMs;
}

inline int directionForDrag(int deltaX, int deltaY, uint16_t swipeThresholdPx,
                            uint16_t axisBiasPx) {
  const int absDeltaX = abs(deltaX);
  const int absDeltaY = abs(deltaY);
  if (absDeltaY < static_cast<int>(swipeThresholdPx) ||
      absDeltaY <= absDeltaX + static_cast<int>(axisBiasPx)) {
    return 0;
  }

  return deltaY < 0 ? -1 : 1;
}

inline bool isRightSwipe(int deltaX, int deltaY, uint16_t swipeThresholdPx,
                         uint16_t axisBiasPx) {
  const int absDeltaX = abs(deltaX);
  const int absDeltaY = abs(deltaY);
  return deltaX >= static_cast<int>(swipeThresholdPx) &&
         absDeltaX > absDeltaY + static_cast<int>(axisBiasPx);
}

inline MoveResult movedIndex(size_t selectedIndex, size_t itemCount, int direction, bool wrap) {
  MoveResult result;
  result.index = selectedIndex;
  if (direction == 0 || itemCount == 0) {
    return result;
  }

  int next = static_cast<int>(selectedIndex) + direction;
  if (wrap && next < 0) {
    next = static_cast<int>(itemCount) - 1;
  } else if (wrap && next >= static_cast<int>(itemCount)) {
    next = 0;
  } else if (next < 0) {
    next = 0;
  } else if (next >= static_cast<int>(itemCount)) {
    next = static_cast<int>(itemCount) - 1;
  }

  const size_t nextIndex = static_cast<size_t>(next);
  result.index = nextIndex;
  result.changed = nextIndex != selectedIndex;
  return result;
}

}  // namespace MenuRepeat
