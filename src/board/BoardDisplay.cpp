#include "board/BoardDisplay.h"

namespace Board::Display {

uint16_t nativeWidth() { return Board::Config::PANEL_NATIVE_WIDTH; }

uint16_t nativeHeight() { return Board::Config::PANEL_NATIVE_HEIGHT; }

size_t txChunkBytes() { return Board::Config::DISPLAY_TX_CHUNK_BYTES; }

}  // namespace Board::Display
