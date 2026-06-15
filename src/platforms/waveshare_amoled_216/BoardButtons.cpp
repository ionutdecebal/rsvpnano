#include "board/BoardButtons.h"

#include "drivers/power/axp2101/Axp2101.h"

namespace Board::Buttons {

bool readVirtualBootHeld() { return false; }

bool readVirtualPowerHeld() { return BoardDrivers::Axp2101::isPowerButtonHeld(); }

bool consumeVirtualPowerShortPress() { return BoardDrivers::Axp2101::consumeShortPress(); }

bool consumeVirtualPowerLongPress() { return BoardDrivers::Axp2101::consumeLongPress(); }

bool usesPowerEvents() { return Config::APP_POWER_BUTTON_USES_PMU_EVENTS; }

uint32_t powerEventIgnoreMs() { return Config::PMU_BOOT_BUTTON_IGNORE_MS; }

}  // namespace Board::Buttons
