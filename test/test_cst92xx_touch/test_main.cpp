#include <unity.h>

#include <array>

#include "drivers/touch/cst92xx/cst92xx.h"

// Exercise the actual board IRQ lifecycle, with only GPIO and power-button hardware replaced.
#define IRAM_ATTR
constexpr int OUTPUT = 1, INPUT_PULLUP = 2, LOW = 0, HIGH = 1, FALLING = 3;
static void (*irqHandler)() = nullptr;
static int irqLevel = HIGH;
void pinMode(int, int) {}
void digitalWrite(int, int) {}
int digitalRead(int) {
    return irqLevel;
}
void attachInterrupt(int, void (*handler)(), int) {
    irqHandler = handler;
}
void detachInterrupt(int) {
    irqHandler = nullptr;
}
namespace Board::Power {
    bool powerButtonHeld() {
        return false;
    }
} // namespace Board::Power
namespace Input {
    void notifyTouchFromISR() {}
} // namespace Input
#include "../../src/platforms/waveshare_amoled_216/BoardInput.cpp"

namespace {
    using Packet = std::array<uint8_t, Cst92xxTouch::kPacketLength>;

    TwoWire readyController() {
        TwoWire wire;
        wire.responses = {{0x00, 0x00, 0xCA, 0xCA},
                          {0xE0, 0x01, 0xE0, 0x01},
                          {0x01, 0x00, 0x20, 0x92},
                          {0x01, 0x00, 0x00, 0x00, 0, 0, 0, 0}};
        return wire;
    }

    bool decode(const Packet& packet, BoardDrivers::Touch::Sample& sample) {
        return Cst92xxTouch::decodePacket(packet.data(), packet.size(), 480, 480, sample);
    }
} // namespace

void setUp() {
    irqLevel = HIGH;
}
void tearDown() {
    Board::Input::end();
}

void test_initialization_uses_controller_command_space() {
    auto wire = readyController();
    TEST_ASSERT_TRUE(Cst92xxTouch::begin(wire, 0x5A));
    const std::vector<std::vector<uint8_t>> expected = {{0xD1, 0x01},
                                                        {0xD1, 0xFC},
                                                        {0xD1, 0xF8},
                                                        {0xD2, 0x04},
                                                        {0xD2, 0x08}};
    TEST_ASSERT_TRUE(wire.writes == expected);
}

void test_initialization_rejects_invalid_identity_and_firmware() {
    auto wire = readyController();
    wire.responses[0][3] = 0;
    TEST_ASSERT_FALSE(Cst92xxTouch::begin(wire, 0x5A));
    wire = readyController();
    wire.responses[2][2] = 0;
    TEST_ASSERT_FALSE(Cst92xxTouch::begin(wire, 0x5A));
    wire = readyController();
    wire.responses[3] = {0xA5, 0xA5, 0xA5, 0xA5, 0, 0, 0, 0};
    TEST_ASSERT_FALSE(Cst92xxTouch::begin(wire, 0x5A));
    wire = readyController();
    wire.transmissionStatus = 2;
    TEST_ASSERT_FALSE(Cst92xxTouch::begin(wire, 0x5A));
}

void test_decodes_contact_and_valid_release() {
    BoardDrivers::Touch::Sample sample;
    Packet packet = {0x06, 0x12, 0x1D, 0x3F, 0, 1, 0xAB};
    TEST_ASSERT_TRUE(decode(packet, sample));
    TEST_ASSERT_TRUE(sample.touched);
    TEST_ASSERT_EQUAL_UINT16(291, sample.physicalX);
    TEST_ASSERT_EQUAL_UINT16(479, sample.physicalY);
    packet[5] = 0;
    TEST_ASSERT_TRUE(decode(packet, sample));
    TEST_ASSERT_FALSE(sample.touched);
}

void test_malformed_reports_preserve_contact() {
    BoardDrivers::Touch::Sample sample = {.touched = true, .physicalX = 120, .physicalY = 240};
    Packet packet = {0x06, 0, 0, 0, 0, 1, 0};
    TEST_ASSERT_FALSE(decode(packet, sample));
    packet[6] = 0xAB;
    packet[5] = 3;
    TEST_ASSERT_FALSE(decode(packet, sample));
    packet[5] = 1;
    packet[0] = 0x26;
    TEST_ASSERT_FALSE(decode(packet, sample));
    TEST_ASSERT_FALSE(Cst92xxTouch::decodePacket(packet.data(), 6, 480, 480, sample));
    TEST_ASSERT_TRUE(sample.touched);
    TEST_ASSERT_EQUAL_UINT16(120, sample.physicalX);
    TEST_ASSERT_EQUAL_UINT16(240, sample.physicalY);
}

void test_two_fingers_does_not_manufacture_release() {
    BoardDrivers::Touch::Sample sample;
    const Packet packet = {0x16, 0x12, 0x12, 0, 0, 2, 0xAB};
    TEST_ASSERT_TRUE(decode(packet, sample));
    TEST_ASSERT_TRUE(sample.touched);
}

void test_board_retains_irq_during_read_and_retries_failures() {
    Wire = readyController();
    Wire.responses.push_back({0x06, 0x12, 0x12, 0, 0, 1, 0xAB, 0, 0, 0});
    TEST_ASSERT_TRUE(Board::Input::beginTouch());
    TEST_ASSERT_FALSE(Board::Input::touchReady());
    irqHandler();
    TEST_ASSERT_TRUE(Board::Input::touchReady());
    TEST_ASSERT_TRUE(Board::Input::touchReady());
    Wire.onRequest = [] {
        irqHandler();
    };
    ui::TouchContact contact;
    TEST_ASSERT_TRUE(Board::Input::readTouch(contact));
    TEST_ASSERT_TRUE(Board::Input::touchReady());
    Wire.onRequest = nullptr;
    Wire.transmissionStatus = 2;
    TEST_ASSERT_FALSE(Board::Input::readTouch(contact));
    TEST_ASSERT_TRUE(Board::Input::touchReady());
    TEST_ASSERT_TRUE(contact.touched);
}

void test_board_detaches_for_sleep_and_rearms_asserted_line() {
    Wire = readyController();
    TEST_ASSERT_TRUE(Board::Input::beginTouch());
    TEST_ASSERT_NOT_NULL(irqHandler);
    Board::Input::cancel();
    TEST_ASSERT_NULL(irqHandler);
    // A completed GPIO wake may outlive its pulse; the paused reader gets one acquisition attempt.
    TEST_ASSERT_TRUE(Board::Input::touchReady());
    Wire = readyController();
    irqLevel = LOW;
    TEST_ASSERT_TRUE(Board::Input::beginTouch());
    TEST_ASSERT_NOT_NULL(irqHandler);
    TEST_ASSERT_TRUE(Board::Input::touchReady());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_initialization_uses_controller_command_space);
    RUN_TEST(test_initialization_rejects_invalid_identity_and_firmware);
    RUN_TEST(test_decodes_contact_and_valid_release);
    RUN_TEST(test_malformed_reports_preserve_contact);
    RUN_TEST(test_two_fingers_does_not_manufacture_release);
    RUN_TEST(test_board_retains_irq_during_read_and_retries_failures);
    RUN_TEST(test_board_detaches_for_sleep_and_rearms_asserted_line);
    return UNITY_END();
}
