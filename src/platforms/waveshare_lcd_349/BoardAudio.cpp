#include "platforms/common/Es8311BoardAudio.h"

#include <Wire.h>

#include "board/BoardAudio.h"
#include "board/BoardConfig.h"
#include "drivers/audio/es8311/Es8311.h"

namespace {

BoardDrivers::Es8311::Context gAudioContext = {
    &Wire1,
    Board::Config::ES8311_ADDRESS,
    I2S_NUM_0,
    Board::Config::PIN_AUDIO_MCLK,
    Board::Config::PIN_AUDIO_BCLK,
    Board::Config::PIN_AUDIO_WS,
    Board::Config::PIN_AUDIO_DOUT,
};

}  // namespace

namespace Board::Audio {

bool begin() { return BoardPlatform::Es8311BoardAudio::begin(gAudioContext); }

bool beep() { return BoardPlatform::Es8311BoardAudio::beep(gAudioContext); }

bool available() { return BoardPlatform::Es8311BoardAudio::available(gAudioContext); }

}  // namespace Board::Audio
