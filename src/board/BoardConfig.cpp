#include "board/BoardConfig.h"

#include <Wire.h>

namespace BoardConfig {

void begin() {
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
}

}  // namespace BoardConfig
