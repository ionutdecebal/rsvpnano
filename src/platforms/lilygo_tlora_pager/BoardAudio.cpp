#include "board/BoardAudio.h"

#include <Arduino.h>
#include <math.h>

#include "platforms/lilygo_tlora_pager/TPagerHardware.h"

// LilyGo T-LoRa-Pager audio cues via the ES8311 codec (driven by LilyGoLib).
// Implements the Board::Audio surface used by App (begin/beep/available).

namespace {

constexpr uint32_t kSampleRateHz = 16000;
constexpr uint32_t kBeepDurationMs = 120;
constexpr size_t kBeepFrames = (static_cast<size_t>(kSampleRateHz) * kBeepDurationMs) / 1000U;

bool gAvailable = false;
int16_t gBeepBuffer[kBeepFrames] = {};

}  // namespace

namespace Board::Audio {

bool begin() {
  tpager::ensureBegun();
  // Enable the Class-D speaker amplifier rail (XL9555 GPIO1). The codec itself
  // is brought up by LilyGoLib::begin().
  tpager::hw().powerControl(POWER_SPEAK, true);
  gAvailable = true;
  return true;
}

bool available() { return gAvailable; }

bool beep() {
  if (!gAvailable) {
    return false;
  }

  // Synthesize a short 1 kHz mono tone with a quick attack/decay envelope so it
  // does not click.
  constexpr float kToneHz = 1000.0f;
  constexpr float kAmplitude = 8000.0f;
  const float fadeFrames = static_cast<float>(kSampleRateHz) * 0.008f;  // 8 ms ramps
  for (size_t frame = 0; frame < kBeepFrames; ++frame) {
    const float t = static_cast<float>(frame) / static_cast<float>(kSampleRateHz);
    float envelope = 1.0f;
    if (frame < fadeFrames) {
      envelope = static_cast<float>(frame) / fadeFrames;
    } else if (frame > kBeepFrames - fadeFrames) {
      envelope = static_cast<float>(kBeepFrames - frame) / fadeFrames;
    }
    const float value = sinf(2.0f * static_cast<float>(PI) * kToneHz * t) * kAmplitude * envelope;
    gBeepBuffer[frame] = static_cast<int16_t>(value);
  }

  EspCodec &codec = tpager::hw().codec;
  codec.open(16, 1, kSampleRateHz);  // 16-bit, mono
  codec.setVolume(80);
  codec.write(reinterpret_cast<uint8_t *>(gBeepBuffer), kBeepFrames * sizeof(int16_t));
  return true;
}

}  // namespace Board::Audio
