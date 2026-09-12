#include "board/BoardInput.h"

#include <array>
#include <atomic>

#include <Wire.h>

#include "drivers/touch/axs5106/axs5106.h"
#include "platforms/waveshare_c6_touch_lcd_147/WaveshareC6TouchLcd147.h"

namespace {

    // Native word loads/stores only: ESP32 builds disable hardware RMW atomics (exchange/fetch/CAS).
    std::atomic<uint32_t> gTouchPending = 0;
    bool gTouchInterruptAttached = false;
    static_assert(sizeof(gTouchPending) == sizeof(uint32_t) && alignof(decltype(gTouchPending)) >= sizeof(uint32_t));

    void IRAM_ATTR onTouchInterrupt() {
        gTouchPending.store(true);
        ::Input::notifyTouchFromISR();
    }

    TwoWire& touchWire() {
        return Wire;
    }

    void resetTouchHardware() {
        if constexpr (WaveshareC6TouchLcd147::System::kTouchResetPin >= 0) {
            pinMode(WaveshareC6TouchLcd147::System::kTouchResetPin, OUTPUT);
            digitalWrite(WaveshareC6TouchLcd147::System::kTouchResetPin, LOW);
            delay(10);
            digitalWrite(WaveshareC6TouchLcd147::System::kTouchResetPin, HIGH);
            delay(10);
        }
    }

    bool primaryPressedRaw() {
        if constexpr (WaveshareC6TouchLcd147::Buttons::kBootPin < 0) {
            return false;
        }
        return !digitalRead(WaveshareC6TouchLcd147::Buttons::kBootPin);
    }

    void configureButtonPins() {
        if constexpr (WaveshareC6TouchLcd147::Buttons::kBootPin >= 0) {
            pinMode(WaveshareC6TouchLcd147::Buttons::kBootPin, INPUT_PULLUP);
        }
    }

} // namespace

namespace Board::Input {

    bool begin() {
        configureButtonPins();
        return true;
    }

    void end() {
        detachInterrupt(WaveshareC6TouchLcd147::System::kTouchIrqPin);
        gTouchInterruptAttached = false;
        gTouchPending.store(false);
    }

    void cancel() {
        // Light sleep changes the GPIO interrupt to level-triggered; never leave the edge ISR installed.
        end();
    }

    ::Input::ControlTiming controlTiming() {
        return {};
    }

    ::Input::PressActions currentActions() {
        ::Input::PressActions actions = {};
        if (primaryPressedRaw()) {
            actions.shortPress |= ::Input::ActionSelect | ::Input::ActionPlayPause;
            actions.longPress |= ::Input::ActionStandby;
        }
        return actions;
    }

    ui::TouchSurface touchSurface() {
        return {WaveshareC6TouchLcd147::DisplayWiring::kPanelWidth,
                WaveshareC6TouchLcd147::DisplayWiring::kPanelHeight};
    }

    ::Input::TouchTiming touchTiming() {
        return {};
    }

    bool beginTouch() {
        detachInterrupt(WaveshareC6TouchLcd147::System::kTouchIrqPin);
        gTouchInterruptAttached = false;
        gTouchPending.store(false);
        resetTouchHardware();
        if (!Axs5106Touch::probe(touchWire(), WaveshareC6TouchLcd147::TouchWiring::kAddress))
            return false;
        pinMode(WaveshareC6TouchLcd147::System::kTouchIrqPin, INPUT_PULLUP);
        attachInterrupt(WaveshareC6TouchLcd147::System::kTouchIrqPin, onTouchInterrupt, FALLING);
        gTouchInterruptAttached = true;
        if (!digitalRead(WaveshareC6TouchLcd147::System::kTouchIrqPin))
            gTouchPending.store(true);
        return true;
    }

    bool touchReady() {
        // While paused, a GPIO wake is enough to justify one report even after its pulse has ended.
        return !gTouchInterruptAttached || gTouchPending.load();
    }

    bool readTouch(ui::TouchContact& contact) {
        // Consume before I2C so an IRQ arriving during the read remains pending.
        gTouchPending.store(false);
        std::array<uint8_t, Axs5106Touch::kPacketLength> data = {};
        if (!Axs5106Touch::readPacket(touchWire(), WaveshareC6TouchLcd147::TouchWiring::kAddress,
                                      WaveshareC6TouchLcd147::TouchWiring::kReleaseBusBeforeRead, data.data(),
                                      data.size())) {
            gTouchPending.store(true);
            return false;
        }

        BoardDrivers::Touch::Sample decoded = {};
        if (!Axs5106Touch::decodePacket(data.data(), data.size(), WaveshareC6TouchLcd147::DisplayWiring::kPanelWidth,
                                        WaveshareC6TouchLcd147::DisplayWiring::kPanelHeight, decoded)) {
            gTouchPending.store(true);
            return false;
        }

        contact = {decoded.touched, decoded.physicalX, decoded.physicalY};
        return true;
    }

} // namespace Board::Input
