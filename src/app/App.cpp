#include "app/App.h"

#include <esp_log.h>
#include <algorithm>
#include <cstdio>
#include <utility>
#include <vector>

#include "board/BoardConfig.h"

static const char *kAppTag = "app";
constexpr uint32_t kBootSplashMs = 750;
constexpr uint32_t kReleaseBufferMs = 200;
constexpr uint32_t kWpmFeedbackMs = 900;
constexpr uint32_t kLongPressMs = 700;
constexpr uint16_t kSwipeThresholdPx = 40;
constexpr uint16_t kAxisBiasPx = 12;
constexpr uint16_t kTapSlopPx = 18;
constexpr uint16_t kScrubFineStepPx = 28;
constexpr uint8_t kScrubFineTicks = 4;
constexpr uint8_t kScrubFastMultiplier = 3;
constexpr int kMaxScrubStepsPerGesture = 40;
constexpr uint32_t kProgressSaveIntervalMs = 15000;
constexpr uint32_t kUsbTransferExitHoldMs = 1200;

namespace {

enum MenuItem : size_t {
  MenuResume,
  MenuChapters,
  MenuRestart,
  MenuChangeBook,
  MenuUsbTransfer,
  MenuSleep,
  MenuItemCount,
};

constexpr const char *kMenuItems[] = {
    "Resume",
    "Chapters",
    "Restart",
    "Change book",
    "USB transfer",
    "Sleep",
};

constexpr size_t kBookPickerBackIndex = 0;
constexpr size_t kChapterPickerBackIndex = 0;
constexpr size_t kChapterPickerFallbackIndex = 1;
constexpr const char *kPrefsNamespace = "rsvp";
constexpr const char *kPrefBookPath = "book";
constexpr const char *kPrefLegacyWordIndex = "word";
constexpr const char *kPrefWpm = "wpm";
constexpr const char *kPrefRecentSeq = "seq";
constexpr uint32_t kNoSavedWordIndex = 0xFFFFFFFFUL;

void logApp(const char *message) {
  ESP_LOGI(kAppTag, "%s", message);
  Serial.printf("[app] %s\n", message);
}

String displayNameForPath(const String &path) {
  const int separator = path.lastIndexOf('/');
  String name = separator >= 0 ? path.substring(separator + 1) : path;
  String lowered = name;
  lowered.toLowerCase();
  if (lowered.endsWith(".txt")) {
    name.remove(name.length() - 4);
  }
  if (lowered.endsWith(".rsvp")) {
    name.remove(name.length() - 5);
  }
  return name;
}

uint32_t hashBookPath(const String &path) {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < path.length(); ++i) {
    hash ^= static_cast<uint8_t>(path[i]);
    hash *= 16777619UL;
  }
  return hash;
}

}  // namespace

App::App() : button_(BoardConfig::PIN_BOOT_BUTTON) {}

void App::begin() {
  BoardConfig::begin();
  button_.begin();
  preferences_.begin(kPrefsNamespace, false);
  bootStartedMs_ = millis();
  lastStateLogMs_ = bootStartedMs_;

  logApp("Initializing hardware modules");
  const bool displayReady = display_.begin();

  if (displayReady) {
    display_.renderCenteredWord("READY");
    logApp("Display init ok");
  } else {
    ESP_LOGE(kAppTag, "Display init failed");
    Serial.println("[app] Display init failed");
  }

  touchInitialized_ = touch_.begin();
  const bool storageReady = storage_.begin();
  storage_.listBooks();
  const uint16_t savedWpm = preferences_.getUShort(kPrefWpm, reader_.wpm());
  reader_.setWpm(savedWpm);

  if (storageReady && restoreSavedBook(bootStartedMs_)) {
    usingStorageBook_ = true;
  } else if (storageReady && loadBookAtIndex(0, bootStartedMs_)) {
    usingStorageBook_ = true;
  } else {
    usingStorageBook_ = false;
    chapterMarkers_.clear();
    currentBookPath_ = "";
    currentBookTitle_ = "Demo";
    reader_.begin(bootStartedMs_);
    Serial.println("[app] using built-in demo text");
  }

  Serial.printf("[app] WPM=%u interval=%lu ms\n", reader_.wpm(),
                static_cast<unsigned long>(reader_.wordIntervalMs()));

  state_ = AppState::Booting;
  Serial.println("[app] READY splash active");
}

void App::update(uint32_t nowMs) {
  button_.update(nowMs);
  updateState(nowMs);
  updateReader(nowMs);
  handleTouch(nowMs);
  updateWpmFeedback(nowMs);
  maybeSaveReadingPosition(nowMs);

  if (nowMs - lastStateLogMs_ > 1500) {
    lastStateLogMs_ = nowMs;
    ESP_LOGI(kAppTag, "state=%s", stateName(state_));
    Serial.printf("[app] state=%s ms=%lu\n", stateName(state_),
                  static_cast<unsigned long>(nowMs));
  }
}

const char *App::stateName(AppState state) const {
  switch (state) {
    case AppState::Booting:
      return "Booting";
    case AppState::Paused:
      return "Paused";
    case AppState::Playing:
      return "Playing";
    case AppState::Menu:
      return "Menu";
    case AppState::UsbTransfer:
      return "UsbTransfer";
    case AppState::Sleeping:
      return "Sleeping";
  }
  return "Unknown";
}

const char *App::touchPhaseName(TouchPhase phase) const {
  switch (phase) {
    case TouchPhase::Start:
      return "Start";
    case TouchPhase::Move:
      return "Move";
    case TouchPhase::End:
      return "End";
  }
  return "Unknown";
}

void App::setState(AppState nextState, uint32_t nowMs) {
  if (nextState == state_) {
    return;
  }

  const AppState previousState = state_;

  if (nextState != AppState::Paused) {
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
    wpmFeedbackVisible_ = false;
  }

  state_ = nextState;

  switch (state_) {
    case AppState::Paused:
      renderReaderWord();
      break;
    case AppState::Playing:
      reader_.start(nowMs);
      renderReaderWord();
      break;
    case AppState::Menu:
      renderMenu();
      break;
    case AppState::UsbTransfer:
      display_.renderStatus("USB", "Preparing SD", "Eject when done");
      break;
    case AppState::Sleeping:
      display_.renderCenteredWord("SLEEP");
      break;
    case AppState::Booting:
      display_.renderCenteredWord("READY");
      break;
  }

  if (state_ == AppState::Paused && previousState == AppState::Playing) {
    saveReadingPosition(true);
  }

  ESP_LOGI(kAppTag, "state -> %s", stateName(state_));
  Serial.printf("[app] state -> %s at %lu ms\n", stateName(state_),
                static_cast<unsigned long>(nowMs));
}

void App::updateState(uint32_t nowMs) {
  if (state_ == AppState::Booting) {
    if (nowMs - bootStartedMs_ < kBootSplashMs) {
      return;
    }

    setState(button_.isHeld() ? AppState::Playing : AppState::Paused, nowMs);
    return;
  }

  if (state_ == AppState::UsbTransfer) {
    updateUsbTransfer(nowMs);
    return;
  }

  if (state_ == AppState::Menu || state_ == AppState::Sleeping) {
    // TODO: Add richer Menu and Sleeping transition rules once those product milestones start.
    return;
  }

  if (button_.isHeld()) {
    setState(AppState::Playing, nowMs);
    return;
  }

  if (state_ == AppState::Playing && nowMs - button_.lastEdgeMs() < kReleaseBufferMs) {
    return;
  }

  setState(AppState::Paused, nowMs);
}

void App::updateReader(uint32_t nowMs) {
  if (state_ != AppState::Playing) {
    return;
  }

  if (!reader_.update(nowMs)) {
    return;
  }

  renderReaderWord();
}

void App::maybeSaveReadingPosition(uint32_t nowMs) {
  if (!usingStorageBook_ || currentBookPath_.isEmpty() || state_ != AppState::Playing) {
    return;
  }

  if (nowMs - lastProgressSaveMs_ < kProgressSaveIntervalMs) {
    return;
  }

  lastProgressSaveMs_ = nowMs;
  saveReadingPosition(false);
}

void App::updateWpmFeedback(uint32_t nowMs) {
  if (!wpmFeedbackVisible_ || state_ != AppState::Paused) {
    return;
  }

  if (nowMs < wpmFeedbackUntilMs_) {
    return;
  }

  wpmFeedbackVisible_ = false;
  renderReaderWord();
}

void App::handleTouch(uint32_t nowMs) {
  if (!touchInitialized_) {
    return;
  }

  if (state_ == AppState::Playing || state_ == AppState::Booting ||
      state_ == AppState::UsbTransfer || state_ == AppState::Sleeping) {
    // Touch is intentionally disabled during playback so it cannot interfere with timing.
    touch_.cancel();
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
    return;
  }

  TouchEvent ev;
  if (!touch_.poll(ev)) {
    return;
  }

  Serial.printf("[touch] phase=%s touched=%u x=%u y=%u gesture=%u state=%s\n",
                touchPhaseName(ev.phase), ev.touched ? 1 : 0, ev.x, ev.y, ev.gesture,
                stateName(state_));
  if (state_ == AppState::Menu) {
    applyMenuTouchGesture(ev, nowMs);
  } else {
    applyPausedTouchGesture(ev, nowMs);
  }
}

void App::applyPausedTouchGesture(const TouchEvent &event, uint32_t nowMs) {
  if (event.phase == TouchPhase::Start) {
    pausedTouch_.active = true;
    pausedTouchIntent_ = TouchIntent::None;
    pausedTouch_.startX = event.x;
    pausedTouch_.startY = event.y;
    pausedTouch_.lastX = event.x;
    pausedTouch_.lastY = event.y;
    pausedTouch_.startMs = nowMs;
    pausedTouch_.lastMs = nowMs;
    pausedTouch_.startWordIndex = reader_.currentIndex();
    pausedTouch_.scrubStepsApplied = 0;
    return;
  }

  if (!pausedTouch_.active) {
    return;
  }

  pausedTouch_.lastX = event.x;
  pausedTouch_.lastY = event.y;
  pausedTouch_.lastMs = nowMs;

  const int deltaX = static_cast<int>(pausedTouch_.lastX) - static_cast<int>(pausedTouch_.startX);
  const int deltaY = static_cast<int>(pausedTouch_.lastY) - static_cast<int>(pausedTouch_.startY);
  const int absDeltaX = abs(deltaX);
  const int absDeltaY = abs(deltaY);
  const uint32_t pressDurationMs = nowMs - pausedTouch_.startMs;
  const bool ended = event.phase == TouchPhase::End;

  if (ended && pressDurationMs >= kLongPressMs && absDeltaX <= static_cast<int>(kTapSlopPx) &&
      absDeltaY <= static_cast<int>(kTapSlopPx)) {
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
    menuScreen_ = MenuScreen::Main;
    menuSelectedIndex_ = MenuResume;
    wpmFeedbackVisible_ = false;
    setState(AppState::Menu, nowMs);
    return;
  }

  if (pausedTouchIntent_ == TouchIntent::None) {
    if (absDeltaX >= static_cast<int>(kSwipeThresholdPx) &&
        absDeltaX > absDeltaY + static_cast<int>(kAxisBiasPx)) {
      pausedTouchIntent_ = TouchIntent::Scrub;
    } else if (absDeltaY >= static_cast<int>(kSwipeThresholdPx) &&
               absDeltaY > absDeltaX + static_cast<int>(kAxisBiasPx)) {
      pausedTouchIntent_ = TouchIntent::Wpm;
    }
  }

  if (pausedTouchIntent_ == TouchIntent::Scrub) {
    applyScrubTarget(scrubStepsForDrag(deltaX));
    if (ended) {
      pausedTouch_.active = false;
      pausedTouchIntent_ = TouchIntent::None;
      saveReadingPosition(true);
    }
    return;
  }

  if (pausedTouchIntent_ == TouchIntent::Wpm) {
    if (!ended) {
      return;
    }

    const int wpmDelta = (deltaY < 0) ? 1 : -1;
    reader_.adjustWpm(wpmDelta);
    preferences_.putUShort(kPrefWpm, reader_.wpm());
    renderWpmFeedback(nowMs);
    Serial.printf("[app] WPM=%u interval=%lu ms\n", reader_.wpm(),
                  static_cast<unsigned long>(reader_.wordIntervalMs()));
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
    return;
  }

  if (ended) {
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
  }

  // TODO: Add paused-state tap behavior only if the product needs it.
}

int App::scrubStepsForDrag(int deltaX) const {
  const int absDeltaX = abs(deltaX);
  if (absDeltaX < static_cast<int>(kSwipeThresholdPx)) {
    return 0;
  }

  const int rawTicks = 1 + ((absDeltaX - static_cast<int>(kSwipeThresholdPx)) /
                            static_cast<int>(kScrubFineStepPx));
  int steps = rawTicks;
  if (rawTicks > static_cast<int>(kScrubFineTicks)) {
    steps = static_cast<int>(kScrubFineTicks) +
            (rawTicks - static_cast<int>(kScrubFineTicks)) *
                static_cast<int>(kScrubFastMultiplier);
  }
  steps = std::min(steps, kMaxScrubStepsPerGesture);

  return (deltaX > 0) ? -steps : steps;
}

void App::applyScrubTarget(int targetSteps) {
  if (targetSteps == pausedTouch_.scrubStepsApplied) {
    return;
  }

  reader_.seekRelative(pausedTouch_.startWordIndex, targetSteps);
  pausedTouch_.scrubStepsApplied = targetSteps;
  wpmFeedbackVisible_ = false;
  renderReaderWord();
  Serial.printf("[app] scrub target=%d word=%s\n", targetSteps, reader_.currentWord().c_str());
}

void App::applyMenuTouchGesture(const TouchEvent &event, uint32_t nowMs) {
  if (event.phase == TouchPhase::Start) {
    pausedTouch_.active = true;
    pausedTouchIntent_ = TouchIntent::None;
    pausedTouch_.startX = event.x;
    pausedTouch_.startY = event.y;
    pausedTouch_.lastX = event.x;
    pausedTouch_.lastY = event.y;
    pausedTouch_.startMs = nowMs;
    pausedTouch_.lastMs = nowMs;
    return;
  }

  if (!pausedTouch_.active) {
    return;
  }

  pausedTouch_.lastX = event.x;
  pausedTouch_.lastY = event.y;
  pausedTouch_.lastMs = nowMs;

  if (event.phase != TouchPhase::End) {
    return;
  }

  pausedTouch_.active = false;

  const int deltaX = static_cast<int>(pausedTouch_.lastX) - static_cast<int>(pausedTouch_.startX);
  const int deltaY = static_cast<int>(pausedTouch_.lastY) - static_cast<int>(pausedTouch_.startY);
  const int absDeltaX = abs(deltaX);
  const int absDeltaY = abs(deltaY);

  if (absDeltaY >= static_cast<int>(kSwipeThresholdPx) &&
      absDeltaY > absDeltaX + static_cast<int>(kAxisBiasPx)) {
    moveMenuSelection(deltaY < 0 ? -1 : 1);
    return;
  }

  if (absDeltaX <= static_cast<int>(kTapSlopPx) && absDeltaY <= static_cast<int>(kTapSlopPx)) {
    selectMenuItem(nowMs);
  }
}

void App::moveMenuSelection(int direction) {
  if (direction == 0) {
    return;
  }

  size_t &selectedIndex =
      (menuScreen_ == MenuScreen::BookPicker)
          ? bookPickerSelectedIndex_
          : (menuScreen_ == MenuScreen::ChapterPicker ? chapterPickerSelectedIndex_
                                                       : menuSelectedIndex_);
  const size_t itemCount =
      (menuScreen_ == MenuScreen::BookPicker)
          ? bookMenuItems_.size()
          : (menuScreen_ == MenuScreen::ChapterPicker ? chapterMenuItems_.size() : MenuItemCount);
  if (itemCount == 0) {
    return;
  }

  const int next = static_cast<int>(selectedIndex) + direction;
  if (next < 0) {
    selectedIndex = itemCount - 1;
  } else if (next >= static_cast<int>(itemCount)) {
    selectedIndex = 0;
  } else {
    selectedIndex = static_cast<size_t>(next);
  }

  renderMenu();
  if (menuScreen_ == MenuScreen::BookPicker) {
    Serial.printf("[book-picker] selected=%s\n", bookMenuItems_[bookPickerSelectedIndex_].c_str());
  } else if (menuScreen_ == MenuScreen::ChapterPicker) {
    Serial.printf("[chapter-picker] selected=%s\n",
                  chapterMenuItems_[chapterPickerSelectedIndex_].c_str());
  } else {
    Serial.printf("[menu] selected=%s\n", kMenuItems[menuSelectedIndex_]);
  }
}

void App::selectMenuItem(uint32_t nowMs) {
  if (menuScreen_ == MenuScreen::BookPicker) {
    selectBookPickerItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::ChapterPicker) {
    selectChapterPickerItem(nowMs);
    return;
  }

  switch (menuSelectedIndex_) {
    case MenuResume:
      setState(AppState::Paused, nowMs);
      return;
    case MenuRestart:
      reader_.begin(nowMs);
      setState(AppState::Paused, nowMs);
      saveReadingPosition(true);
      return;
    case MenuSleep:
      enterSleep(nowMs);
      return;
    case MenuUsbTransfer:
      enterUsbTransfer(nowMs);
      return;
    case MenuChapters:
      openChapterPicker();
      return;
    case MenuChangeBook:
      openBookPicker();
      return;
    default:
      return;
  }
}

void App::openBookPicker() {
  storage_.refreshBooks();
  bookMenuItems_.clear();
  bookPickerBookIndices_.clear();
  bookMenuItems_.push_back("Back");

  const size_t count = storage_.bookCount();
  std::vector<size_t> sortedBookIndices;
  sortedBookIndices.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    sortedBookIndices.push_back(i);
  }

  std::stable_sort(sortedBookIndices.begin(), sortedBookIndices.end(),
                   [this](size_t leftIndex, size_t rightIndex) {
                     const bool leftCurrent =
                         usingStorageBook_ && leftIndex == currentBookIndex_;
                     const bool rightCurrent =
                         usingStorageBook_ && rightIndex == currentBookIndex_;
                     if (leftCurrent != rightCurrent) {
                       return leftCurrent;
                     }

                     const uint32_t leftRecent =
                         bookRecentSequence(storage_.bookPath(leftIndex));
                     const uint32_t rightRecent =
                         bookRecentSequence(storage_.bookPath(rightIndex));
                     const bool leftHasRecent = leftRecent > 0;
                     const bool rightHasRecent = rightRecent > 0;
                     if (leftHasRecent != rightHasRecent) {
                       return leftHasRecent;
                     }
                     if (leftRecent != rightRecent) {
                       return leftRecent > rightRecent;
                     }

                     return false;
                   });

  for (size_t bookIndex : sortedBookIndices) {
    bookPickerBookIndices_.push_back(bookIndex);
    bookMenuItems_.push_back(bookMenuLabel(bookIndex));
  }

  if (count == 0) {
    Serial.println("[book-picker] No SD books available");
  }

  menuScreen_ = MenuScreen::BookPicker;
  bookPickerSelectedIndex_ = kBookPickerBackIndex;
  if (usingStorageBook_) {
    for (size_t row = 0; row < bookPickerBookIndices_.size(); ++row) {
      if (bookPickerBookIndices_[row] == currentBookIndex_) {
        bookPickerSelectedIndex_ = row + 1;
        break;
      }
    }
  }
  renderBookPicker();
}

void App::selectBookPickerItem(uint32_t nowMs) {
  if (bookPickerSelectedIndex_ == kBookPickerBackIndex || bookMenuItems_.size() <= 1) {
    menuScreen_ = MenuScreen::Main;
    renderMainMenu();
    return;
  }

  const size_t rowIndex = bookPickerSelectedIndex_ - 1;
  if (rowIndex >= bookPickerBookIndices_.size()) {
    renderBookPicker();
    return;
  }

  const size_t bookIndex = bookPickerBookIndices_[rowIndex];
  saveReadingPosition(true);
  if (!loadBookAtIndex(bookIndex, nowMs)) {
    Serial.println("[book-picker] Failed to load selected book");
    renderBookPicker();
    return;
  }

  menuScreen_ = MenuScreen::Main;
  setState(AppState::Paused, nowMs);
  saveReadingPosition(true);
}

void App::openChapterPicker() {
  chapterMenuItems_.clear();
  chapterMenuItems_.push_back("Back");

  if (chapterMarkers_.empty()) {
    chapterMenuItems_.push_back("Start of book");
    chapterPickerSelectedIndex_ = kChapterPickerFallbackIndex;
    Serial.println("[chapter-picker] No chapter markers found; showing start fallback");
  } else {
    for (size_t i = 0; i < chapterMarkers_.size(); ++i) {
      chapterMenuItems_.push_back(chapterMenuLabel(i));
    }

    size_t selectedChapter = 0;
    const size_t currentWordIndex = reader_.currentIndex();
    for (size_t i = 0; i < chapterMarkers_.size(); ++i) {
      if (chapterMarkers_[i].wordIndex <= currentWordIndex) {
        selectedChapter = i;
      }
    }
    chapterPickerSelectedIndex_ = selectedChapter + 1;
  }

  menuScreen_ = MenuScreen::ChapterPicker;
  renderChapterPicker();
}

void App::selectChapterPickerItem(uint32_t nowMs) {
  if (chapterPickerSelectedIndex_ == kChapterPickerBackIndex || chapterMenuItems_.size() <= 1) {
    menuScreen_ = MenuScreen::Main;
    renderMainMenu();
    return;
  }

  if (chapterMarkers_.empty()) {
    reader_.seekTo(0);
    menuScreen_ = MenuScreen::Main;
    setState(AppState::Paused, nowMs);
    saveReadingPosition(true);
    Serial.println("[chapter-picker] jumped to start of book");
    return;
  }

  const size_t chapterIndex = chapterPickerSelectedIndex_ - 1;
  if (chapterIndex >= chapterMarkers_.size()) {
    renderChapterPicker();
    return;
  }

  reader_.seekTo(chapterMarkers_[chapterIndex].wordIndex);
  menuScreen_ = MenuScreen::Main;
  setState(AppState::Paused, nowMs);
  saveReadingPosition(true);
  Serial.printf("[chapter-picker] jumped to %s at word %u\n",
                chapterMarkers_[chapterIndex].title.c_str(),
                static_cast<unsigned int>(chapterMarkers_[chapterIndex].wordIndex));
}

void App::enterUsbTransfer(uint32_t nowMs) {
  Serial.println("[app] entering USB transfer mode");
  saveReadingPosition(true);
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;
  setState(AppState::UsbTransfer, nowMs);

  storage_.end();
  if (!usbTransfer_.begin(true)) {
    Serial.printf("[app] USB transfer failed: %s\n", usbTransfer_.statusMessage());
    display_.renderStatus("USB", "SD not ready", "Returning");
    const bool storageReady = storage_.begin();
    if (storageReady) {
      storage_.listBooks();
    }
    setState(AppState::Paused, nowMs);
    return;
  }

  const uint64_t sizeMb = usbTransfer_.cardSizeBytes() / (1024ULL * 1024ULL);
  Serial.printf("[app] USB transfer active (%llu MB). Eject from computer when finished.\n",
                sizeMb);
  display_.renderStatus("USB", "Copy books now", "Eject then hold BOOT");
}

void App::updateUsbTransfer(uint32_t nowMs) {
  if (!usbTransfer_.active()) {
    return;
  }

  const bool bootExitRequested =
      button_.isHeld() && nowMs - button_.lastEdgeMs() >= kUsbTransferExitHoldMs;
  if (!usbTransfer_.ejected() && !bootExitRequested) {
    return;
  }

  if (bootExitRequested && !usbTransfer_.ejected()) {
    Serial.println("[app] leaving USB transfer by BOOT hold; make sure host was ejected first");
  }

  exitUsbTransfer(nowMs);
}

void App::exitUsbTransfer(uint32_t nowMs) {
  Serial.println("[app] USB transfer ejected; remounting SD");
  display_.renderStatus("USB", "Remounting SD", "");
  usbTransfer_.end();

  const bool storageReady = storage_.begin();
  if (storageReady) {
    storage_.listBooks();
    const int refreshedBookIndex = findBookIndexByPath(currentBookPath_);
    if (refreshedBookIndex >= 0) {
      currentBookIndex_ = static_cast<size_t>(refreshedBookIndex);
    } else if (storage_.bookCount() > 0) {
      loadBookAtIndex(0, nowMs);
    }
  } else {
    Serial.println("[app] SD remount failed after USB transfer");
  }

  menuScreen_ = MenuScreen::Main;
  setState(AppState::Paused, nowMs);
}

void App::enterSleep(uint32_t nowMs) {
  Serial.println("[app] entering light sleep; press BOOT to wake");
  saveReadingPosition(true);
  setState(AppState::Sleeping, nowMs);
  Serial.flush();
  delay(200);

  display_.prepareForSleep();
  storage_.end();
  touch_.end();
  touchInitialized_ = false;

  BoardConfig::lightSleepUntilBootButton();
  wakeFromSleep();
}

void App::wakeFromSleep() {
  const uint32_t nowMs = millis();
  Serial.println("[app] woke from light sleep");

  BoardConfig::begin();
  button_.begin();
  const bool displayReady = display_.begin();
  touchInitialized_ = touch_.begin();
  const bool storageReady = storage_.begin();
  if (storageReady) {
    storage_.listBooks();
  }

  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;
  menuScreen_ = MenuScreen::Main;
  lastStateLogMs_ = nowMs;
  state_ = AppState::Paused;

  if (displayReady) {
    renderReaderWord();
  }
}

bool App::restoreSavedBook(uint32_t nowMs) {
  const String savedPath = preferences_.getString(kPrefBookPath, "");
  if (savedPath.isEmpty()) {
    return false;
  }

  const int bookIndex = findBookIndexByPath(savedPath);
  if (bookIndex < 0) {
    Serial.printf("[app] saved book not found: %s\n", savedPath.c_str());
    return false;
  }

  if (!loadBookAtIndex(static_cast<size_t>(bookIndex), nowMs, true)) {
    return false;
  }

  Serial.printf("[app] restored %s at word %u\n", savedPath.c_str(),
                static_cast<unsigned int>(reader_.currentIndex()));
  return true;
}

void App::saveReadingPosition(bool force) {
  if (!usingStorageBook_ || currentBookPath_.isEmpty()) {
    return;
  }

  const size_t wordIndex = reader_.currentIndex();
  if (!force && wordIndex == lastSavedWordIndex_) {
    return;
  }

  preferences_.putString(kPrefBookPath, currentBookPath_);
  preferences_.putUInt(bookPositionKey(currentBookPath_).c_str(), static_cast<uint32_t>(wordIndex));
  preferences_.putUInt(bookWordCountKey(currentBookPath_).c_str(),
                       static_cast<uint32_t>(reader_.wordCount()));
  preferences_.putUInt(kPrefLegacyWordIndex, static_cast<uint32_t>(wordIndex));
  preferences_.putUShort(kPrefWpm, reader_.wpm());
  markBookRecent(currentBookPath_);
  lastSavedWordIndex_ = wordIndex;
  Serial.printf("[app] saved position word=%u book=%s\n", static_cast<unsigned int>(wordIndex),
                currentBookPath_.c_str());
}

bool App::loadBookAtIndex(size_t index, uint32_t nowMs, bool allowLegacyPositionFallback) {
  BookContent book;
  String loadedPath;
  size_t loadedIndex = index;
  if (!storage_.loadBookContent(index, book, &loadedPath, &loadedIndex)) {
    return false;
  }

  chapterMarkers_ = std::move(book.chapters);
  reader_.setWords(std::move(book.words), nowMs);
  currentBookIndex_ = loadedIndex;
  currentBookPath_ = loadedPath;
  currentBookTitle_ = book.title.isEmpty() ? displayNameForPath(loadedPath) : book.title;
  lastSavedWordIndex_ = static_cast<size_t>(-1);
  usingStorageBook_ = true;
  preferences_.putString(kPrefBookPath, currentBookPath_);
  preferences_.putUInt(bookWordCountKey(currentBookPath_).c_str(),
                       static_cast<uint32_t>(reader_.wordCount()));
  markBookRecent(currentBookPath_);

  const uint32_t savedWordIndex =
      savedWordIndexForBook(currentBookPath_, allowLegacyPositionFallback);
  if (savedWordIndex != kNoSavedWordIndex) {
    reader_.seekTo(savedWordIndex);
    lastSavedWordIndex_ = reader_.currentIndex();
    Serial.printf("[app] restored book position word=%u key=%s\n",
                  static_cast<unsigned int>(reader_.currentIndex()),
                  bookPositionKey(currentBookPath_).c_str());
  }

  lastProgressSaveMs_ = nowMs;
  Serial.printf("[app] loaded SD book[%u/%u]: %s (%u chapters)\n",
                static_cast<unsigned int>(loadedIndex + 1),
                static_cast<unsigned int>(storage_.bookCount()), loadedPath.c_str(),
                static_cast<unsigned int>(chapterMarkers_.size()));
  return true;
}

String App::bookPositionKey(const String &bookPath) const {
  char key[10];
  std::snprintf(key, sizeof(key), "p%08lx", static_cast<unsigned long>(hashBookPath(bookPath)));
  return String(key);
}

String App::bookWordCountKey(const String &bookPath) const {
  char key[10];
  std::snprintf(key, sizeof(key), "c%08lx", static_cast<unsigned long>(hashBookPath(bookPath)));
  return String(key);
}

String App::bookRecentKey(const String &bookPath) const {
  char key[10];
  std::snprintf(key, sizeof(key), "r%08lx", static_cast<unsigned long>(hashBookPath(bookPath)));
  return String(key);
}

uint32_t App::nextRecentSequence() {
  uint32_t sequence = preferences_.getUInt(kPrefRecentSeq, 0);
  if (sequence == 0xFFFFFFFEUL) {
    sequence = 0;
  }
  ++sequence;
  preferences_.putUInt(kPrefRecentSeq, sequence);
  return sequence;
}

uint32_t App::bookRecentSequence(const String &bookPath) {
  return preferences_.getUInt(bookRecentKey(bookPath).c_str(), 0);
}

void App::markBookRecent(const String &bookPath) {
  if (bookPath.isEmpty()) {
    return;
  }

  preferences_.putUInt(bookRecentKey(bookPath).c_str(), nextRecentSequence());
}

uint32_t App::savedWordIndexForBook(const String &bookPath, bool allowLegacyFallback) {
  const String key = bookPositionKey(bookPath);
  if (preferences_.isKey(key.c_str())) {
    return preferences_.getUInt(key.c_str(), 0);
  }

  if (allowLegacyFallback && preferences_.isKey(kPrefLegacyWordIndex)) {
    const uint32_t legacyWordIndex = preferences_.getUInt(kPrefLegacyWordIndex, 0);
    preferences_.putUInt(key.c_str(), legacyWordIndex);
    Serial.printf("[app] migrated legacy position word=%u to key=%s\n",
                  static_cast<unsigned int>(legacyWordIndex), key.c_str());
    return legacyWordIndex;
  }

  return kNoSavedWordIndex;
}

bool App::bookProgressPercent(size_t bookIndex, uint8_t &percent) {
  size_t wordIndex = 0;
  size_t wordCount = 0;

  if (usingStorageBook_ && bookIndex == currentBookIndex_) {
    wordIndex = reader_.currentIndex();
    wordCount = reader_.wordCount();
  } else {
    const String path = storage_.bookPath(bookIndex);
    const String positionKey = bookPositionKey(path);
    const String countKey = bookWordCountKey(path);
    if (!preferences_.isKey(positionKey.c_str()) || !preferences_.isKey(countKey.c_str())) {
      return false;
    }

    wordIndex = preferences_.getUInt(positionKey.c_str(), 0);
    wordCount = preferences_.getUInt(countKey.c_str(), 0);
  }

  if (wordCount <= 1) {
    return false;
  }

  wordIndex = std::min(wordIndex, wordCount - 1);
  const size_t progress = (wordIndex * static_cast<size_t>(100)) / (wordCount - 1);
  percent = static_cast<uint8_t>(std::min(static_cast<size_t>(100), progress));
  return true;
}

int App::findBookIndexByPath(const String &path) const {
  for (size_t i = 0; i < storage_.bookCount(); ++i) {
    if (storage_.bookPath(i) == path) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void App::renderMenu() {
  if (menuScreen_ == MenuScreen::BookPicker) {
    renderBookPicker();
  } else if (menuScreen_ == MenuScreen::ChapterPicker) {
    renderChapterPicker();
  } else {
    renderMainMenu();
  }
}

void App::renderMainMenu() {
  display_.renderMenu(kMenuItems, MenuItemCount, menuSelectedIndex_);
}

void App::renderBookPicker() {
  display_.renderMenu(bookMenuItems_, bookPickerSelectedIndex_);
}

void App::renderChapterPicker() {
  display_.renderMenu(chapterMenuItems_, chapterPickerSelectedIndex_);
}

String App::bookMenuLabel(size_t bookIndex) {
  String label = storage_.bookDisplayName(bookIndex);
  uint8_t percent = 0;
  const bool hasProgress = bookProgressPercent(bookIndex, percent);

  String prefix;
  if (usingStorageBook_ && bookIndex == currentBookIndex_) {
    prefix += "* ";
  }
  if (hasProgress) {
    prefix += String(percent) + "% ";
  }

  return prefix + label;
}

String App::chapterMenuLabel(size_t chapterIndex) const {
  if (chapterIndex >= chapterMarkers_.size()) {
    return "";
  }

  String label = String(chapterIndex + 1) + " " + chapterMarkers_[chapterIndex].title;
  if (label.length() > 36) {
    label = label.substring(0, 36) + "...";
  }

  const size_t currentIndex = reader_.currentIndex();
  const size_t startIndex = chapterMarkers_[chapterIndex].wordIndex;
  const size_t endIndex = (chapterIndex + 1 < chapterMarkers_.size())
                              ? chapterMarkers_[chapterIndex + 1].wordIndex
                              : reader_.wordCount();
  if (currentIndex >= startIndex && currentIndex < endIndex) {
    label += " *";
  }
  return label;
}

String App::currentChapterLabel() const {
  if (chapterMarkers_.empty()) {
    return currentBookTitle_.isEmpty() ? "Start" : currentBookTitle_;
  }

  size_t currentChapter = 0;
  const size_t currentIndex = reader_.currentIndex();
  for (size_t i = 0; i < chapterMarkers_.size(); ++i) {
    if (chapterMarkers_[i].wordIndex <= currentIndex) {
      currentChapter = i;
    }
  }

  return chapterMarkers_[currentChapter].title;
}

uint8_t App::readingProgressPercent() const {
  const size_t count = reader_.wordCount();
  if (count <= 1) {
    return 0;
  }

  const size_t index = std::min(reader_.currentIndex(), count - 1);
  const size_t percent = (index * 100UL) / (count - 1);
  return static_cast<uint8_t>(std::min(static_cast<size_t>(100), percent));
}

void App::renderReaderWord() {
  const bool showFooter = state_ != AppState::Playing;
  display_.renderRsvpWord(reader_.currentWord(), currentChapterLabel(), readingProgressPercent(),
                          showFooter);
}

void App::renderWpmFeedback(uint32_t nowMs) {
  wpmFeedbackVisible_ = true;
  wpmFeedbackUntilMs_ = nowMs + kWpmFeedbackMs;
  display_.renderRsvpWordWithWpm(reader_.currentWord(), reader_.wpm(), currentChapterLabel(),
                                 readingProgressPercent(), true);
}
