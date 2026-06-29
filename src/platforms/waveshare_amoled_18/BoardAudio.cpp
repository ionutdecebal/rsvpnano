#include "platforms/common/Es8311BoardAudio.h"

#include <Wire.h>

#include "board/BoardAudio.h"
#include "drivers/audio/es8311/Es8311.h"
#include "platforms/waveshare_amoled_18/WaveshareAmoled18.h"

namespace {

BoardDrivers::Es8311::Context gAudioContext = {
    &Wire,
    WaveshareAmoled18::AudioWiring::kEs8311Address,
    I2S_NUM_0,
    WaveshareAmoled18::AudioWiring::kMclkPin,
    WaveshareAmoled18::AudioWiring::kBclkPin,
    WaveshareAmoled18::AudioWiring::kWsPin,
    WaveshareAmoled18::AudioWiring::kDoutPin,
};

}  // namespace

namespace Board::Audio {

bool begin() { return BoardPlatform::Es8311BoardAudio::begin(gAudioContext); }

bool beep() { return BoardPlatform::Es8311BoardAudio::beep(gAudioContext); }

bool available() { return BoardPlatform::Es8311BoardAudio::available(gAudioContext); }

}  // namespace Board::Audio
