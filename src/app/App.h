#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <memory>
#include <vector>

#include "app/AppState.h"
#include "app/MenuRepeat.h"
#include "board/Board.h"
#include "book/BookMetadata.h"
#include "display/DisplayManager.h"
#include "input/InputButtons.h"
#include "input/InputTouch.h"
#include "reader/ReadingLoop.h"
#include "rss/RssFeedManager.h"
#include "standby/Screensaver.h"
#include "storage/index/IndexedBookStore.h"
#include "storage/StorageManager.h"
#include "sync/CompanionSyncManager.h"
#include "timer/FocusTimer.h"
#include "ui/Localization.h"
#include "update/OtaUpdater.h"
#include "usb/UsbMassStorageManager.h"

using TouchEvent = Input::Touch::Event;
using TouchPhase = Input::Touch::Phase;

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
  static constexpr size_t kOtaVersionLabelMax = 32;
  static constexpr size_t kOtaSummaryLabelMax = 40;
  static constexpr size_t kOtaDetailLabelMax = 96;

  struct OtaCheckResult {
    OtaUpdater::ResultCode code = OtaUpdater::ResultCode::MetadataFailed;
    char currentVersion[kOtaVersionLabelMax] = {};
    char latestVersion[kOtaVersionLabelMax] = {};
    char summary[kOtaSummaryLabelMax] = {};
    char detail[kOtaDetailLabelMax] = {};
  };

  struct OtaCheckTaskParams {
    OtaUpdater::Config config;
    QueueHandle_t resultQueue = nullptr;
  };

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
    Scrub,
    BrowseScroll,
    Wpm,
  };

  enum class MenuScreen {
    Main,
    Articles,
    SettingsHome,
    SettingsDisplay,
    SettingsPacing,
    SettingsBattery,
    WifiSettings,
    WifiNetworkSettings,
    WifiNetworks,
    TextEntry,
    TypographyTuning,
    BookPicker,
    ChapterPicker,
    RestartConfirm,
    SdCardRepairConfirm,
    UpdateConfirm,
    PowerOffConfirm,
    QuickSettings,
    QuickSync,
    FocusTimerGenres,
    FocusTimerSession,
  };

  enum class FooterMetricMode : uint8_t {
    Percentage = 0,
    ChapterTime = 1,
    BookTime = 2,
  };

  enum class BatteryLabelMode : uint8_t {
    Percent = 0,
    TimeRemaining = 1,
    Voltage = 2,
  };

  enum class ScreensaverMode : uint8_t {
    Life = 0,
    Maze = 2,
    Voronoi = 3,
    ScreenOff = 6,
  };

  enum class PauseMode : uint8_t {
    SentenceEnd = 0,
    Instant = 1,
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
  void applyStateCpuFrequency();
  void updateState(uint32_t nowMs);
  void updateIdleStandby(uint32_t nowMs);
  void updateReader(uint32_t nowMs);
  void updateWpmFeedback(uint32_t nowMs);
  void updateBrightnessToast(uint32_t nowMs);
  void maybeSaveReadingPosition(uint32_t nowMs);
  void handleBootButton(uint32_t nowMs);
  void handlePowerButton(uint32_t nowMs);
  void handleKeyButton(uint32_t nowMs);
  void executeBootButtonSingleTap(uint32_t nowMs);
  void toggleMenuFromPowerButton(uint32_t nowMs);
  void toggleReaderPlaybackFromShortcut(uint32_t nowMs);
  void openMainMenu(uint32_t nowMs);
  void cycleBrightness(uint32_t nowMs);
  void cycleThemeMode(uint32_t nowMs);
  void cycleUiLanguage(uint32_t nowMs);
  void cycleReaderMode(uint32_t nowMs);
  void cycleHandednessMode(uint32_t nowMs);
  void toggleReaderControlsLayout(uint32_t nowMs);
  void togglePhantomWords(uint32_t nowMs);
  void cycleReaderFontSize(uint32_t nowMs);
  void applyDisplayPreferences(uint32_t nowMs, bool rerender = true);
  void applyHandednessSettings(uint32_t nowMs, bool rerender = true);
  void applyTypographySettings(uint32_t nowMs, bool rerender = true);
  uint8_t currentBrightnessPercent() const;
  bool updateBatteryStatus(uint32_t nowMs, bool force = false);
  void handleBatteryProtection(uint32_t nowMs);
  void showLowBatteryWarning(uint32_t nowMs);
  void updateBatteryWarningOverlay(uint32_t nowMs);
  void updateAutoDim(uint32_t nowMs);
  void restoreFromAutoDim(uint32_t nowMs);
  void updateBatteryRuntimeLabel(uint32_t nowMs);
  void handleTouch(uint32_t nowMs);
  void applyPausedTouchGesture(const TouchEvent &event, uint32_t nowMs);
  bool handleTopEdgeMenuSwipe(const TouchEvent &event, uint32_t nowMs, int deltaX, int deltaY,
                              bool ended);
  bool handleBottomEdgeQuickSettingsSwipe(const TouchEvent &event, uint32_t nowMs, int deltaX,
                                          int deltaY, bool ended);
  bool handleFooterMetricTap(uint16_t x, uint16_t y, uint32_t nowMs);
  bool handleBatteryBadgeTap(uint16_t x, uint16_t y, uint32_t nowMs);
  bool handlePreviousSentenceTap(uint16_t x, uint16_t y, uint32_t nowMs);
  void requestReaderPauseAtSentenceEnd(uint32_t nowMs);
  void finalizeReaderPause(uint32_t nowMs);
  bool shouldFinalizeReaderPause(uint32_t nowMs) const;
  bool isFooterMetricTap(uint16_t x, uint16_t y) const;
  bool isBatteryBadgeTap(uint16_t x, uint16_t y) const;
  bool isPreviousSentenceTap(uint16_t x, uint16_t y) const;
  bool isActivelyReading() const;
  bool readerFooterVisible() const;
  DisplayManager::ReaderChrome readerChrome() const;
  String readerFooterStatusLabel() const;
  String onOffLabel(bool enabled) const;
  int scrubStepsForDrag(int deltaX) const;
  void applyScrubTarget(int targetSteps, uint32_t nowMs);
  int browseScrollRatePermille(uint16_t y) const;
  void applyBrowseHoldScroll(uint16_t y, uint32_t elapsedMs, uint32_t nowMs);
  void renderContextBrowsePreview(size_t currentIndex, uint16_t scrollProgressPermille);
  void applyMenuTouchGesture(const TouchEvent &event, uint32_t nowMs);
  void applyFocusTimerTouch(const TouchEvent &event, uint32_t nowMs);
  bool navigateBackInMenu(uint32_t nowMs);
  bool moveMenuSelection(int direction, bool wrap);
  void selectMenuItem(uint32_t nowMs);
  bool isSettingsMenuScreen(MenuScreen screen) const;
  void openArticlesMenu();
  void selectArticlesItem(uint32_t nowMs);
  void openQuickSettings(uint32_t nowMs);
  void selectQuickSettingsItem(uint32_t nowMs);
  void openQuickSync();
  void selectQuickSyncItem(uint32_t nowMs);
  void openFocusTimer();
  void updateFocusTimer(uint32_t nowMs);
  void resetFocusTimer();
  void rebuildFocusTimerGenreMenuItems();
  void selectFocusTimerGenre(uint32_t nowMs);
  void openSettings();
  void selectSettingsItem(uint32_t nowMs);
  void selectRestructuredSettingsItem(uint32_t nowMs);
  void openBatterySettings();
  void selectBatterySettingsItem(uint32_t nowMs);
  static String cpuMhzLabel(uint32_t mhz);
  String autoDimDelayLabel() const;
  String autoDimBrightnessLabel() const;
  uint32_t nominalBatteryRuntimeMinutes() const;
  void openWifiSettings();
  void openWifiNetworkSettings();
  void selectWifiSettingsItem(uint32_t nowMs);
  void openTypographyTuning();
  void selectTypographyTuningItem(uint32_t nowMs);
  void cycleTypographyPreviewSample(int direction);
  void rebuildSettingsMenuItems();
  void applyPacingSettings();
  void maybeAutoCheckForUpdates(uint32_t nowMs);
  bool startBackgroundOtaCheck(const OtaUpdater::Config &config);
  static void otaCheckTask(void *params);
  void pollOtaCheckResult(uint32_t nowMs);
  void maybeOpenUpdateConfirm(uint32_t nowMs);
  bool updateConfirmCanOpen() const;
  bool blockNetworkActionForOtaCheck(const String &title, uint32_t nowMs);
  void runFirmwareUpdate(const OtaUpdater::Config &config, bool automatic, uint32_t nowMs);
  void runRssFeedCheck(uint32_t nowMs);
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
  String firmwareVersionLabel() const;
  String themeModeLabel() const;
  String phantomWordsLabel() const;
  String focusHighlightLabel() const;
  String uiLanguageLabel() const;
  String readerModeLabel() const;
  String pauseModeLabel() const;
  String handednessLabel() const;
  String readerControlsLayoutLabel() const;
  String readerFontSizeLabel() const;
  String readerTypefaceLabel() const;
  String typographyTuningLabel() const;
  String typographyTuningValueLabel() const;
  String uiText(UiText key) const;
  void openBookPicker(bool articlesOnly = false);
  void selectBookPickerItem(uint32_t nowMs);
  void openChapterPicker();
  void selectChapterPickerItem(uint32_t nowMs);
  void openRestartConfirm();
  void selectRestartConfirmItem(uint32_t nowMs);
  void openSdCardRepairConfirm();
  void selectSdCardRepairConfirmItem(uint32_t nowMs);
  void runSdCardRepair(uint32_t nowMs);
  void runSdCardCheck(uint32_t nowMs);
  void openUpdateConfirm();
  void selectUpdateConfirmItem(uint32_t nowMs);
  void openPowerOffConfirm(uint32_t nowMs);
  void cancelPowerOffConfirm(uint32_t nowMs);
  void selectPowerOffConfirmItem(uint32_t nowMs);
  void enterCompanionSync(uint32_t nowMs);
  void updateCompanionSync(uint32_t nowMs);
  void exitCompanionSync(uint32_t nowMs);
  void enterUsbTransfer(uint32_t nowMs);
  void updateUsbTransfer(uint32_t nowMs);
  void exitUsbTransfer(uint32_t nowMs);
  void enterStandby(uint32_t nowMs);
  void exitStandby(uint32_t nowMs);
  void seedStandbyScreensaver(uint32_t nowMs);
  void stepStandbyScreensaver(uint32_t nowMs);
  uint32_t standbyRngSeed(uint32_t nowMs) const;
  void seedStandbyScreenOff(uint32_t nowMs);
  void updateStandbyScreensaver(uint32_t nowMs, bool force = false);
  void enterPowerOff(uint32_t nowMs);
  void enterSleep(uint32_t nowMs);
  void wakeFromSleep(bool fullPeripheralReset = false);
  bool restoreSavedBook(uint32_t nowMs);
  bool prepareBootBookLoad();
  void loadPendingBootBook(uint32_t nowMs);
  void saveReadingPosition(bool force = false);
  struct BookOpenOptions {
    BookOpenOptions()
        : allowLegacyPositionFallback(false),
          allowIndexBuild(true),
          allowEpubConversion(true),
          rebuildTimeEstimate(true) {}

    bool allowLegacyPositionFallback;
    bool allowIndexBuild;
    bool allowEpubConversion;
    bool rebuildTimeEstimate;
  };
  bool loadBookAtIndex(size_t index, uint32_t nowMs,
                       const BookOpenOptions &options = BookOpenOptions());
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
  void renderArticlesMenu();
  void renderSettings();
  void renderTypographyTuning();
  void renderBookPicker();
  void renderChapterPicker();
  void renderRestartConfirm();
  void renderSdCardRepairConfirm();
  void renderUpdateConfirm();
  void renderPowerOffConfirm();
  void renderQuickSettings();
  void renderQuickSync();
  void renderFocusTimerGenres();
  void renderFocusTimerSession();
  void renderActiveReader(uint32_t nowMs);
  bool updateChapterTransition(uint32_t nowMs);
  bool maybeStartChapterTransition(size_t previousWordIndex, size_t currentWordIndex,
                                   uint32_t nowMs);
  void renderChapterTransition();
  void renderScrollReader(uint32_t nowMs, const String &overlayText = "");
  DisplayManager::LibraryItem libraryItemForBook(size_t bookIndex);
  String chapterMenuLabel(size_t chapterIndex) const;
  size_t currentChapterIndex() const;
  String currentChapterLabel() const;
  String cleanedChapterTitle(const String &raw, const String &fallback) const;
  const char *chapterLabelPrefKey() const;
  static bool chapterLabelDefaultForMode(ReaderMode mode);
  String currentFooterMetricLabel() const;
  String currentBatteryLabel() const;
  String footerMetricModeLabel() const;
  String batteryLabelModeLabel() const;
  String screensaverModeLabel() const;
  String standbyTimerLabel() const;
  uint32_t standbyTimerMs() const;
  String batteryTimeRemainingLabel() const;
  String batteryVoltageLabel() const;
  String formatBatteryTimeRemaining(uint32_t minutes) const;
  uint32_t estimatedReadingTimeRemainingMs(size_t startIndex, size_t endIndex) const;
  uint32_t estimatedPacingBonusMs(size_t startIndex, size_t endIndex) const;
  void rebuildTimeEstimateCache();
  void invalidateTimeEstimateCache();
  void flushPendingTimeEstimateRebuild();
  void cancelTimeEstimateBuild();
  void updateTimeEstimateBuild(uint32_t nowMs);
  bool timeEstimateBuildMatchesCurrentBook() const;
  String formatReadingTimeRemaining(uint32_t remainingMs) const;
  String timeEstimateModeLabel() const;
  uint8_t readingProgressPercent() const;
  bool ensureCurrentBookWordAvailable(uint32_t nowMs);
  void handleCurrentBookReadFailure(uint32_t nowMs, const char *detail);
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
  void applyUiOrientation(Board::Config::UiOrientation orientation);
  void applyReaderUiOrientation();
  void reloadRuntimePreferences(uint32_t nowMs, bool rerender);
  Board::Config::UiOrientation readerUiOrientation() const;
  bool uiRotated180() const;
  uint8_t effectiveAnchorPercent() const;
  DisplayManager::TypographyConfig effectiveTypographyConfig() const;
  uint32_t currentReaderContentToken() const;
  String formatFocusTimerDuration(uint32_t durationMs) const;
  String formatFocusTimerRemaining(uint32_t nowMs) const;
  String focusTimerCountsLabel() const;
  void playFocusTimerCompletionCue();

  AppState state_ = AppState::Booting;
  AppState standbyReturnState_ = AppState::Paused;
  AppState powerOffConfirmReturnState_ = AppState::Paused;
  DisplayManager display_;
  FocusTimer focusTimer_;
  ReadingLoop reader_;
  Input::Buttons::Button button_;
  Input::Buttons::Button powerButton_;
  Input::Buttons::Button keyButton_;
  StorageManager storage_;
  IndexedBookStore activeBookStore_;
  OtaUpdater otaUpdater_;
  RssFeedManager rssFeedManager_;
  CompanionSyncManager companionSync_;
  UsbMassStorageManager usbTransfer_;
  Preferences preferences_;
  PausedTouchSession pausedTouch_;
  TouchIntent pausedTouchIntent_ = TouchIntent::None;

  uint32_t bootStartedMs_ = 0;
  uint32_t lastActivityMs_ = 0;
  uint32_t lastStateLogMs_ = 0;
  uint32_t powerButtonEventArmMs_ = 0;
  uint32_t wpmFeedbackUntilMs_ = 0;
  uint32_t brightnessToastUntilMs_ = 0;
  uint32_t lastProgressSaveMs_ = 0;
  uint32_t lastBatterySampleMs_ = 0;
  uint32_t batteryRuntimeAnchorMs_ = 0;
  uint32_t lastBatteryLabelRefreshMs_ = 0;
  uint32_t lastScrollAnimationRenderMs_ = 0;
  uint32_t lastCompanionSyncRenderMs_ = 0;
  uint32_t standbyEnteredMs_ = 0;
  uint32_t lastStandbyFrameMs_ = 0;
  uint32_t lastKeyButtonTapMs_ = 0;
  uint32_t chapterTransitionUntilMs_ = 0;
  uint32_t lastLowBatteryWarningMs_ = 0;
  uint32_t batteryWarningRestoreAtMs_ = 0;
  size_t lastSavedWordIndex_ = static_cast<size_t>(-1);
  size_t contextPreviewStartIndex_ = 0;
  size_t contextPreviewCurrentLocalIndex_ = static_cast<size_t>(-1);
  size_t currentBookIndex_ = 0;
  size_t pendingBootBookIndex_ = 0;
  size_t menuSelectedIndex_ = 0;
  size_t articlesSelectedIndex_ = 0;
  size_t settingsSelectedIndex_ = 0;
  size_t wifiNetworkSelectedIndex_ = 0;
  size_t bookPickerSelectedIndex_ = 0;
  size_t chapterPickerSelectedIndex_ = 0;
  size_t chapterTransitionIndex_ = static_cast<size_t>(-1);
  size_t restartConfirmSelectedIndex_ = 0;
  size_t sdCardRepairConfirmSelectedIndex_ = 0;
  size_t updateConfirmSelectedIndex_ = 0;
  size_t powerOffConfirmSelectedIndex_ = 0;
  size_t quickSettingsSelectedIndex_ = 0;
  size_t quickSyncSelectedIndex_ = 0;
  size_t focusTimerGenreSelectedIndex_ = 0;
  uint8_t standbyTimerIndex_ = 0;
  uint8_t brightnessLevelIndex_ = 4;
  uint8_t readerFontSizeIndex_ = 0;
  uint16_t menuRepeatDelayMs_ = MenuRepeat::kDefaultDelayMs;
  uint16_t pacingLongWordDelayMs_ = 200;
  uint16_t pacingComplexWordDelayMs_ = 200;
  uint16_t pacingPunctuationDelayMs_ = 200;
  size_t typographyTuningSelectedIndex_ = 1;
  size_t typographyPreviewSampleIndex_ = 0;
  MenuScreen menuScreen_ = MenuScreen::Main;
  MenuScreen restartConfirmReturnScreen_ = MenuScreen::Main;
  MenuScreen powerOffConfirmReturnScreen_ = MenuScreen::Main;
  QueueHandle_t otaCheckQueue_ = nullptr;
  std::vector<String> settingsMenuItems_;
  std::vector<String> focusTimerGenreMenuItems_;
  std::vector<DisplayManager::LibraryItem> wifiNetworkMenuItems_;
  std::vector<DisplayManager::LibraryItem> bookMenuItems_;
  std::vector<size_t> bookPickerBookIndices_;
  std::vector<String> chapterMenuItems_;
  std::vector<ChapterMarker> chapterMarkers_;
  std::vector<size_t> paragraphStarts_;
  std::vector<uint32_t> wordBonusBlockPrefixSumMs_;
  String timeEstimateBuildBookPath_;
  size_t timeEstimateBuildWordCount_ = 0;
  size_t timeEstimateBuildBlockCount_ = 0;
  size_t timeEstimateBuildNextBlock_ = 0;
  uint32_t timeEstimateBuildRunningMs_ = 0;
  uint32_t timeEstimateBuildStartedMs_ = 0;
  uint32_t timeEstimateBuildLastLogMs_ = 0;
  bool timeEstimateCacheValid_ = false;
  bool timeEstimateBuildInProgress_ = false;
  bool accurateTimeEstimateEnabled_ = true;
  bool pacingCacheDirty_ = false;
  std::vector<DisplayManager::ContextWord> contextPreviewWords_;
  std::vector<WifiNetworkInfo> wifiNetworks_;
  std::vector<TextEntryButton> textEntryButtons_;
  std::unique_ptr<standby::Screensaver> screensaver_;
  String currentBookPath_;
  String currentBookTitle_;
  String pendingUpdateCurrentVersion_;
  String pendingUpdateNewVersion_;
  String batteryLabel_;
  float batteryFilteredVoltage_ = 0.0f;
  float batteryFilteredPercent_ = 0.0f;
  uint8_t batteryDisplayedPercent_ = 0;
  uint8_t batteryRuntimeAnchorPercent_ = 0;
  uint32_t batteryRuntimeMinutesRemaining_ = 0;
  uint32_t cpuMhzPlay_ = 160;
  uint32_t cpuMhzScroll_ = 160;
  uint32_t cpuMhzPaused_ = 80;
  uint32_t cpuMhzMenu_ = 80;
  uint32_t cpuMhzStandby_ = 80;
  uint8_t autoDimBrightnessPercent_ = 10;
  uint32_t autoDimDelayMs_ = 60000;
  TextEntrySession textEntrySession_;
  bool touchInitialized_ = false;
  bool menuRepeatGestureConsumed_ = false;
  bool menuRepeatMoved_ = false;
  bool playLocked_ = false;
  bool pauseAtSentenceEndRequested_ = false;
  bool brightnessToastVisible_ = false;
  bool autoDimActive_ = false;
  bool bootButtonReleasedSinceBoot_ = false;
  bool bootButtonLongPressHandled_ = false;
  bool powerButtonReleasedSinceBoot_ = false;
  bool powerButtonLongPressHandled_ = false;
  bool keyButtonReleasedSinceBoot_ = false;
  bool keyButtonLongPressHandled_ = false;
  bool keyButtonTapArmed_ = false;
  bool bookPickerArticlesOnly_ = false;
  bool powerOffStarted_ = false;
  bool standbyButtonsReleased_ = false;
  bool standbyScreenOffActive_ = false;
  bool chapterTransitionVisible_ = false;
  bool batteryWarningOverlayVisible_ = false;
  bool focusTimerCancelHoldTriggered_ = false;
  bool otaCheckInProgress_ = false;
  bool otaUpdatePromptPending_ = false;
  bool contextViewVisible_ = false;
  bool contextPreviewWindowValid_ = false;
  bool wpmFeedbackVisible_ = false;
  bool usingStorageBook_ = false;
  bool storageReady_ = false;
  bool pendingBootBookLoad_ = false;
  int menuRepeatDirection_ = 0;
  uint32_t menuRepeatNextMs_ = 0;
  bool pendingBootBookLegacyFallback_ = false;
  bool batteryPresent_ = false;
  bool batterySampleInitialized_ = false;
  bool batteryRuntimeEstimateReady_ = false;
  uint8_t batteryCriticalSampleCount_ = 0;
  bool phantomWordsEnabled_ = true;
  bool readerBatteryVisibleWhilePlaying_ = true;
  bool readerChapterVisibleWhilePlaying_ = false;
  bool readerProgressVisibleWhilePlaying_ = false;
  bool readerControlsSwapped_ = false;
  bool chapterLabelEnabled_ = true;
  FooterMetricMode footerMetricMode_ = FooterMetricMode::Percentage;
  BatteryLabelMode batteryLabelMode_ = BatteryLabelMode::Percent;
  ScreensaverMode screensaverMode_ = ScreensaverMode::Life;
  PauseMode pauseMode_ = PauseMode::SentenceEnd;
  bool darkMode_ = true;
  bool nightMode_ = false;
  bool yellowModeEnabled_ = false;
  UiLanguage uiLanguage_ = UiLanguage::English;
  ReaderMode readerMode_ = ReaderMode::Rsvp;
  HandednessMode handednessMode_ = HandednessMode::Right;
  DisplayManager::TypographyConfig typographyConfig_;
};
