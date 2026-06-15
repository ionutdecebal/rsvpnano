#include "board/BoardButtons.h"

namespace Board::Buttons {

bool readVirtualBootHeld() { return false; }
bool readVirtualPowerHeld() { return false; }
bool consumeVirtualPowerShortPress() { return false; }
bool consumeVirtualPowerLongPress() { return false; }
bool usesPowerEvents() { return Config::APP_POWER_BUTTON_USES_PMU_EVENTS; }
uint32_t powerEventIgnoreMs() { return Config::PMU_BOOT_BUTTON_IGNORE_MS; }

}  // namespace Board::Buttons
