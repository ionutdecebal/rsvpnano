#pragma once

#include <Arduino.h>
#include <esp_log.h>

#include "board/BoardPower.h"
#include "drivers/audio/es8311/Es8311.h"

namespace {

constexpr char kAudioTag[] = "audio";
constexpr uint32_t kAudioStartupDelayMs = 15;
constexpr uint32_t kBeepFrequencyHz = 1320;
constexpr int16_t kBeepAmplitude = 12000;
constexpr uint32_t kEnvelopeAttackMs = 6;
constexpr uint32_t kEnvelopeReleaseMs = 12;
constexpr uint32_t kSampleRateHz = 16000;
constexpr uint32_t kBeepDurationMs = 120;
constexpr uint32_t kWriteTimeoutMs = 250;
constexpr size_t kBeepFrames = (static_cast<size_t>(kSampleRateHz) * kBeepDurationMs) / 1000U;
constexpr size_t kBeepSamples = kBeepFrames * 2U;

int16_t gBeepBuffer[kBeepSamples] = {};

void fillBeepBuffer() {
  const size_t attackFrames = (static_cast<size_t>(kSampleRateHz) * kEnvelopeAttackMs) / 1000U;
  const size_t releaseFrames = (static_cast<size_t>(kSampleRateHz) * kEnvelopeReleaseMs) / 1000U;
  const uint32_t halfPeriodSamples = kSampleRateHz / (kBeepFrequencyHz * 2U);

  for (size_t frame = 0; frame < kBeepFrames; ++frame) {
    int32_t sample =
        ((frame / halfPeriodSamples) % 2U == 0U) ? kBeepAmplitude : -kBeepAmplitude;

    if (attackFrames > 0 && frame < attackFrames) {
      sample = (sample * static_cast<int32_t>(frame)) / static_cast<int32_t>(attackFrames);
    } else if (releaseFrames > 0 && frame >= (kBeepFrames - releaseFrames)) {
      const size_t remaining = kBeepFrames - frame;
      sample = (sample * static_cast<int32_t>(remaining)) / static_cast<int32_t>(releaseFrames);
    }

    const size_t index = frame * 2U;
    gBeepBuffer[index] = static_cast<int16_t>(sample);
    gBeepBuffer[index + 1U] = static_cast<int16_t>(sample);
  }
}

bool enableAudioRail() { return Board::Power::enableAudioPowerIfAvailable(); }

bool writeBeepBuffer(BoardDrivers::Es8311::Context &context) {
  return BoardDrivers::Es8311::writeSamples(context, gBeepBuffer, kBeepSamples, kWriteTimeoutMs);
}

}  // namespace

namespace BoardPlatform::Es8311BoardAudio {

bool begin(BoardDrivers::Es8311::Context &context) {
  if (BoardDrivers::Es8311::available(context)) {
    return true;
  }

  fillBeepBuffer();

  if (!enableAudioRail()) {
    ESP_LOGW(kAudioTag, "Audio rail unavailable");
    return false;
  }

  delay(kAudioStartupDelayMs);
  return BoardDrivers::Es8311::begin(context);
}

bool beep(BoardDrivers::Es8311::Context &context) {
  if (!enableAudioRail() || !BoardDrivers::Es8311::prepareOutput(context)) {
    return false;
  }

  if (writeBeepBuffer(context)) {
    return true;
  }

  ESP_LOGW(kAudioTag, "Retrying speaker beep after recovering output path");
  if (!enableAudioRail() || !BoardDrivers::Es8311::recoverOutputPath(context)) {
    return false;
  }

  return writeBeepBuffer(context);
}

bool available(const BoardDrivers::Es8311::Context &context) {
  return BoardDrivers::Es8311::available(context);
}

}  // namespace BoardPlatform::Es8311BoardAudio
