#pragma once

#include "ui/screens/ReaderLayout.h"

namespace screens::modern {
    enum class ReaderControl : uint8_t { None, Chapter, Footer, Focus, Controls };
    constexpr ui::Rect readerControlRect(ReaderControl control) {
        switch(control){
        case ReaderControl::Chapter:return {12,0,290,36};
        case ReaderControl::Focus:return {310,0,194,36};
        case ReaderControl::Footer:return {510,136,122,34};
        case ReaderControl::Controls:return {270,136,100,34};
        default:return {};
        }
    }
    ReaderControl readerControlAt(uint16_t x, uint16_t y, const settings::ReadingSettings& config,
                                  bool reading, bool canRead = true, bool focusActive = false);
    void readerChrome(ui::Context& ui, const readerLayout::Chrome& view,
                      const settings::ReadingSettings& config, const Board::Power::BatteryState& battery);
}
