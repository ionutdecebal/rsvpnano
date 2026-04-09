#include "storage/StorageManager.h"

#include <SD.h>
#include <SPI.h>

#include "board/BoardConfig.h"

bool StorageManager::begin() {
  mounted_ = SD.begin(BoardConfig::PIN_SD_CS);
  if (!mounted_) {
    Serial.println("[storage] SD init failed");
    return false;
  }

  Serial.println("[storage] SD initialized");
  return true;
}

void StorageManager::listBooks() {
  if (!mounted_ || listedOnce_) {
    return;
  }
  listedOnce_ = true;

  File dir = SD.open("/books");
  if (!dir || !dir.isDirectory()) {
    Serial.println("[storage] /books directory not found");
    return;
  }

  Serial.println("[storage] Listing /books:");
  File entry = dir.openNextFile();
  while (entry) {
    Serial.printf("  %s\n", entry.name());
    entry.close();
    entry = dir.openNextFile();
  }
}
