#pragma once

namespace WaveshareAmoled241::Version {

    constexpr const char* kBoardId = "waveshare_esp32s3_touch_amoled_2_41";
    constexpr const char* kBoardLabel = "Waveshare ESP32-S3-Touch-AMOLED-2.41 V1";
    constexpr const char* kOtaAssetName = "rsvp-nano-esp32-s3-touch-amoled-2.41-ota.bin";

    constexpr int kDisplayResetPin = 21;
    constexpr int kTouchResetPin = 3;
    // TP_INT reaches EXIO2, but the V1 expander INT output is not connected to the MCU.
    constexpr int kTouchIrqPin = -1;
    constexpr int kTouchIrqExpanderPin = 2;
    constexpr int kDisplayRailEnablePin = 1;
    constexpr int kDisplayResetExpanderPin = -1;
    constexpr int kTouchResetExpanderPin = -1;

} // namespace WaveshareAmoled241::Version
