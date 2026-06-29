#include "board/BoardStorage.h"

#include <SD_MMC.h>

#include "platforms/waveshare_amoled_241/WaveshareAmoled241.h"

namespace Board::Storage {

fs::FS &filesystem() { return SD_MMC; }

bool mount(const char *mountPoint, int frequencyKhz) {
  return SD_MMC.begin(mountPoint, true, false, frequencyKhz, 5);
}

void end() { SD_MMC.end(); }

uint64_t cardSize() { return SD_MMC.cardSize(); }

CardType cardType() { return static_cast<CardType>(SD_MMC.cardType()); }

bool supportsFrequencySelection() { return true; }

bool setSdMmcPins() {
  return SD_MMC.setPins(WaveshareAmoled241::Storage::kSdClockPin,
                        WaveshareAmoled241::Storage::kSdCommandPin,
                        WaveshareAmoled241::Storage::kSdData0Pin);
}

#if RSVP_USB_TRANSFER_ENABLED
void configureSdMmcSlot(sdmmc_slot_config_t &slotConfig) {
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
  slotConfig.clk = WaveshareAmoled241::Storage::kSdClockPin;
  slotConfig.cmd = WaveshareAmoled241::Storage::kSdCommandPin;
  slotConfig.d0 = WaveshareAmoled241::Storage::kSdData0Pin;
  slotConfig.d1 = WaveshareAmoled241::Storage::kSdData1Pin;
  slotConfig.d2 = WaveshareAmoled241::Storage::kSdData2Pin;
  slotConfig.d3 = WaveshareAmoled241::Storage::kSdData3Pin;
#else
  (void)slotConfig;
#endif
}
#endif

}  // namespace Board::Storage
