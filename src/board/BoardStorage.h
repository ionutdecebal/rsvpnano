#pragma once

#include <Arduino.h>
#include <FS.h>

#ifndef RSVP_USB_TRANSFER_ENABLED
#define RSVP_USB_TRANSFER_ENABLED 0
#endif

#if RSVP_USB_TRANSFER_ENABLED
#include <driver/sdmmc_host.h>
#endif

namespace Board::Storage {

    enum class CardType : uint8_t {
        None = 0,
        Mmc = 1,
        Sd = 2,
        Sdhc = 3,
    };

    fs::FS& filesystem();
    bool mount(const char* mountPoint, int frequencyKhz);
    void end();
    uint64_t cardSize();
    CardType cardType();
    bool supportsFrequencySelection();
    bool setSdMmcPins();
#if RSVP_USB_TRANSFER_ENABLED
    void configureSdMmcSlot(sdmmc_slot_config_t& slotConfig);
#endif

} // namespace Board::Storage
