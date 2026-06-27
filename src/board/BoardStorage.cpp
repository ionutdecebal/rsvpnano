#include "board/BoardStorage.h"

#include <SD_MMC.h>
#include <driver/gpio.h>

#include "board/BoardConfig.h"

namespace Board::Storage {

bool setSdMmcPins() {
  return SD_MMC.setPins(Board::Config::PIN_SD_CLK, Board::Config::PIN_SD_CMD,
                        Board::Config::PIN_SD_D0);
}

void configureSdMmcSlot(sdmmc_slot_config_t &slotConfig) {
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
  slotConfig.clk = static_cast<gpio_num_t>(Board::Config::PIN_SD_CLK);
  slotConfig.cmd = static_cast<gpio_num_t>(Board::Config::PIN_SD_CMD);
  slotConfig.d0 = static_cast<gpio_num_t>(Board::Config::PIN_SD_D0);
  slotConfig.d1 = GPIO_NUM_NC;
  slotConfig.d2 = GPIO_NUM_NC;
  slotConfig.d3 = GPIO_NUM_NC;
#else
  (void)slotConfig;
#endif
}

}  // namespace Board::Storage
