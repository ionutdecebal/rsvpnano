#include "platforms/common/Es8311BoardAudio.h"

#include <Wire.h>

#include "board/BoardAudio.h"
#include "drivers/audio/es8311/Es8311.h"
#include "platforms/waveshare_amoled_216/WaveshareAmoled216.h"

namespace {

BoardDrivers::Es8311::Context gAudioContext = {
    &Wire,
    WaveshareAmoled216::AudioWiring::kEs8311Address,
    I2S_NUM_0,
    WaveshareAmoled216::AudioWiring::kMclkPin,
    WaveshareAmoled216::AudioWiring::kBclkPin,
    WaveshareAmoled216::AudioWiring::kWsPin,
    WaveshareAmoled216::AudioWiring::kDoutPin,
};

}  // namespace

namespace Board::Audio {

bool begin() { return BoardPlatform::Es8311BoardAudio::begin(gAudioContext); }

bool beep() { return BoardPlatform::Es8311BoardAudio::beep(gAudioContext); }

bool available() { return BoardPlatform::Es8311BoardAudio::available(gAudioContext); }

}  // namespace Board::Audio
