#include "board/BoardKeyboard.h"

// Default backend for boards without a physical keyboard: text entry uses the
// on-screen virtual keyboard. The T-LoRa-Pager replaces this file with its own
// TCA8418-backed implementation (see platforms/lilygo_tlora_pager/BoardKeyboard.cpp).

namespace Board::Keyboard {

bool present() { return false; }

bool readChar(char &c) {
  (void)c;
  return false;
}

}  // namespace Board::Keyboard
