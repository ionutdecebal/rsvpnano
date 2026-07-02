#include "platforms/lilygo_tlora_pager/TPagerHardware.h"

namespace tpager {
namespace {
bool gBegun = false;
}  // namespace

LilyGoLoRaPager &hw() { return instance; }

void ensureBegun() {
  if (gBegun) {
    return;
  }
  // Full BSP init: display, I2C bus, XL9555 expander, fuel gauge, PMU, rotary
  // encoder + keyboard background tasks, codec, haptics, SD power rail, radio.
  instance.begin();
  gBegun = true;

  // Power-gate the radios this app never uses, to cut idle battery draw.
  // LilyGo_LoRa_Pager::begin() unconditionally powers up and initializes the
  // SX1262 LoRa radio, the GPS module and the NFC front-end. RSVP Nano is a
  // reader; it uses none of them. Put the LoRa radio into its ~1uA sleep state
  // (the same call LilyGoLib makes before light sleep — it releases the shared
  // SPI bus cleanly, which is safer than cutting POWER_RADIO out from under the
  // bus) and cut the GPS and NFC power rails outright.
  radio.sleep();
  instance.powerControl(POWER_GPS, false);
  instance.powerControl(POWER_NFC, false);
}

}  // namespace tpager

// LilyGoLib's LilyGo_LoRa_Pager::begin() unconditionally *references* setupMSC()
// (USB Mass Storage), even though it only *calls* it when NO_INIT_FATFS is clear.
// This variant disables USB transfer and excludes LilyGoLib's USB_MSC.cpp from the
// build (see README-tpager.md), which removes the definition -- so provide a no-op
// here to satisfy the linker without pulling in TinyUSB/USBMSC. The signature must
// match LilyGoLib's `extern void setupMSC(lock_callback_t, lock_callback_t)`, where
// lock_callback_t is `bool (*)(void)`.
void setupMSC(bool (*lock_cb)(void), bool (*unlock_cb)(void)) {
  (void)lock_cb;
  (void)unlock_cb;
}
