#include "display/Panel.h"

#include <sdkconfig.h>

#if CONFIG_IDF_TARGET_ESP32C6
#include "display/jd9853.h"
#else
#include "display/axs15231b.h"
#endif

namespace Panel {

void init() {
#if CONFIG_IDF_TARGET_ESP32C6
  jd9853Init();
#else
  axs15231bInit();
#endif
}

void setBacklight(bool on) {
#if CONFIG_IDF_TARGET_ESP32C6
  jd9853SetBacklight(on);
#else
  axs15231bSetBacklight(on);
#endif
}

void setBrightnessPercent(uint8_t percent) {
#if CONFIG_IDF_TARGET_ESP32C6
  jd9853SetBrightnessPercent(percent);
#else
  axs15231bSetBrightnessPercent(percent);
#endif
}

void sleep() {
#if CONFIG_IDF_TARGET_ESP32C6
  jd9853Sleep();
#else
  axs15231bSleep();
#endif
}

void wake() {
#if CONFIG_IDF_TARGET_ESP32C6
  jd9853Wake();
#else
  axs15231bWake();
#endif
}

void pushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *data) {
#if CONFIG_IDF_TARGET_ESP32C6
  jd9853PushColors(x, y, width, height, data);
#else
  axs15231bPushColors(x, y, width, height, data);
#endif
}

}  // namespace Panel
