#pragma once

namespace WaveshareAmoled241::Version {

    constexpr const char* kBoardId = "waveshare_esp32s3_touch_amoled_2_41_v2";
    constexpr const char* kBoardLabel = "Waveshare ESP32-S3-Touch-AMOLED-2.41 V2";
    constexpr const char* kOtaAssetName = "rsvp-nano-esp32-s3-touch-amoled-2.41-v2-ota.bin";

    // GPIO21 is OLED_TE and GPIO3 is TP_INT on V2, not reset outputs.
    constexpr int kDisplayResetPin = -1;
    constexpr int kTouchResetPin = -1;
    constexpr int kTouchIrqPin = 3;
    constexpr int kTouchIrqExpanderPin = -1;
    constexpr int kDisplayRailEnablePin = -1;
    constexpr int kDisplayResetExpanderPin = 0;
    constexpr int kTouchResetExpanderPin = 1;

} // namespace WaveshareAmoled241::Version
