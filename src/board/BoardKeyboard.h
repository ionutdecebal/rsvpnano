#pragma once

namespace Board::Keyboard {

// True if this board has a physical keyboard. When true, the on-screen virtual
// keyboard is suppressed and text entry is driven by readChar().
bool present();

// Polls the physical keyboard. Returns true and sets `c` to the resolved
// character for a key pressed this poll: a printable character, ' ' (space),
// '\n' (enter/confirm) or '\b' (backspace). Returns false when nothing was
// pressed. Boards without a physical keyboard always return false.
bool readChar(char &c);

}  // namespace Board::Keyboard
