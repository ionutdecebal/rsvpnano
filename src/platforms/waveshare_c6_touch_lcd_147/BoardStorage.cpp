#include "board/BoardStorage.h"

#include <SD.h>
#include <SPI.h>

#include "platforms/waveshare_c6_touch_lcd_147/WaveshareC6TouchLcd147.h"

namespace Board::Storage {

fs::FS &filesystem() { return SD; }

bool mount(const char *mountPoint, int frequencyKhz) {
  const uint32_t frequencyHz =
      frequencyKhz > 0 ? static_cast<uint32_t>(frequencyKhz) * 1000U
                       : WaveshareC6TouchLcd147::Storage::kDefaultFrequencyHz;
  SPI.begin(WaveshareC6TouchLcd147::Storage::kSclkPin, WaveshareC6TouchLcd147::Storage::kMisoPin,
            WaveshareC6TouchLcd147::Storage::kMosiPin, WaveshareC6TouchLcd147::Storage::kCsPin);
  return SD.begin(WaveshareC6TouchLcd147::Storage::kCsPin, SPI, frequencyHz, mountPoint, 5, false);
}

void end() { SD.end(); }

uint64_t cardSize() { return SD.cardSize(); }

uint8_t cardType() { return SD.cardType(); }

bool supportsFrequencySelection() { return false; }

bool setSdMmcPins() { return true; }

void configureSdMmcSlot(sdmmc_slot_config_t &slotConfig) { (void)slotConfig; }

}  // namespace Board::Storage
