#pragma once

#include <Arduino.h>
#include "Types.h"

class ReadingEngine {
 public:
  void setBook(const BookData* book);
  void setWpm(int wpm);

  bool tick(uint32_t nowMs, size_t& inOutIndex);
  int wpm() const { return wpm_; }

 private:
  uint32_t wordDurationMs_(const WordToken& t) const;

  const BookData* book_ = nullptr;
  int wpm_ = 280;
  uint32_t nextWordMs_ = 0;
};
