#include "board/BoardStorage.h"

#include "platforms/waveshare_amoled_206/WaveshareAmoled206.h"

namespace Board::Storage {

bool setSdMmcPins() {
  return SD_MMC.setPins(WaveshareAmoled206::Storage::kSdClockPin,
                        WaveshareAmoled206::Storage::kSdCommandPin,
                        WaveshareAmoled206::Storage::kSdData0Pin);
}

void configureSdMmcSlot(sdmmc_slot_config_t &slotConfig) {
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
  slotConfig.clk = WaveshareAmoled206::Storage::kSdClockPin;
  slotConfig.cmd = WaveshareAmoled206::Storage::kSdCommandPin;
  slotConfig.d0 = WaveshareAmoled206::Storage::kSdData0Pin;
  slotConfig.d1 = WaveshareAmoled206::Storage::kSdData1Pin;
  slotConfig.d2 = WaveshareAmoled206::Storage::kSdData2Pin;
  slotConfig.d3 = WaveshareAmoled206::Storage::kSdData3Pin;
#else
  (void)slotConfig;
#endif
}

}  // namespace Board::Storage
