#include "board/BoardStorage.h"

#include <SD.h>

#include "platforms/lilygo_tlora_pager/TPagerHardware.h"

// LilyGo T-LoRa-Pager storage backend.
//
// The SD card shares the main SPI bus and is mounted by the LilyGoLib BSP
// (installSD()), not the ESP32 SDMMC peripheral. LilyGoLib populates the Arduino
// `SD` object, which exposes the same fs::FS API the shared storage code uses.
//
// Because the card is SPI-mounted at a LilyGoLib-fixed rate, this board reports
// supportsFrequencySelection()==false; the shared SdDiagnostics mount path then
// calls mount() once with a default frequency (which we ignore) and never probes
// SDMMC frequencies or pins. No SDMMC slot is configured, so USB mass storage
// (which drives the SDMMC host directly) is disabled for this board -- see
// platformio.ini / README-tpager.md.

namespace Board::Storage {

fs::FS &filesystem() { return SD; }

bool mount(const char * /*mountPoint*/, int /*frequencyKhz*/) {
  // installSD() is idempotent: it powers the SD rail, claims the shared SPI bus
  // and mounts the card into the Arduino `SD` object, returning true once ready.
  tpager::ensureBegun();
  return tpager::hw().installSD();
}

void end() { SD.end(); }

uint64_t cardSize() { return SD.cardSize(); }

CardType cardType() { return static_cast<CardType>(SD.cardType()); }

bool supportsFrequencySelection() { return false; }

bool setSdMmcPins() { return false; }

}  // namespace Board::Storage
