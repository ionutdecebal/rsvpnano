#include <Wire.h>
#include <unity.h>

#include "platforms/waveshare_amoled_18/BoardDisplayPower.h"
#include "platforms/waveshare_amoled_18/WaveshareAmoled18.h"
#include "platforms/waveshare_c6_touch_lcd_147/WaveshareC6TouchLcd147.h"

using namespace WaveshareAmoled18;

static_assert(Tca9554Wiring::kLcdResetPin == 0);
static_assert(Tca9554Wiring::kDisplayEnablePin == 1);
static_assert(Tca9554Wiring::kTouchResetPin == 2);
static_assert(Tca9554Wiring::kSdCsPin == 7);
static_assert(WaveshareC6TouchLcd147::Buttons::kBootPin == 9);
static_assert(WaveshareC6TouchLcd147::System::kLightSleepWakeGpio == WaveshareC6TouchLcd147::Buttons::kBootPin);

void setUp() {
    Wire = {};
}
void tearDown() {}

void test_touch_reset_preserves_outputs_and_directions() {
    // A separate output changed while reset was low: the second edge must read
    // the current latch rather than overwrite it with the pre-reset snapshot.
    Wire.responses = {{0xAF}, {0x78}, {0xEB}, {0x78}};
    TEST_ASSERT_TRUE(DisplayPower::resetTouchHardware());
    const std::vector<std::vector<uint8_t>> expected = {
        {1}, {1, 0xAB}, {3}, {3, 0x78}, {1}, {1, 0xEF}, {3}, {3, 0x78},
    };
    TEST_ASSERT_TRUE(Wire.writes == expected);
}

void test_touch_reset_rejects_failed_port_read() {
    Wire.transmissionStatus = 2;
    TEST_ASSERT_FALSE(DisplayPower::resetTouchHardware());
    TEST_ASSERT_EQUAL_UINT32(1, Wire.writes.size());
}

void test_display_startup_propagates_expander_failure() {
    Wire.transmissionStatus = 2;
    TEST_ASSERT_FALSE(DisplayPower::releaseHardware());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_touch_reset_preserves_outputs_and_directions);
    RUN_TEST(test_touch_reset_rejects_failed_port_read);
    RUN_TEST(test_display_startup_propagates_expander_failure);
    return UNITY_END();
}
