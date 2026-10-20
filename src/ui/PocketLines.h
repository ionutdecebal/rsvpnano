#pragma once
#include "ui/Ui.h"
namespace ui::pocketlines {
void mark(Context& ui, Arduino_GFX& gfx, int16_t x, int16_t y, int size, bool flipped,
          themes::ColorRole role, uint8_t opacity=255);
void guides(Context& ui, Arduino_GFX& gfx, Rect bounds, int16_t anchor, int16_t textTop,
            int16_t textBottom, int size, int gap, themes::ColorRole role, uint8_t opacity=255);
}
