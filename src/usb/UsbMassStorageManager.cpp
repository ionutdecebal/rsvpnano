#include "usb/UsbMassStorageManager.h"

#include <algorithm>
#include <cstring>

#include <esp_heap_caps.h>
#include <ff.h>
#include <diskio.h>
#include <diskio_impl.h>
#include <tusb.h>

#if RSVP_USB_TRANSFER_ENABLED && CONFIG_TINYUSB_MSC_ENABLED && !ARDUINO_USB_MODE
#include <USB.h>
#endif

namespace {

constexpr uint16_t kUsbBlockSize = 512;

void pulseUsbReconnect() {
#if RSVP_USB_TRANSFER_ENABLED && CONFIG_TINYUSB_MSC_ENABLED && !ARDUINO_USB_MODE
  if (!tud_inited()) {
    return;
  }

  tud_disconnect();
  delay(120);
  tud_connect();
#endif
}

}  // namespace

UsbMassStorageManager *UsbMassStorageManager::instance_ = nullptr;

UsbMassStorageManager::UsbMassStorageManager() {
  instance_ = this;
  configureMsc();
}

bool UsbMassStorageManager::begin(bool writeEnabled) {
#if RSVP_USB_TRANSFER_ENABLED && CONFIG_TINYUSB_MSC_ENABLED && !ARDUINO_USB_MODE
  if (active_) {
    return true;
  }

  writeEnabled_ = writeEnabled;
  ejected_ = false;
  statusMessage_ = "Preparing SD";

  if (!beginSdCard()) {
    endSdCard();
    return false;
  }

  if (!configureMsc() || !msc_.begin(blockCount_, blockSize_)) {
    statusMessage_ = "USB MSC failed";
    endSdCard();
    return false;
  }

  if (!USB.begin()) {
    statusMessage_ = "USB start failed";
    msc_.end();
    endSdCard();
    return false;
  }

  active_ = true;
  statusMessage_ = writeEnabled_ ? "Mounted read/write" : "Mounted read-only";
  msc_.mediaPresent(true);
  pulseUsbReconnect();
  Serial.printf("[usb-msc] active blocks=%lu blockSize=%u write=%u\n",
                static_cast<unsigned long>(blockCount_), blockSize_, writeEnabled_ ? 1 : 0);
  return true;
#else
  (void)writeEnabled;
  statusMessage_ = "USB transfer disabled";
  Serial.println("[usb-msc] unsupported: build a USB-enabled target to enable it");
  return false;
#endif
}

void UsbMassStorageManager::end() {
#if RSVP_USB_TRANSFER_ENABLED && CONFIG_TINYUSB_MSC_ENABLED && !ARDUINO_USB_MODE
  msc_.mediaPresent(false);
  msc_.end();
#endif
  endSdCard();
  active_ = false;
  ejected_ = false;
  writeEnabled_ = false;
  statusMessage_ = "Idle";
}

bool UsbMassStorageManager::active() const { return active_; }

bool UsbMassStorageManager::ejected() const { return ejected_; }

bool UsbMassStorageManager::writeEnabled() const { return writeEnabled_; }

uint64_t UsbMassStorageManager::cardSizeBytes() const {
  return static_cast<uint64_t>(blockCount_) * blockSize_;
}

const char *UsbMassStorageManager::statusMessage() const { return statusMessage_; }

bool UsbMassStorageManager::configureMsc() {
#if RSVP_USB_TRANSFER_ENABLED && CONFIG_TINYUSB_MSC_ENABLED && !ARDUINO_USB_MODE
  msc_.vendorID("RSVPNANO");
  msc_.productID("SD Transfer");
  msc_.productRevision("0.1");
  msc_.onRead(onRead);
  msc_.onWrite(onWrite);
  msc_.onStartStop(onStartStop);
  msc_.mediaPresent(false);
  return true;
#else
  return false;
#endif
}

bool UsbMassStorageManager::beginSdCard() {
  blockCount_ = 0;
  blockSize_ = kUsbBlockSize;
  cardReady_ = false;
  physicalDrive_ = 0xFF;

  if (sectorBuffer_ == nullptr) {
    sectorBuffer_ = static_cast<uint8_t *>(
        heap_caps_malloc(kUsbBlockSize, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  }
  if (sectorBuffer_ == nullptr) {
    Serial.println("[usb-msc] failed to allocate DMA sector buffer");
    statusMessage_ = "Buffer allocation failed";
    return false;
  }

  BYTE firstUnusedDrive = FF_VOLUMES;
  if (ff_diskio_get_drive(&firstUnusedDrive) != ESP_OK) {
    firstUnusedDrive = FF_VOLUMES;
  }

  for (BYTE drive = 0; drive < firstUnusedDrive; ++drive) {
    DWORD sectorCount = 0;
    WORD sectorSize = 0;
    const DRESULT countResult = disk_ioctl(drive, GET_SECTOR_COUNT, &sectorCount);
    const DRESULT sizeResult = disk_ioctl(drive, GET_SECTOR_SIZE, &sectorSize);
    if (countResult != RES_OK || sizeResult != RES_OK || sectorCount == 0) {
      Serial.printf("[usb-msc] FatFS drive %u unavailable count=%u size=%u sectors=%lu\n",
                    drive, static_cast<unsigned int>(countResult),
                    static_cast<unsigned int>(sizeResult),
                    static_cast<unsigned long>(sectorCount));
      continue;
    }

    if (sectorSize != kUsbBlockSize) {
      Serial.printf("[usb-msc] unsupported SD geometry on drive %u: sectors=%lu sectorSize=%u\n",
                    drive, static_cast<unsigned long>(sectorCount),
                    static_cast<unsigned int>(sectorSize));
      statusMessage_ = "Unsupported SD geometry";
      continue;
    }

    physicalDrive_ = drive;
    blockCount_ = static_cast<uint32_t>(sectorCount);
    blockSize_ = static_cast<uint16_t>(sectorSize);
    cardReady_ = true;
    Serial.printf("[usb-msc] FatFS drive %u ready for USB (%lu MB)\n", physicalDrive_,
                  static_cast<unsigned long>(cardSizeBytes() / (1024ULL * 1024ULL)));
    return true;
  }

  statusMessage_ = "No mounted SD drive";
  Serial.println("[usb-msc] no mounted FatFS SD drive found for USB transfer");
  return false;
}

void UsbMassStorageManager::endSdCard() {
  if (cardReady_ && physicalDrive_ != 0xFF) {
    disk_ioctl(physicalDrive_, CTRL_SYNC, nullptr);
  }
  cardReady_ = false;
  physicalDrive_ = 0xFF;
  blockCount_ = 0;

  if (sectorBuffer_ != nullptr) {
    heap_caps_free(sectorBuffer_);
    sectorBuffer_ = nullptr;
  }
}

int32_t UsbMassStorageManager::onRead(uint32_t lba, uint32_t offset, void *buffer,
                                      uint32_t bufsize) {
  if (instance_ == nullptr) {
    return -1;
  }
  return instance_->readSectors(lba, offset, buffer, bufsize);
}

int32_t UsbMassStorageManager::onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer,
                                       uint32_t bufsize) {
  if (instance_ == nullptr) {
    return -1;
  }
  return instance_->writeSectors(lba, offset, buffer, bufsize);
}

bool UsbMassStorageManager::onStartStop(uint8_t powerCondition, bool start, bool loadEject) {
  if (instance_ == nullptr) {
    return false;
  }
  return instance_->handleStartStop(powerCondition, start, loadEject);
}

int32_t UsbMassStorageManager::readSectors(uint32_t lba, uint32_t offset, void *buffer,
                                           uint32_t bufsize) {
  if (!active_ || !cardReady_ || buffer == nullptr || sectorBuffer_ == nullptr ||
      offset >= blockSize_) {
    return -1;
  }

  uint8_t *out = static_cast<uint8_t *>(buffer);
  uint32_t copied = 0;
  uint32_t currentLba = lba;
  uint32_t currentOffset = offset;

  while (copied < bufsize && currentLba < blockCount_) {
    const uint32_t bytesThisSector =
        std::min<uint32_t>(blockSize_ - currentOffset, bufsize - copied);
    const DRESULT result = disk_read(physicalDrive_, sectorBuffer_, currentLba, 1);
    if (result != RES_OK) {
      Serial.printf("[usb-msc] read failed drive=%u lba=%lu result=%u\n", physicalDrive_,
                    static_cast<unsigned long>(currentLba),
                    static_cast<unsigned int>(result));
      return copied > 0 ? static_cast<int32_t>(copied) : -1;
    }

    std::memcpy(out + copied, sectorBuffer_ + currentOffset, bytesThisSector);
    copied += bytesThisSector;
    currentOffset = 0;
    ++currentLba;
  }

  return static_cast<int32_t>(copied);
}

int32_t UsbMassStorageManager::writeSectors(uint32_t lba, uint32_t offset, uint8_t *buffer,
                                            uint32_t bufsize) {
  if (!writeEnabled_) {
    return -1;
  }
  if (!active_ || !cardReady_ || buffer == nullptr || sectorBuffer_ == nullptr ||
      offset >= blockSize_) {
    return -1;
  }

  uint32_t written = 0;
  uint32_t currentLba = lba;
  uint32_t currentOffset = offset;

  while (written < bufsize && currentLba < blockCount_) {
    const uint32_t bytesThisSector =
        std::min<uint32_t>(blockSize_ - currentOffset, bufsize - written);
    if (currentOffset != 0 || bytesThisSector != blockSize_) {
      const DRESULT readResult = disk_read(physicalDrive_, sectorBuffer_, currentLba, 1);
      if (readResult != RES_OK) {
        Serial.printf("[usb-msc] write pre-read failed drive=%u lba=%lu result=%u\n",
                      physicalDrive_, static_cast<unsigned long>(currentLba),
                      static_cast<unsigned int>(readResult));
        return written > 0 ? static_cast<int32_t>(written) : -1;
      }
    }

    std::memcpy(sectorBuffer_ + currentOffset, buffer + written, bytesThisSector);

    const DRESULT writeResult = disk_write(physicalDrive_, sectorBuffer_, currentLba, 1);
    if (writeResult != RES_OK) {
      Serial.printf("[usb-msc] write failed drive=%u lba=%lu result=%u\n", physicalDrive_,
                    static_cast<unsigned long>(currentLba),
                    static_cast<unsigned int>(writeResult));
      return written > 0 ? static_cast<int32_t>(written) : -1;
    }

    written += bytesThisSector;
    currentOffset = 0;
    ++currentLba;
  }

  return static_cast<int32_t>(written);
}

bool UsbMassStorageManager::handleStartStop(uint8_t powerCondition, bool start, bool loadEject) {
  Serial.printf("[usb-msc] start-stop power=%u start=%u eject=%u\n", powerCondition,
                start ? 1 : 0, loadEject ? 1 : 0);

  if (loadEject && !start) {
    ejected_ = true;
    statusMessage_ = "Ejected";
    if (cardReady_ && physicalDrive_ != 0xFF) {
      disk_ioctl(physicalDrive_, CTRL_SYNC, nullptr);
    }
#if RSVP_USB_TRANSFER_ENABLED && CONFIG_TINYUSB_MSC_ENABLED && !ARDUINO_USB_MODE
    msc_.mediaPresent(false);
#endif
  }

  return true;
}
