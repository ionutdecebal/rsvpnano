#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

#include "app/AppState.h"
#include "display/DisplayManager.h"
#include "input/ButtonHandler.h"
#include "input/TouchHandler.h"
#include "reader/ReadingLoop.h"
#include "storage/StorageManager.h"
#include "usb/UsbMassStorageManager.h"

class App {
 public:
  App();

  void begin();
  void update(uint32_t nowMs);

 private:
  struct PausedTouchSession {
    bool active = false;
    uint16_t startX = 0;
    uint16_t startY = 0;
    uint16_t lastX = 0;
    uint16_t lastY = 0;
    uint32_t startMs = 0;
    uint32_t lastMs = 0;
    size_t startWordIndex = 0;
    int scrubStepsApplied = 0;
  };

  enum class TouchIntent {
    None,
    PlayHold,
    Scrub,
    Wpm,
  };

  enum class MenuScreen {
    Main,
    SettingsHome,
    SettingsDisplay,
    SettingsPacing,
    TypographyTuning,
    BookPicker,
    ChapterPicker,
    RestartConfirm,
  };

  void setState(AppState nextState, uint32_t nowMs);
  void updateState(uint32_t nowMs);
  void updateReader(uint32_t nowMs);
  void updateWpmFeedback(uint32_t nowMs);
  void maybeSaveReadingPosition(uint32_t nowMs);
  void handleBootButton(uint32_t nowMs);
  void handlePowerButton(uint32_t nowMs);
  void toggleMenuFromPowerButton(uint32_t nowMs);
  void openMainMenu(uint32_t nowMs);
  void cycleBrightness();
  void cycleThemeMode(uint32_t nowMs);
  void togglePhantomWords(uint32_t nowMs);
  void cycleReaderFontSize(uint32_t nowMs);
  void applyDisplayPreferences(uint32_t nowMs, bool rerender = true);
  void applyTypographySettings(uint32_t nowMs, bool rerender = true);
  uint8_t currentBrightnessPercent() const;
  bool updateBatteryStatus(uint32_t nowMs, bool force = false);
  void handleTouch(uint32_t nowMs);
  void applyPausedTouchGesture(const TouchEvent &event, uint32_t nowMs);
  int scrubStepsForDrag(int deltaX) const;
  void applyScrubTarget(int targetSteps);
  void applyMenuTouchGesture(const TouchEvent &event, uint32_t nowMs);
  void moveMenuSelection(int direction);
  void selectMenuItem(uint32_t nowMs);
  void openSettings();
  void selectSettingsItem(uint32_t nowMs);
  void openTypographyTuning();
  void selectTypographyTuningItem(uint32_t nowMs);
  void cycleTypographyPreviewSample(int direction);
  void rebuildSettingsMenuItems();
  void applyPacingSettings();
  String pacingLevelLabel(uint8_t levelIndex) const;
  String themeModeLabel() const;
  String phantomWordsLabel() const;
  String readerFontSizeLabel() const;
  String typographyTuningLabel() const;
  String typographyTuningValueLabel() const;
  void openBookPicker();
  void selectBookPickerItem(uint32_t nowMs);
  void openChapterPicker();
  void selectChapterPickerItem(uint32_t nowMs);
  void openRestartConfirm();
  void selectRestartConfirmItem(uint32_t nowMs);
  void enterUsbTransfer(uint32_t nowMs);
  void updateUsbTransfer(uint32_t nowMs);
  void exitUsbTransfer(uint32_t nowMs);
  void enterPowerOff(uint32_t nowMs);
  void enterSleep(uint32_t nowMs);
  void wakeFromSleep();
  bool restoreSavedBook(uint32_t nowMs);
  void saveReadingPosition(bool force = false);
  bool loadBookAtIndex(size_t index, uint32_t nowMs, bool allowLegacyPositionFallback = false);
  String bookPositionKey(const String &bookPath) const;
  String bookWordCountKey(const String &bookPath) const;
  String bookRecentKey(const String &bookPath) const;
  uint32_t nextRecentSequence();
  uint32_t bookRecentSequence(const String &bookPath);
  void markBookRecent(const String &bookPath);
  uint32_t savedWordIndexForBook(const String &bookPath, bool allowLegacyFallback = false);
  bool bookProgressPercent(size_t bookIndex, uint8_t &percent);
  int findBookIndexByPath(const String &path) const;
  void renderMenu();
  void renderMainMenu();
  void renderSettings();
  void renderTypographyTuning();
  void renderBookPicker();
  void renderChapterPicker();
  void renderRestartConfirm();
  DisplayManager::LibraryItem libraryItemForBook(size_t bookIndex);
  String chapterMenuLabel(size_t chapterIndex) const;
  String currentChapterLabel() const;
  uint8_t readingProgressPercent() const;
  void renderReaderWord();
  void renderContextPreview();
  void renderWpmFeedback(uint32_t nowMs);
  size_t phantomBeforeCharTarget() const;
  size_t phantomAfterCharTarget() const;
  String collectPhantomBeforeText(size_t currentIndex, size_t charTarget) const;
  String collectPhantomAfterText(size_t currentIndex, size_t charTarget) const;
  String phantomBeforeText() const;
  String phantomAfterText() const;
  bool isParagraphStart(size_t wordIndex) const;
  size_t paragraphStartAtOrBefore(size_t wordIndex) const;
  size_t contextPreviewAnchorIndex(size_t currentIndex) const;
  void invalidateContextPreviewWindow();
  void renderStorageStatus(const char *title, const char *line1, const char *line2,
                           int progressPercent);
  static void handleStorageStatus(void *context, const char *title, const char *line1,
                                  const char *line2, int progressPercent);
  const char *stateName(AppState state) const;
  const char *touchPhaseName(TouchPhase phase) const;

  AppState state_ = AppState::Booting;
  DisplayManager display_;
  ReadingLoop reader_;
  ButtonHandler button_;
  ButtonHandler powerButton_;
  TouchHandler touch_;
  StorageManager storage_;
  UsbMassStorageManager usbTransfer_;
  Preferences preferences_;
  PausedTouchSession pausedTouch_;
  TouchIntent pausedTouchIntent_ = TouchIntent::None;

  uint32_t bootStartedMs_ = 0;
  uint32_t lastStateLogMs_ = 0;
  uint32_t wpmFeedbackUntilMs_ = 0;
  uint32_t lastProgressSaveMs_ = 0;
  uint32_t lastBatterySampleMs_ = 0;
  size_t lastSavedWordIndex_ = static_cast<size_t>(-1);
  size_t contextPreviewStartIndex_ = 0;
  size_t currentBookIndex_ = 0;
  size_t menuSelectedIndex_ = 0;
  size_t settingsSelectedIndex_ = 0;
  size_t bookPickerSelectedIndex_ = 0;
  size_t chapterPickerSelectedIndex_ = 0;
  size_t restartConfirmSelectedIndex_ = 0;
  uint8_t brightnessLevelIndex_ = 4;
  uint8_t readerFontSizeIndex_ = 0;
  uint8_t pacingLongWordLevelIndex_ = 2;
  uint8_t pacingComplexWordLevelIndex_ = 2;
  uint8_t pacingPunctuationLevelIndex_ = 2;
  size_t typographyTuningSelectedIndex_ = 1;
  size_t typographyPreviewSampleIndex_ = 0;
  MenuScreen menuScreen_ = MenuScreen::Main;
  std::vector<String> settingsMenuItems_;
  std::vector<DisplayManager::LibraryItem> bookMenuItems_;
  std::vector<size_t> bookPickerBookIndices_;
  std::vector<String> chapterMenuItems_;
  std::vector<ChapterMarker> chapterMarkers_;
  std::vector<size_t> paragraphStarts_;
  String currentBookPath_;
  String currentBookTitle_;
  String batteryLabel_;
  bool touchInitialized_ = false;
  bool touchPlayHeld_ = false;
  bool bootButtonReleasedSinceBoot_ = false;
  bool bootButtonLongPressHandled_ = false;
  bool powerButtonReleasedSinceBoot_ = false;
  bool powerButtonLongPressHandled_ = false;
  bool powerOffStarted_ = false;
  bool contextViewVisible_ = false;
  bool contextPreviewWindowValid_ = false;
  bool wpmFeedbackVisible_ = false;
  bool usingStorageBook_ = false;
  bool phantomWordsEnabled_ = true;
  bool darkMode_ = true;
  bool nightMode_ = false;
  DisplayManager::TypographyConfig typographyConfig_;
};
