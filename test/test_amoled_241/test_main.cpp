#include <Wire.h>
#include <unity.h>

#include "board/BoardInput.h"
#include "platforms/waveshare_amoled_241/BoardConfig.h"
#include "platforms/waveshare_amoled_241/BoardDisplayPower.h"

using namespace WaveshareAmoled241;

#ifndef RSVP_TEST_AMOLED_241_V2
#define RSVP_TEST_AMOLED_241_V2 0
#endif
constexpr bool kV2 = RSVP_TEST_AMOLED_241_V2;

namespace Input {
    void notifyTouchFromISR() {
        ++FakeArduino::state.notifications;
    }
}

void setUp() {
    Board::Input::end();
    FakeArduino::state = {};
    Wire1 = {};
    Wire1.registers[0x20][1] = 0xA0;
    Wire1.registers[0x20][3] = 0x5F;
}
void tearDown() {
    Board::Input::end();
}

void pulseTouch() {
    auto& gpio = FakeArduino::state;
    TEST_ASSERT_NOT_NULL(gpio.interrupt);
    const auto transactions = Wire1.transactions;
    gpio.levels[3] = LOW;
    gpio.interrupt();
    gpio.levels[3] = HIGH;
    TEST_ASSERT_EQUAL_UINT32(transactions, Wire1.transactions); // No I2C in the ISR.
}

void setContact(bool touched, uint16_t x = 0, uint16_t y = 0) {
    auto& regs = Wire1.registers[0x38];
    regs[2] = touched ? 1 : 0;
    regs[3] = x >> 8;
    regs[4] = x & 0xFF;
    regs[5] = y >> 8;
    regs[6] = y & 0xFF;
}

void test_revision_wiring_and_identity() {
    TEST_ASSERT_EQUAL_INT(kV2 ? 3 : -1, System::kTouchIrqPin);
    TEST_ASSERT_EQUAL_INT(kV2 ? -1 : 3, System::kTouchResetPin);
    TEST_ASSERT_EQUAL_INT(kV2 ? -1 : 21, DisplayWiring::kResetPin);
    TEST_ASSERT_EQUAL_INT(kV2 ? -1 : 2, Tca9554Wiring::kTouchIrqPin);
    TEST_ASSERT_EQUAL_INT(kV2 ? 1 : -1, Tca9554Wiring::kTouchResetPin);
    TEST_ASSERT_EQUAL_INT(kV2 ? 0 : -1, Tca9554Wiring::kDisplayResetPin);
    TEST_ASSERT_EQUAL_INT(kV2 ? -1 : 1, Tca9554Wiring::kDisplayRailEnablePin);
    TEST_ASSERT_EQUAL_INT(kV2, Board::Config::HAS_LIGHT_SLEEP_TOUCH_IRQ);
    TEST_ASSERT_EQUAL_INT(600, Board::Config::DISPLAY_WIDTH);
    TEST_ASSERT_EQUAL_INT(450, Board::Config::DISPLAY_HEIGHT);
    TEST_ASSERT_EQUAL_STRING(kV2 ? "waveshare_esp32s3_touch_amoled_2_41_v2"
                                : "waveshare_esp32s3_touch_amoled_2_41", Board::Config::BOARD_ID);
    TEST_ASSERT_EQUAL_STRING(kV2 ? "rsvp-nano-esp32-s3-touch-amoled-2.41-v2-ota.bin"
                                : "rsvp-nano-esp32-s3-touch-amoled-2.41-ota.bin", Board::Config::OTA_ASSET_NAME);
}

void test_display_and_touch_reset_preserve_unrelated_outputs() {
    TEST_ASSERT_TRUE(DisplayPower::releaseHardware());
    TEST_ASSERT_EQUAL_HEX8(kV2 ? 0xA1 : 0xA2, Wire1.registers[0x20][1]);
    TEST_ASSERT_EQUAL_HEX8(kV2 ? 0x5E : 0x5D, Wire1.registers[0x20][3]);
    if (kV2) {
        TEST_ASSERT_TRUE((FakeArduino::state.delays == std::vector<unsigned long>{20, 20, 120}));
        // EXIO1 is TP_RESET, not the old V1 display enable: display init must not touch it.
        for (const auto& write : Wire1.writes)
            if (write[1] == 1) TEST_ASSERT_EQUAL_HEX8(0, write[2] & 0x02);
    }
    Wire1.registers[0x20][3] &= ~0x04; // V1 must restore EXIO2's input direction.
    TEST_ASSERT_TRUE(Board::Input::beginTouch());
    TEST_ASSERT_EQUAL_HEX8(kV2 ? 0xA3 : 0xA2, Wire1.registers[0x20][1]);
    TEST_ASSERT_EQUAL_HEX8(kV2 ? 0x58 : 0x5D, Wire1.registers[0x20][3]);
    TEST_ASSERT_EQUAL_INT(kV2 ? INPUT_PULLUP : OUTPUT, FakeArduino::state.modes[3]);
    if (kV2) {
        TEST_ASSERT_TRUE(FakeArduino::state.writes.empty());
        TEST_ASSERT_EQUAL_INT(-1, FakeArduino::state.modes[21]); // OLED_TE is not reset.
    } else {
        TEST_ASSERT_TRUE((FakeArduino::state.writes == std::vector<std::pair<int, int>>{{3, LOW}, {3, HIGH}}));
    }
    for (const auto& write : Wire1.writes)
        TEST_ASSERT_FALSE(write[0] == 0x38 && write[1] == 0xA5); // Preserve FT6336 active defaults.
}

void test_initial_contact_and_interrupt_pulse() {
    TEST_ASSERT_TRUE(Board::Input::beginTouch());
    TEST_ASSERT_EQUAL_UINT32(kV2 ? 1 : 0, FakeArduino::state.attaches);
    if (kV2) {
        TEST_ASSERT_EQUAL_INT(3, FakeArduino::state.interruptPin);
        TEST_ASSERT_EQUAL_INT(FALLING, FakeArduino::state.interruptMode);
    }
    TEST_ASSERT_TRUE(Board::Input::touchReady());
    setContact(true, 123, 456);
    ui::TouchContact contact;
    TEST_ASSERT_TRUE(Board::Input::readTouch(contact));
    TEST_ASSERT_TRUE(contact.touched);
    TEST_ASSERT_EQUAL_UINT16(123, contact.x);
    TEST_ASSERT_EQUAL_UINT16(456, contact.y);
    TEST_ASSERT_EQUAL_INT(!kV2, Board::Input::touchReady());
    if (kV2) pulseTouch();
    TEST_ASSERT_TRUE(Board::Input::touchReady()); // Pulse has already returned HIGH.
    setContact(false);
    TEST_ASSERT_TRUE(Board::Input::readTouch(contact));
    TEST_ASSERT_FALSE(contact.touched);
    TEST_ASSERT_EQUAL_INT(!kV2, Board::Input::touchReady());
    TEST_ASSERT_EQUAL_UINT32(kV2 ? 1 : 0, FakeArduino::state.notifications);
}

void test_interrupt_during_read_stays_pending() {
    if (!kV2) return;
    TEST_ASSERT_TRUE(Board::Input::beginTouch());
    ui::TouchContact contact;
    Wire1.onRequest = pulseTouch;
    TEST_ASSERT_TRUE(Board::Input::readTouch(contact));
    TEST_ASSERT_TRUE(Board::Input::touchReady());
    TEST_ASSERT_TRUE(Board::Input::readTouch(contact));
    TEST_ASSERT_FALSE(Board::Input::touchReady());
}

void test_read_failures_remain_retryable() {
    TEST_ASSERT_TRUE(Board::Input::beginTouch());
    ui::TouchContact contact;
    Wire1.transmissionStatus[0x38] = 2;
    TEST_ASSERT_FALSE(Board::Input::readTouch(contact));
    TEST_ASSERT_TRUE(Board::Input::touchReady());
    Wire1.transmissionStatus[0x38] = 0;
    Wire1.shortRead = true;
    TEST_ASSERT_FALSE(Board::Input::readTouch(contact));
    TEST_ASSERT_TRUE(Board::Input::touchReady());
    Wire1.shortRead = false;
    TEST_ASSERT_TRUE(Board::Input::readTouch(contact));
    TEST_ASSERT_EQUAL_INT(!kV2, Board::Input::touchReady());
}

void test_failed_startup_does_not_attach_interrupt() {
    Wire1.transmissionStatus[0x20] = 2;
    TEST_ASSERT_FALSE(DisplayPower::releaseHardware());
    TEST_ASSERT_FALSE(Board::Input::beginTouch());
    TEST_ASSERT_EQUAL_UINT32(0, FakeArduino::state.attaches);
    Wire1.transmissionStatus[0x20] = 0;
    Wire1.transmissionStatus[0x38] = 2;
    TEST_ASSERT_FALSE(Board::Input::beginTouch());
    TEST_ASSERT_EQUAL_UINT32(0, FakeArduino::state.attaches);
}

void test_cancel_detaches_and_resume_reattaches() {
    TEST_ASSERT_TRUE(Board::Input::beginTouch());
    if (kV2) pulseTouch();
    Board::Input::cancel();
    TEST_ASSERT_NULL(FakeArduino::state.interrupt);
    TEST_ASSERT_EQUAL_UINT32(kV2 ? 1 : 0, FakeArduino::state.detaches);
    TEST_ASSERT_TRUE(Board::Input::touchReady()); // A wake can be read while the sampler is paused.
    TEST_ASSERT_TRUE(Board::Input::beginTouch());
    TEST_ASSERT_EQUAL_UINT32(kV2 ? 2 : 0, FakeArduino::state.attaches);
    TEST_ASSERT_TRUE(Board::Input::touchReady());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_revision_wiring_and_identity);
    RUN_TEST(test_display_and_touch_reset_preserve_unrelated_outputs);
    RUN_TEST(test_initial_contact_and_interrupt_pulse);
    RUN_TEST(test_interrupt_during_read_stays_pending);
    RUN_TEST(test_read_failures_remain_retryable);
    RUN_TEST(test_failed_startup_does_not_attach_interrupt);
    RUN_TEST(test_cancel_detaches_and_resume_reattaches);
    return UNITY_END();
}
