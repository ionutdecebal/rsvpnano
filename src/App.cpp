#include "App.h"

#include <esp_sleep.h>

bool App::begin() {
  Serial.begin(115200);
  if (!board_.begin()) {
    return false;
  }

  view_.begin(board_.gfx());
  view_.drawLoading("Loading...");

  storage_.begin();
  loadLibrary();
  button_.begin();
  touch_.reset();

  if (books_.empty()) {
    view_.drawLoading("No books in /books");
    return true;
  }

  openBookById(books_[selectedBookIdx_].id);
  state_ = AppState::PAUSED;
  pauseStartedMs_ = board_.nowMs();
  return true;
}

void App::loadLibrary() {
  books_ = storage_.listBooks();
  String selectedId;
  storage_.loadSettings(selectedId, wpm_);

  selectedBookIdx_ = 0;
  for (size_t i = 0; i < books_.size(); ++i) {
    if (books_[i].id == selectedId) {
      selectedBookIdx_ = i;
      break;
    }
  }
}

void App::openBookById(const String& id) {
  for (size_t i = 0; i < books_.size(); ++i) {
    if (books_[i].id != id) continue;

    selectedBookIdx_ = i;
    String content;
    if (!storage_.readBook(books_[i], content)) return;

    currentBook_ = parser_.parse(books_[i].id, books_[i].title, content);
    BookProgress p;
    if (storage_.loadProgress(currentBook_.id, p)) {
      wordIndex_ = min(p.wordIndex, currentBook_.words.size() > 0 ? currentBook_.words.size() - 1 : 0);
      wpm_ = p.wpm;
    } else {
      wordIndex_ = 0;
    }

    engine_.setBook(&currentBook_);
    engine_.setWpm(wpm_);
    storage_.saveSettings(currentBook_.id, wpm_);
    return;
  }
}

void App::changeBook(int delta) {
  if (books_.empty()) return;
  int next = selectedBookIdx_ + delta;
  if (next < 0) next = books_.size() - 1;
  if (next >= (int)books_.size()) next = 0;
  openBookById(books_[next].id);
}

void App::jumpChapter(int chapterIdx) {
  if (chapterIdx < 0 || chapterIdx >= (int)currentBook_.chapters.size()) return;
  wordIndex_ = currentBook_.chapters[chapterIdx].wordIndex;
  saveProgressNow();
}

void App::saveProgressNow() {
  if (currentBook_.id.isEmpty()) return;
  BookProgress p;
  p.wordIndex = wordIndex_;
  p.wpm = wpm_;
  storage_.saveProgress(currentBook_.id, p);
  storage_.saveSettings(currentBook_.id, wpm_);
  lastProgressSaveMs_ = board_.nowMs();
}

void App::enterSleep() {
  saveProgressNow();
  board_.powerDownDisplay();
  state_ = AppState::SLEEPING;

  esp_sleep_enable_ext0_wakeup((gpio_num_t)BoardConfig::PIN_BOOT_BUTTON, 0);
  delay(50);
  esp_deep_sleep_start();
}

void App::handlePausedGestures(const GestureResult& g) {
  if (g.verticalWpm) {
    wpm_ = constrain(wpm_ + g.wpmDelta, 120, 900);
    engine_.setWpm(wpm_);
    view_.drawOverlayWpm(wpm_);
    saveProgressNow();
  }

  if (g.horizontalScrub && !currentBook_.words.empty()) {
    int delta = g.scrubDelta;
    size_t idx = wordIndex_;

    int stepAbs = abs(delta);
    bool big = stepAbs > 18;
    bool medium = stepAbs > 8;

    if (delta > 0) {
      if (big) {
        for (size_t i = idx; i + 1 < currentBook_.words.size(); ++i) {
          if (currentBook_.words[i].paragraphBreak) {
            idx = i;
            break;
          }
        }
      } else if (medium) {
        for (size_t i = idx; i + 1 < currentBook_.words.size(); ++i) {
          if (currentBook_.words[i].sentenceEnd) {
            idx = min(i + 1, currentBook_.words.size() - 1);
            break;
          }
        }
      } else {
        idx = min(idx + (size_t)stepAbs, currentBook_.words.size() - 1);
      }
    } else {
      if (big) {
        for (int i = (int)idx - 1; i >= 0; --i) {
          if (currentBook_.words[i].paragraphBreak) {
            idx = i;
            break;
          }
        }
      } else if (medium) {
        for (int i = (int)idx - 1; i >= 0; --i) {
          if (currentBook_.words[i].sentenceEnd) {
            idx = min((size_t)i + 1, currentBook_.words.size() - 1);
            break;
          }
        }
      } else {
        idx = (idx > (size_t)stepAbs) ? idx - stepAbs : 0;
      }
    }

    wordIndex_ = idx;
  }

  if (g.longPress) {
    state_ = AppState::MENU;
    menuIndex_ = 0;
  }
}

void App::handleMenuGestures(const GestureResult& g) {
  if (g.verticalWpm) {
    menuIndex_ = constrain(menuIndex_ + (g.wpmDelta > 0 ? -1 : 1), 0, 4);
  }

  if (g.horizontalScrub) {
    if (g.scrubDelta > 0) {
      switch (menuIndex_) {
        case 0:
          state_ = AppState::PAUSED;
          break;
        case 1:
          chapterMenu_ = true;
          chapterIndex_ = 0;
          break;
        case 2:
          wordIndex_ = 0;
          saveProgressNow();
          state_ = AppState::PAUSED;
          break;
        case 3:
          changeBook(1);
          state_ = AppState::PAUSED;
          break;
        case 4:
          enterSleep();
          break;
      }
    } else {
      state_ = AppState::PAUSED;
    }
  }
}

void App::handleChapterGestures(const GestureResult& g) {
  if (g.verticalWpm) {
    chapterIndex_ = constrain(chapterIndex_ + (g.wpmDelta > 0 ? -1 : 1), 0, (int)currentBook_.chapters.size() - 1);
  }

  if (g.horizontalScrub) {
    if (g.scrubDelta > 0) {
      jumpChapter(chapterIndex_);
      chapterMenu_ = false;
      state_ = AppState::PAUSED;
    } else {
      chapterMenu_ = false;
      state_ = AppState::MENU;
    }
  }
}

void App::loop() {
  board_.update();
  uint32_t now = board_.nowMs();

  bool pausedOrMenu = (state_ == AppState::PAUSED || state_ == AppState::MENU);
  ButtonEvents be = button_.update(board_.bootPressed(), pausedOrMenu, now);

  if (state_ == AppState::PAUSED || state_ == AppState::PLAYING) {
    if (be.holdActive) {
      state_ = AppState::PLAYING;
    } else {
      if (state_ != AppState::PAUSED) {
        state_ = AppState::PAUSED;
        pauseStartedMs_ = now;
        saveProgressNow();
      }
    }
  }

  if (state_ == AppState::PAUSED && be.triplePress) {
    enterSleep();
  }

  TouchEvent te = board_.readTouch();
  bool touchEnabled = (state_ == AppState::PAUSED || state_ == AppState::MENU);
  GestureResult g = touch_.update(te, touchEnabled, now);

  if (state_ == AppState::PLAYING) {
    size_t idxRef = wordIndex_;
    engine_.tick(now, idxRef);
    wordIndex_ = idxRef;
    if (now - lastProgressSaveMs_ > 8000) {
      saveProgressNow();
    }
  } else if (state_ == AppState::PAUSED) {
    handlePausedGestures(g);
  } else if (state_ == AppState::MENU) {
    if (chapterMenu_) {
      handleChapterGestures(g);
    } else {
      handleMenuGestures(g);
    }
  }

  if (currentBook_.words.empty()) {
    view_.drawLoading("No parsed words");
    delay(16);
    return;
  }

  float pauseAnim = 1.0f;
  if (state_ == AppState::PAUSED) {
    pauseAnim = min(1.0f, (float)(now - pauseStartedMs_) / (float)BoardConfig::PAUSE_TRANSITION_MS);
  }

  view_.drawWord(currentBook_.words[wordIndex_].text, state_, pauseAnim);
  view_.drawStatus(currentBook_.title, wpm_, wordIndex_, currentBook_.words.size());

  if (state_ == AppState::MENU) {
    if (chapterMenu_) {
      view_.drawChapterList(currentBook_.chapters, chapterIndex_);
    } else {
      std::vector<String> items = {"Resume", "Chapters", "Restart book", "Change book", "Sleep"};
      view_.drawMenu(items, menuIndex_);
    }
  }

  delay(10);
}
