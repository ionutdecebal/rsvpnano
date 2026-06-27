#include <unity.h>

#include "app/MenuRepeat.h"

namespace {

constexpr uint16_t kSwipeThresholdPx = 40;
constexpr uint16_t kAxisBiasPx = 12;

}  // namespace

void test_repeat_delay_sanitizes_to_default(void) {
  TEST_ASSERT_EQUAL_UINT16(250u, MenuRepeat::sanitizeDelayMs(1));
  TEST_ASSERT_EQUAL_UINT16(250u, MenuRepeat::sanitizeDelayMs(149));
  TEST_ASSERT_EQUAL_UINT16(250u, MenuRepeat::sanitizeDelayMs(700));
}

void test_repeat_delay_accepts_configured_options(void) {
  TEST_ASSERT_EQUAL_UINT16(0u, MenuRepeat::sanitizeDelayMs(0));
  TEST_ASSERT_EQUAL_UINT16(150u, MenuRepeat::sanitizeDelayMs(150));
  TEST_ASSERT_EQUAL_UINT16(250u, MenuRepeat::sanitizeDelayMs(250));
  TEST_ASSERT_EQUAL_UINT16(350u, MenuRepeat::sanitizeDelayMs(350));
  TEST_ASSERT_EQUAL_UINT16(500u, MenuRepeat::sanitizeDelayMs(500));
}

void test_repeat_delay_cycles_through_ui_options(void) {
  TEST_ASSERT_EQUAL_UINT16(150u, MenuRepeat::nextDelayMs(0));
  TEST_ASSERT_EQUAL_UINT16(250u, MenuRepeat::nextDelayMs(150));
  TEST_ASSERT_EQUAL_UINT16(350u, MenuRepeat::nextDelayMs(250));
  TEST_ASSERT_EQUAL_UINT16(500u, MenuRepeat::nextDelayMs(350));
  TEST_ASSERT_EQUAL_UINT16(0u, MenuRepeat::nextDelayMs(500));
  TEST_ASSERT_EQUAL_UINT16(350u, MenuRepeat::nextDelayMs(999));
}

void test_drag_direction_uses_vertical_threshold_and_bias(void) {
  TEST_ASSERT_EQUAL_INT(0, MenuRepeat::directionForDrag(0, 39, kSwipeThresholdPx, kAxisBiasPx));
  TEST_ASSERT_EQUAL_INT(1, MenuRepeat::directionForDrag(0, 40, kSwipeThresholdPx, kAxisBiasPx));
  TEST_ASSERT_EQUAL_INT(-1, MenuRepeat::directionForDrag(0, -40, kSwipeThresholdPx, kAxisBiasPx));
  TEST_ASSERT_EQUAL_INT(0, MenuRepeat::directionForDrag(35, 46, kSwipeThresholdPx, kAxisBiasPx));
  TEST_ASSERT_EQUAL_INT(1, MenuRepeat::directionForDrag(20, 40, kSwipeThresholdPx, kAxisBiasPx));
}

void test_right_swipe_uses_horizontal_threshold_and_bias(void) {
  TEST_ASSERT_FALSE(MenuRepeat::isRightSwipe(39, 0, kSwipeThresholdPx, kAxisBiasPx));
  TEST_ASSERT_TRUE(MenuRepeat::isRightSwipe(40, 0, kSwipeThresholdPx, kAxisBiasPx));
  TEST_ASSERT_FALSE(MenuRepeat::isRightSwipe(-40, 0, kSwipeThresholdPx, kAxisBiasPx));
  TEST_ASSERT_FALSE(MenuRepeat::isRightSwipe(46, 35, kSwipeThresholdPx, kAxisBiasPx));
  TEST_ASSERT_TRUE(MenuRepeat::isRightSwipe(40, 20, kSwipeThresholdPx, kAxisBiasPx));
}

void test_moved_index_wraps_for_quick_swipe_behavior(void) {
  MenuRepeat::MoveResult move = MenuRepeat::movedIndex(0, 3, -1, true);
  TEST_ASSERT_EQUAL_UINT32(2u, move.index);
  TEST_ASSERT_TRUE(move.changed);

  move = MenuRepeat::movedIndex(2, 3, 1, true);
  TEST_ASSERT_EQUAL_UINT32(0u, move.index);
  TEST_ASSERT_TRUE(move.changed);
}

void test_moved_index_clamps_for_hold_repeat_behavior(void) {
  MenuRepeat::MoveResult move = MenuRepeat::movedIndex(0, 3, -1, false);
  TEST_ASSERT_EQUAL_UINT32(0u, move.index);
  TEST_ASSERT_FALSE(move.changed);

  move = MenuRepeat::movedIndex(2, 3, 1, false);
  TEST_ASSERT_EQUAL_UINT32(2u, move.index);
  TEST_ASSERT_FALSE(move.changed);

  move = MenuRepeat::movedIndex(0, 3, 1, false);
  TEST_ASSERT_EQUAL_UINT32(1u, move.index);
  TEST_ASSERT_TRUE(move.changed);
}

void test_moved_index_ignores_empty_or_zero_direction(void) {
  MenuRepeat::MoveResult move = MenuRepeat::movedIndex(4, 0, 1, false);
  TEST_ASSERT_EQUAL_UINT32(4u, move.index);
  TEST_ASSERT_FALSE(move.changed);

  move = MenuRepeat::movedIndex(1, 3, 0, true);
  TEST_ASSERT_EQUAL_UINT32(1u, move.index);
  TEST_ASSERT_FALSE(move.changed);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_repeat_delay_sanitizes_to_default);
  RUN_TEST(test_repeat_delay_accepts_configured_options);
  RUN_TEST(test_repeat_delay_cycles_through_ui_options);
  RUN_TEST(test_drag_direction_uses_vertical_threshold_and_bias);
  RUN_TEST(test_right_swipe_uses_horizontal_threshold_and_bias);
  RUN_TEST(test_moved_index_wraps_for_quick_swipe_behavior);
  RUN_TEST(test_moved_index_clamps_for_hold_repeat_behavior);
  RUN_TEST(test_moved_index_ignores_empty_or_zero_direction);

  return UNITY_END();
}
