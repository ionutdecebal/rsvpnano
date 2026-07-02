#include "board/BoardInput.h"

#include <Arduino.h>

#include "board/BoardConfig.h"
#include "platforms/lilygo_tlora_pager/TPagerHardware.h"

// LilyGo T-LoRa-Pager input backend (post-PR#120 Board::Input seam).
//
// This board has no touch panel and a single physical BOOT button, so it maps
// onto the generic Input:: state machine (input/Input.cpp) like this:
//
//   * Controls: the BOOT button (GPIO0, wired as PIN_PWR_BUTTON) reports as
//     InputPower. App turns an InputPower short press into open/close menu
//     (toggleMenuFromPowerButton) and a long press into the power-off confirm --
//     matching this board's single-button model.
//
//   * Touch: there is no touch hardware. Instead readTouch() synthesizes a touch
//     contact trajectory from the rotary encoder and its centre push, so the
//     generic poller derives the very same TouchStart/TouchMove/TouchEnd/Tapped
//     gestures a real panel would, and App's touch UI is reused unchanged:
//       rotate CW  -> synthetic swipe down -> menu next / reader WPM- / timer +
//       rotate CCW -> synthetic swipe up   -> menu prev / reader WPM+ / timer -
//       centre push-> synthetic tap        -> select / play-pause / start-stop
//     The contacts are emitted in logical (display) coordinates anchored at the
//     screen centre, so the gesture is independent of orientation; Board::Imu
//     reports the identity (Portrait) orientation, so the generic mapper passes
//     them through unrotated.
//
// Latency note: getRotary() blocks up to ~50 ms when the encoder queue is empty,
// so the idle loop runs at ~20 Hz. A small FIFO of pending synthetic contacts is
// drained first, so a multi-contact gesture still flushes without re-blocking.

namespace {

// Synthetic gesture geometry, in logical (display) pixels. The vertical delta
// must clear App's swipe threshold (kSwipeThresholdPx 40 + kAxisBiasPx 12) so it
// reads as a directional swipe rather than a tap.
constexpr int kCenterX = Board::Config::DISPLAY_WIDTH / 2;
constexpr int kCenterY = Board::Config::DISPLAY_HEIGHT / 2;
constexpr int kSynthSwipePx = 64;

// A gesture enqueues its touched samples plus two released samples; the generic
// poller needs releaseConfirmSamples (2) empty reads to close a touch, and the
// explicit releases keep consecutive rotary ticks from merging into one drag.
constexpr size_t kQueueCapacity = 16;

::Input::TouchContact gQueue[kQueueCapacity];
size_t gHead = 0;
size_t gCount = 0;
bool gInitialized = false;

void clearQueue() {
  gHead = 0;
  gCount = 0;
}

void enqueue(const ::Input::TouchContact &contact) {
  if (gCount >= kQueueCapacity) {
    return;  // Drop overflow; input is paced far slower than this fills.
  }
  gQueue[(gHead + gCount) % kQueueCapacity] = contact;
  ++gCount;
}

bool dequeue(::Input::TouchContact &contact) {
  if (gCount == 0) {
    return false;
  }
  contact = gQueue[gHead];
  gHead = (gHead + 1) % kQueueCapacity;
  --gCount;
  return true;
}

::Input::TouchContact touchedAt(int x, int y) {
  ::Input::TouchContact contact;
  contact.touched = true;
  contact.x = static_cast<uint16_t>(constrain(x, 0, Board::Config::DISPLAY_WIDTH - 1));
  contact.y = static_cast<uint16_t>(constrain(y, 0, Board::Config::DISPLAY_HEIGHT - 1));
  return contact;
}

::Input::TouchContact released() {
  ::Input::TouchContact contact;
  contact.touched = false;
  contact.x = static_cast<uint16_t>(kCenterX);
  contact.y = static_cast<uint16_t>(kCenterY);
  return contact;
}

// direction: +1 = swipe down (rotate CW), -1 = swipe up (rotate CCW).
void enqueueSwipe(int direction) {
  enqueue(touchedAt(kCenterX, kCenterY));
  enqueue(touchedAt(kCenterX, kCenterY + direction * kSynthSwipePx));
  enqueue(released());
  enqueue(released());
}

void enqueueTap() {
  enqueue(touchedAt(kCenterX, kCenterY));
  enqueue(released());
  enqueue(released());
}

bool bootButtonPressed() {
  if (Board::Config::PIN_PWR_BUTTON < 0) {
    return false;
  }
  return !digitalRead(Board::Config::PIN_PWR_BUTTON);  // Active-low BOOT (GPIO0).
}

}  // namespace

namespace Board::Input {

bool begin() {
  tpager::ensureBegun();
  if (Config::PIN_PWR_BUTTON >= 0) {
    pinMode(Config::PIN_PWR_BUTTON, INPUT_PULLUP);
  }
  tpager::hw().clearRotaryMsg();
  clearQueue();
  gInitialized = true;
  return true;
}

void end() {
  clearQueue();
  if (gInitialized) {
    tpager::hw().clearRotaryMsg();
  }
  gInitialized = false;
}

void cancel() { clearQueue(); }

::Input::ControlTiming controlTiming() {
  // Defaults: 25 ms debounce, <=700 ms short press, >=900 ms long press.
  return {};
}

::Input::ControlMask currentControls() {
  ::Input::ControlMask controls = ::Input::InputNone;
  if (bootButtonPressed()) {
    // The single BOOT button is the power button on this board: short = toggle
    // menu, long = power-off confirm (see App::handleInputEvent).
    controls |= ::Input::InputPower;
  }
  return controls;
}

::Input::TouchSurface touchSurface() {
  return {static_cast<uint16_t>(Board::Config::DISPLAY_WIDTH),
          static_cast<uint16_t>(Board::Config::DISPLAY_HEIGHT)};
}

::Input::TouchTiming touchTiming() {
  // Defaults are fine: 20 ms poll, 2-sample release confirm, 40/48 px swipe etc.
  return {};
}

bool beginTouch() {
  if (gInitialized) {
    tpager::hw().clearRotaryMsg();
  }
  clearQueue();
  return true;
}

bool touchReady() { return true; }

bool readTouch(::Input::TouchContact &contact) {
  contact = ::Input::TouchContact{};

  if (!gInitialized) {
    return true;  // No hardware failure; just report "no contact".
  }

  // Flush any pending synthetic contacts from a gesture already in progress.
  if (dequeue(contact)) {
    return true;
  }

  // Otherwise read the encoder. This blocks up to ~50 ms only when idle.
  RotaryMsg_t msg = tpager::hw().getRotary();

  // Encoder direction is inverted on purpose: CW (ROTARY_DIR_UP) drives a swipe
  // down (menu next / reader WPM- / timer longer), CCW drives a swipe up.
  if (msg.dir == ROTARY_DIR_UP) {
    enqueueSwipe(1);
  } else if (msg.dir == ROTARY_DIR_DOWN) {
    enqueueSwipe(-1);
  }

  if (msg.centerBtnPressed) {
    tpager::hw().vibrator();  // Haptic confirmation on select / play-pause.
    enqueueTap();
  }

  dequeue(contact);  // Returns the first queued contact, or leaves it untouched.
  return true;
}

}  // namespace Board::Input
