#include <unity.h>

#include <vector>

#include "standby/LifeGrid.h"

namespace {

// Builds an empty packed grid sized for columns x rows.
std::vector<uint32_t> emptyGrid(uint16_t columns, uint16_t rows) {
  return std::vector<uint32_t>(
      standby::packedWordCount(static_cast<size_t>(columns) * rows), 0);
}

}  // namespace

void test_set_and_read_cell_roundtrips() {
  std::vector<uint32_t> grid = emptyGrid(8, 8);
  TEST_ASSERT_FALSE(standby::cellAlive(grid, 35));
  standby::setCell(grid, 35, true);
  TEST_ASSERT_TRUE(standby::cellAlive(grid, 35));
  standby::setCell(grid, 35, false);
  TEST_ASSERT_FALSE(standby::cellAlive(grid, 35));
}

void test_set_cell_at_clips_out_of_bounds() {
  std::vector<uint32_t> grid = emptyGrid(8, 8);
  standby::setCellAt(grid, 8, 8, -1, 4, true);  // off-grid, no-op
  standby::setCellAt(grid, 8, 8, 8, 4, true);   // off-grid, no-op
  for (uint32_t word : grid) {
    TEST_ASSERT_EQUAL_UINT32(0, word);
  }
  standby::setCellAt(grid, 8, 8, 3, 2, true);
  TEST_ASSERT_TRUE(standby::cellAlive(grid, 2 * 8 + 3));
}

void test_blinker_oscillates_period_two() {
  // A vertical 3-cell blinker becomes horizontal after one step, then vertical
  // again after the next -- the canonical Game-of-Life period-2 oscillator.
  const uint16_t cols = 8;
  const uint16_t rows = 8;
  std::vector<uint32_t> grid = emptyGrid(cols, rows);
  standby::setCellAt(grid, cols, rows, 4, 3, true);
  standby::setCellAt(grid, cols, rows, 4, 4, true);
  standby::setCellAt(grid, cols, rows, 4, 5, true);

  std::vector<uint32_t> next;
  const size_t aliveAfterOne = standby::lifeStep(grid, next, cols, rows);
  TEST_ASSERT_EQUAL_UINT32(3, aliveAfterOne);
  // Now horizontal: (3,4) (4,4) (5,4).
  TEST_ASSERT_TRUE(standby::cellAlive(next, 4 * cols + 3));
  TEST_ASSERT_TRUE(standby::cellAlive(next, 4 * cols + 4));
  TEST_ASSERT_TRUE(standby::cellAlive(next, 4 * cols + 5));
  TEST_ASSERT_FALSE(standby::cellAlive(next, 3 * cols + 4));
  TEST_ASSERT_FALSE(standby::cellAlive(next, 5 * cols + 4));

  std::vector<uint32_t> back;
  standby::lifeStep(next, back, cols, rows);
  // Back to the original vertical blinker.
  TEST_ASSERT_TRUE(standby::cellAlive(back, 3 * cols + 4));
  TEST_ASSERT_TRUE(standby::cellAlive(back, 4 * cols + 4));
  TEST_ASSERT_TRUE(standby::cellAlive(back, 5 * cols + 4));
}

void test_empty_grid_stays_empty() {
  const uint16_t cols = 8;
  const uint16_t rows = 8;
  std::vector<uint32_t> grid = emptyGrid(cols, rows);
  std::vector<uint32_t> next;
  TEST_ASSERT_EQUAL_UINT32(0, standby::lifeStep(grid, next, cols, rows));
}

void test_advance_rng_is_deterministic() {
  uint32_t a = 12345;
  uint32_t b = 12345;
  TEST_ASSERT_EQUAL_UINT32(standby::advanceRng(a), standby::advanceRng(b));
  TEST_ASSERT_EQUAL_UINT32(a, b);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_set_and_read_cell_roundtrips);
  RUN_TEST(test_set_cell_at_clips_out_of_bounds);
  RUN_TEST(test_blinker_oscillates_period_two);
  RUN_TEST(test_empty_grid_stays_empty);
  RUN_TEST(test_advance_rng_is_deterministic);
  return UNITY_END();
}
