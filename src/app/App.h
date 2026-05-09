#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

#include "app/AppState.h"
#include "app/Localization.h"
#include "audio/AudioManager.h"
#include "display/DisplayManager.h"
#include "input/ButtonHandler.h"
#include "input/TouchHandler.h"
#include "reader/ReadingLoop.h"
#include "storage/SettingsFile.h"
#include "storage/StorageManager.h"
#include "timer/FocusTimer.h"
#include "update/OtaUpdater.h"
#include "usb/UsbMassStorageManager.h"

class App {
 public:
  enum class ReaderMode : uint8_t {
    Rsvp = 0,
    Scroll = 1,
  };

  enum class HandednessMode : uint8_t {
    Right = 0,
    Left = 1,
  };

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
    int gestureStepsApplied = 0;
    int32_t browseOffsetPermille = 0;
  };

  enum class TouchIntent {
    None,
    PlayHold,
    Scrub,
    BrowseScroll,
    Wpm,
  };

  enum class MenuScreen {
    Main,
    SettingsHome,
    SettingsDisplay,
    SettingsPacing,
    WifiSettings,
    WifiNetworks,
    TextEntry,
    TypographyTuning,
    BookPicker,
    ChapterPicker,
    RestartConfirm,
    FocusTimerGenres,
    FocusTimerSession,
  };

  enum class FooterMetricMode : uint8_t {
    Percentage = 0,
    ChapterTime = 1,
    BookTime = 2,
  };

  enum class TextEntryPurpose : uint8_t {
    None,
    WifiPassword,
    OtaOwner,
  };

  enum class KeyboardMode : uint8_t {
    Lower,
    Upper,
    Symbols,
  };

  enum class TextEntryAction : uint8_t {
    Insert,
    SetLower,
    SetUpper,
    SetSymbols,
    Space,
    Backspace,
    Clear,
    ToggleMask,
    Save,
    Cancel,
  };

  struct WifiNetworkInfo {
    String ssid;
    int32_t rssi = 0;
    uint8_t authMode = 0;
  };

  struct TextEntryButton {
    DisplayManager::Button view;
    TextEntryAction action = TextEntryAction::Insert;
    String payload;
  };

  struct TextEntrySession {
    bool active = false;
    TextEntryPurpose purpose = TextEntryPurpose::None;
    KeyboardMode mode = KeyboardMode::Lower;
    MenuScreen returnScreen = MenuScreen::Main;
    String title;
    String prompt;
    String helperText;
    String value;
    String contextValue;
    size_t maxLength = 63;
    bool masked = false;
    bool revealValue = false;
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
  void cycleUiLanguage(uint32_t nowMs);
  void cycleReaderMode(uint32_t nowMs);
  void cycleHandednessMode(uint32_t nowMs);
  void togglePhantomWords(uint32_t nowMs);
  void cycleReaderFontSize(uint32_t nowMs);
  void applyDisplayPreferences(uint32_t nowMs, bool rerender = true);
  void applyHandednessSettings(uint32_t nowMs, bool rerender = true);
  void applyTypographySettings(uint32_t nowMs, bool rerender = true);
  uint8_t currentBrightnessPercent() const;
  bool updateBatteryStatus(uint32_t nowMs, bool force = false);
  void handleTouch(uint32_t nowMs);
  void applyPausedTouchGesture(const TouchEvent &event, uint32_t nowMs);
  void handleReaderTap(uint16_t x, uint16_t y, uint32_t nowMs);
  bool handleFooterMetricTap(uint16_t x, uint16_t y, uint32_t nowMs);
  void requestReaderPauseAtSentenceEnd(uint32_t nowMs);
  void finalizeReaderPause(uint32_t nowMs);
  bool shouldFinalizeReaderPause(uint32_t nowMs) const;
  void resetReaderTapTracking();
  bool isFooterMetricTap(uint16_t x, uint16_t y) const;
  bool isPreviousSentenceTap(uint16_t x) const;
  bool readerFooterVisible() const;
  void rewindReaderSentence(uint32_t nowMs);
  int scrubStepsForDrag(int deltaX) const;
  void applyScrubTarget(int targetSteps, uint32_t nowMs);
  int browseScrollRatePermille(uint16_t y) const;
  void applyBrowseHoldScroll(uint16_t y, uint32_t elapsedMs, uint32_t nowMs);
  void renderContextBrowsePreview(size_t currentIndex, uint16_t scrollProgressPermille);
  void applyMenuTouchGesture(const TouchEvent &event, uint32_t nowMs);
  void applyFocusTimerTouch(const TouchEvent &event, uint32_t nowMs);
  void moveMenuSelection(int direction);
  void selectMenuItem(uint32_t nowMs);
  void openFocusTimer();
  void updateFocusTimer(uint32_t nowMs);
  void resetFocusTimer();
  void rebuildFocusTimerGenreMenuItems();
  void selectFocusTimerGenre(uint32_t nowMs);
  void openSettings();
  void selectSettingsItem(uint32_t nowMs);
  void openWifiSettings();
  void selectWifiSettingsItem(uint32_t nowMs);
  void openTypographyTuning();
  void selectTypographyTuningItem(uint32_t nowMs);
  void cycleTypographyPreviewSample(int direction);
  void rebuildSettingsMenuItems();
  void applyPacingSettings();
  void maybeAutoCheckForUpdates(uint32_t nowMs);
  void runFirmwareUpdate(const OtaUpdater::Config &config, bool automatic, uint32_t nowMs);
  OtaUpdater::Config preferredOtaConfig();
  void scanWifiNetworks();
  void renderWifiNetworks();
  void selectWifiNetworkItem(uint32_t nowMs);
  void openTextEntry(TextEntryPurpose purpose, const String &title, const String &prompt,
                     const String &helperText, const String &initialValue,
                     const String &contextValue, bool masked, size_t maxLength,
                     MenuScreen returnScreen);
  void rebuildTextEntryButtons();
  void renderTextEntry();
  bool handleTextEntryTap(uint16_t x, uint16_t y, uint32_t nowMs);
  void activateTextEntryButton(size_t buttonIndex, uint32_t nowMs);
  void commitTextEntry(uint32_t nowMs);
  String configuredWifiSsid();
  bool otaAutoCheckEnabled();
  String otaOwnerLabel();
  String pacingDelayLabel(uint16_t delayMs) const;
  String firmwareUpdateMenuLabel() const;
  String themeModeLabel() const;
  String phantomWordsLabel() const;
  String focusHighlightLabel() const;
  String uiLanguageLabel() const;
  String readerModeLabel() const;
  String handednessLabel() const;
  String readerFontSizeLabel() const;
  String readerTypefaceLabel() const;
  String typographyTuningLabel() const;
  String typographyTuningValueLabel() const;
  String uiText(UiText key) const;
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
  void renderFocusTimerGenres();
  void renderFocusTimerSession();
  void renderActiveReader(uint32_t nowMs);
  void renderScrollReader(uint32_t nowMs, const String &overlayText = "");
  DisplayManager::LibraryItem libraryItemForBook(size_t bookIndex);
  String chapterMenuLabel(size_t chapterIndex) const;
  size_t currentChapterIndex() const;
  String currentChapterLabel() const;
  String currentFooterMetricLabel() const;
  uint32_t estimatedReadingTimeRemainingMs(size_t startIndex, size_t endIndex) const;
  String formatReadingTimeRemaining(uint32_t remainingMs) const;
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
  void updateContextPreviewWindow(size_t currentIndex);
  void invalidateContextPreviewWindow();
  void renderStorageStatus(const char *title, const char *line1, const char *line2,
                           int progressPercent);
  static void handleStorageStatus(void *context, const char *title, const char *line1,
                                  const char *line2, int progressPercent);
  const char *stateName(AppState state) const;
  const char *touchPhaseName(TouchPhase phase) const;
  bool isFocusTimerMenuScreen(MenuScreen screen) const;
  bool scrollModeEnabled() const;
  void markSettingsFileDirty();
  void maybeSaveSettingsToSd(uint32_t nowMs);
  void saveSettingsToSd();
  SettingsData buildSettingsData();
  void applySettingsFromSd();
  void applyUiOrientation(BoardConfig::UiOrientation orientation);
  void applyReaderUiOrientation();
  BoardConfig::UiOrientation readerUiOrientation() const;
  bool uiRotated180() const;
  uint8_t effectiveAnchorPercent() const;
  DisplayManager::TypographyConfig effectiveTypographyConfig() const;
  uint32_t currentReaderContentToken() const;
  String formatFocusTimerRemaining(uint32_t nowMs) const;
  String focusTimerCountsLabel() const;
  void playFocusTimerCompletionCue();

  AppState state_ = AppState::Booting;
  DisplayManager display_;
  AudioManager audio_;
  FocusTimer focusTimer_;
  ReadingLoop reader_;
  ButtonHandler button_;
  ButtonHandler powerButton_;
  TouchHandler touch_;
  StorageManager storage_;
  SettingsFile settingsFile_;
  OtaUpdater otaUpdater_;
  UsbMassStorageManager usbTransfer_;
  Preferences preferences_;
  PausedTouchSession pausedTouch_;
  TouchIntent pausedTouchIntent_ = TouchIntent::None;

  bool settingsFileDirty_ = false;
  uint32_t settingsFileDirtyMs_ = 0;
  uint32_t bootStartedMs_ = 0;
  uint32_t lastStateLogMs_ = 0;
  uint32_t wpmFeedbackUntilMs_ = 0;
  uint32_t lastProgressSaveMs_ = 0;
  uint32_t lastBatterySampleMs_ = 0;
  uint32_t lastScrollAnimationRenderMs_ = 0;
  uint32_t lastReaderTapMs_ = 0;
  size_t lastSavedWordIndex_ = static_cast<size_t>(-1);
  size_t contextPreviewStartIndex_ = 0;
  size_t contextPreviewCurrentLocalIndex_ = static_cast<size_t>(-1);
  size_t currentBookIndex_ = 0;
  size_t menuSelectedIndex_ = 0;
  size_t settingsSelectedIndex_ = 0;
  size_t wifiNetworkSelectedIndex_ = 0;
  size_t bookPickerSelectedIndex_ = 0;
  size_t chapterPickerSelectedIndex_ = 0;
  size_t restartConfirmSelectedIndex_ = 0;
  size_t focusTimerGenreSelectedIndex_ = 0;
  uint8_t brightnessLevelIndex_ = 4;
  uint8_t readerFontSizeIndex_ = 0;
  uint16_t pacingLongWordDelayMs_ = 200;
  uint16_t pacingComplexWordDelayMs_ = 200;
  uint16_t pacingPunctuationDelayMs_ = 200;
  size_t typographyTuningSelectedIndex_ = 1;
  size_t typographyPreviewSampleIndex_ = 0;
  MenuScreen menuScreen_ = MenuScreen::Main;
  MenuScreen restartConfirmReturnScreen_ = MenuScreen::Main;
  std::vector<String> settingsMenuItems_;
  std::vector<String> focusTimerGenreMenuItems_;
  std::vector<DisplayManager::LibraryItem> wifiNetworkMenuItems_;
  std::vector<DisplayManager::LibraryItem> bookMenuItems_;
  std::vector<size_t> bookPickerBookIndices_;
  std::vector<String> chapterMenuItems_;
  std::vector<ChapterMarker> chapterMarkers_;
  std::vector<size_t> paragraphStarts_;
  std::vector<DisplayManager::ContextWord> contextPreviewWords_;
  std::vector<WifiNetworkInfo> wifiNetworks_;
  std::vector<TextEntryButton> textEntryButtons_;
  String currentBookPath_;
  String currentBookTitle_;
  String batteryLabel_;
  TextEntrySession textEntrySession_;
  uint16_t lastReaderTapX_ = 0;
  uint16_t lastReaderTapY_ = 0;
  bool touchInitialized_ = false;
  bool touchPlayHeld_ = false;
  bool playLocked_ = false;
  bool pauseAtSentenceEndRequested_ = false;
  bool lastReaderTapValid_ = false;
  bool bootButtonReleasedSinceBoot_ = false;
  bool bootButtonLongPressHandled_ = false;
  bool powerButtonReleasedSinceBoot_ = false;
  bool powerButtonLongPressHandled_ = false;
  bool powerOffStarted_ = false;
  bool focusTimerCancelHoldTriggered_ = false;
  bool contextViewVisible_ = false;
  bool contextPreviewWindowValid_ = false;
  bool wpmFeedbackVisible_ = false;
  bool usingStorageBook_ = false;
  bool phantomWordsEnabled_ = true;
  FooterMetricMode footerMetricMode_ = FooterMetricMode::Percentage;
  bool darkMode_ = true;
  bool nightMode_ = false;
  UiLanguage uiLanguage_ = UiLanguage::English;
  ReaderMode readerMode_ = ReaderMode::Rsvp;
  HandednessMode handednessMode_ = HandednessMode::Right;
  DisplayManager::TypographyConfig typographyConfig_;
};
