#include "drivers/touch/cst92xx/cst92xx.h"

#include <algorithm>

namespace {

    constexpr uint16_t kReadCommand = 0xD000;
    constexpr uint8_t kAck = 0xAB;

    bool validAddress(uint8_t address) {
        return address <= 0x7F;
    }

    uint16_t clampPhysical(uint16_t value, uint16_t limit) {
        return limit == 0 ? 0 : std::min<uint16_t>(value, static_cast<uint16_t>(limit - 1));
    }

    bool readRegister(TwoWire& wire, uint8_t address, uint16_t reg, uint8_t* buffer, size_t len) {
        const uint8_t readCommand[] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg)};
        constexpr uint8_t kMaxRetries = 5;

        for (uint8_t retry = 0; retry < kMaxRetries; ++retry) {
            wire.beginTransmission(address);
            wire.write(readCommand, sizeof(readCommand));
            if (wire.endTransmission(true) != 0) {
                delay(3);
                continue;
            }

            delay(2);
            const size_t readLen = wire.requestFrom(address, static_cast<size_t>(len), true);
            if (readLen == len) {
                for (size_t i = 0; i < len; ++i) {
                    buffer[i] = wire.read();
                }
                return true;
            }
            while (wire.available() > 0) {
                wire.read();
            }
            delay(3);
        }

        return false;
    }

} // namespace

namespace Cst92xxTouch {

    bool begin(TwoWire& wire, uint8_t address) {
        if (!validAddress(address))
            return false;

        // Waveshare's 2.16 SensorLib getAttribute(): enter command mode, then identify the controller.
        // FA/40 belongs to the CST816 family, not this controller's 16-bit command space.
        constexpr uint8_t commandMode[] = {0xD1, 0x01};
        wire.beginTransmission(address);
        wire.write(commandMode, sizeof(commandMode));
        if (wire.endTransmission(true) != 0)
            return false;
        delay(10);

        uint8_t data[8] = {};
        if (!readRegister(wire, address, 0xD1FC, data, 4) || data[2] != 0xCA || data[3] != 0xCA)
            return false;
        if (!readRegister(wire, address, 0xD1F8, data, 4))
            return false;
        if (!readRegister(wire, address, 0xD204, data, 4))
            return false;
        const uint16_t chip = static_cast<uint16_t>((data[3] << 8) | data[2]);
        if (chip != 0x9220 && chip != 0x9217)
            return false;
        if (!readRegister(wire, address, 0xD208, data, sizeof(data)))
            return false;
        return !(data[0] == 0xA5 && data[1] == 0xA5 && data[2] == 0xA5 && data[3] == 0xA5);
    }

    bool readPacket(TwoWire& wire, uint8_t address, uint8_t* buffer, size_t len) {
        if (!validAddress(address) || buffer == nullptr || len < kPacketLength)
            return false;
        return readRegister(wire, address, kReadCommand, buffer, len);
    }

    bool decodePacket(const uint8_t* data, size_t len, uint16_t panelWidth, uint16_t panelHeight,
                      BoardDrivers::Touch::Sample& sample) {
        if (data == nullptr || len < kPacketLength) {
            return false;
        }

        // A corrupt report is not a release: keep the previous contact until a valid packet or recovery.
        if (data[6] != kAck)
            return false;

        const uint8_t points = data[5] & 0x7F;
        const uint8_t touchId = data[0] >> 4;
        const uint8_t event = data[0] & 0x0F;
        // The hardware supports two fingers; this application consumes only the first reported point.
        if (points > kMaxTouchPoints || (points != 0 && touchId >= kMaxTouchPoints))
            return false;
        if (points == 0 || event != 0x06) {
            sample.touched = false;
            return true;
        }

        sample.touched = true;
        sample.physicalX = clampPhysical(static_cast<uint16_t>((data[1] << 4) | (data[3] >> 4)), panelWidth);
        sample.physicalY = clampPhysical(static_cast<uint16_t>((data[2] << 4) | (data[3] & 0x0F)), panelHeight);
        return true;
    }

} // namespace Cst92xxTouch
