#include "board/BoardKeyboard.h"

#include "platforms/lilygo_tlora_pager/TPagerHardware.h"

// LilyGo T-LoRa-Pager physical keyboard backend.
//
// The board carries a TCA8418 QWERTY keyboard driven by LilyGoLib's
// LilyGoKeyboard. getKey() resolves the keymap, the Shift/caps key and the
// Space-as-Fn symbol layer internally, returning the final character: a letter
// (upper-case when Shift is latched), a number/symbol (when Space-Fn is held),
// ' ', '\n' (Enter) or '\b' (backspace). Modifier-only keys yield '\0'. We
// surface that here so App's text entry uses the physical keys instead of the
// on-screen keyboard.

namespace Board::Keyboard {

bool present() { return true; }

bool readChar(char &c) {
  char key = '\0';
  // LilyGoKeyboard::getKey returns KB_PRESSED (1) on a key press; act on those
  // only, and ignore modifier/no-op keys that resolve to the null character.
  if (tpager::hw().kb.getKey(&key) != 1 || key == '\0') {
    return false;
  }
  c = key;
  return true;
}

}  // namespace Board::Keyboard
