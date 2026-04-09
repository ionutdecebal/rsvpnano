#include "ReadingEngine.h"

void ReadingEngine::setBook(const BookData* book) {
  book_ = book;
  nextWordMs_ = 0;
}

void ReadingEngine::setWpm(int wpm) {
  wpm_ = constrain(wpm, 120, 900);
}

bool ReadingEngine::tick(uint32_t nowMs, size_t& inOutIndex) {
  if (!book_ || book_->words.empty()) return false;
  if (inOutIndex >= book_->words.size()) inOutIndex = book_->words.size() - 1;

  if (nextWordMs_ == 0) {
    nextWordMs_ = nowMs + wordDurationMs_(book_->words[inOutIndex]);
    return false;
  }

  if (nowMs < nextWordMs_) return false;

  if (inOutIndex + 1 < book_->words.size()) {
    inOutIndex++;
  }
  nextWordMs_ = nowMs + wordDurationMs_(book_->words[inOutIndex]);
  return true;
}

uint32_t ReadingEngine::wordDurationMs_(const WordToken& t) const {
  float base = 60000.0f / (float)wpm_;
  float mult = 1.0f;

  if (t.text.endsWith(",") || t.text.endsWith(":")) mult *= 1.25f;
  if (t.text.endsWith(".") || t.text.endsWith("!") || t.text.endsWith("?")) mult *= 1.55f;
  if (t.text.endsWith(";") ) mult *= 1.35f;
  if (t.text.length() >= 9) mult *= 1.2f;
  if (t.text.length() >= 13) mult *= 1.3f;
  if (t.paragraphBreak) mult *= 1.3f;

  return (uint32_t)(base * mult);
}
