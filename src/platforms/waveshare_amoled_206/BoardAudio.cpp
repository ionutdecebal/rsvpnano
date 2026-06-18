#include "platforms/common/Es8311BoardAudio.h"

#include <Wire.h>

#include "board/BoardAudio.h"
#include "drivers/audio/es8311/Es8311.h"
#include "platforms/waveshare_amoled_206/WaveshareAmoled206.h"

namespace {

BoardDrivers::Es8311::Context gAudioContext = {
    &Wire,
    WaveshareAmoled206::AudioWiring::kEs8311Address,
    I2S_NUM_0,
    WaveshareAmoled206::AudioWiring::kMclkPin,
    WaveshareAmoled206::AudioWiring::kBclkPin,
    WaveshareAmoled206::AudioWiring::kWsPin,
    WaveshareAmoled206::AudioWiring::kDoutPin,
};

}  // namespace

namespace Board::Audio {

bool begin() { return BoardPlatform::Es8311BoardAudio::begin(gAudioContext); }

bool beep() { return BoardPlatform::Es8311BoardAudio::beep(gAudioContext); }

bool available() { return BoardPlatform::Es8311BoardAudio::available(gAudioContext); }

}  // namespace Board::Audio
