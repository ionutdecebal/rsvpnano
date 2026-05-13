#include "app/App.h"

#include <esp_sleep.h>
#include <esp_log.h>
#include <WiFi.h>
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <utility>
#include <vector>

#include "board/BoardConfig.h"

#ifndef RSVP_USB_TRANSFER_ENABLED
#define RSVP_USB_TRANSFER_ENABLED 0
#endif

#ifndef RSVP_USB_TRANSFER_AUTO_START
#define RSVP_USB_TRANSFER_AUTO_START 0
#endif

static const char *kAppTag = "app";
constexpr uint32_t kBootSplashMs = 750;
constexpr uint32_t kWpmFeedbackMs = 900;
constexpr uint32_t kPowerOffHoldMs = 1600;
constexpr uint32_t kPowerOffReleaseWaitMs = 4000;
constexpr uint32_t kBatterySampleIntervalMs = 180000;
constexpr uint32_t kTouchPlayHoldMs = 180;
constexpr uint32_t kPreviewBrowseHoldMs = 240;
constexpr uint32_t kReaderDoubleTapWindowMs = 320;
constexpr uint32_t kThemeToggleHoldMs = 900;
constexpr uint32_t kScrollAnimationFrameMs = 16;
constexpr uint16_t kSwipeThresholdPx = 40;
constexpr uint16_t kAxisBiasPx = 12;
constexpr uint16_t kTapSlopPx = 18;
constexpr uint16_t kReaderDoubleTapSlopPx = 36;
constexpr uint16_t kPreviousSentenceTapWidthPx = 96;
constexpr uint16_t kFooterMetricTapWidthPx = 220;
constexpr uint16_t kFooterMetricTapHeightPx = 32;
constexpr uint16_t kScrubStepPx = 22;
constexpr uint16_t kBrowseNeutralZonePx = 14;
constexpr uint16_t kFocusTimerCancelHoldMaxDriftPx = 20;
constexpr int kMaxScrubStepsPerGesture = 96;
constexpr uint32_t kBrowseMinWordsPerSecondPermille = 4000;
constexpr uint32_t kBrowseMaxWordsPerSecondPermille = 72000;
constexpr uint32_t kFocusTimerCancelHoldMs = 850;
constexpr size_t kContextPreviewWindowWords = 288;
constexpr size_t kContextPreviewAnchorLeadWords = 112;
constexpr size_t kContextPreviewMaxParagraphSnapWords = 48;
constexpr uint32_t kProgressSaveIntervalMs = 15000;
constexpr uint32_t kUsbTransferExitHoldMs = 1200;
constexpr uint8_t kBrightnessLevels[] = {40, 55, 70, 85, 100};
constexpr uint8_t kNightBrightnessLevels[] = {35, 40, 45, 50, 55};
constexpr size_t kBrightnessLevelCount = sizeof(kBrightnessLevels) / sizeof(kBrightnessLevels[0]);

namespace {

enum MenuItem : size_t {
  MenuResume,
  MenuChapters,
  MenuChangeBook,
  MenuFocusTimer,
  MenuSettings,
#if RSVP_USB_TRANSFER_ENABLED
  MenuUsbTransfer,
#endif
  MenuPowerOff,
  MenuItemCount,
};

enum SettingsItem : size_t {
  SettingsBack,
  SettingsDisplay,
  SettingsTypography,
  SettingsWordPacing,
  SettingsHandedness,
  SettingsBrightness,
  SettingsTheme,
  SettingsPhantomWords,
  SettingsFontSize,
  SettingsLongWords,
  SettingsComplexWords,
  SettingsPunctuation,
  SettingsReset,
  SettingsItemCount,
};

enum TypographyTuningItem : size_t {
  TypographyTuningBack,
  TypographyTuningFontSize,
  TypographyTuningTypeface,
  TypographyTuningPhantomWords,
  TypographyTuningFocusHighlight,
  TypographyTuningTracking,
  TypographyTuningAnchor,
  TypographyTuningGuideWidth,
  TypographyTuningGuideGap,
  TypographyTuningReset,
  TypographyTuningItemCount,
};

enum RestartConfirmItem : size_t {
  RestartConfirmNo,
  RestartConfirmYes,
  RestartConfirmItemCount,
};

constexpr size_t kRestartConfirmHeaderRows = 1;
constexpr size_t kSettingsBackIndex = 0;
constexpr size_t kSettingsHomeDisplayIndex = 1;
constexpr size_t kSettingsHomeTypographyIndex = 2;
constexpr size_t kSettingsHomePacingIndex = 3;
constexpr size_t kSettingsHomeWifiIndex = 4;
constexpr size_t kSettingsHomeUpdateIndex = 5;
constexpr size_t kSettingsDisplayReadingModeIndex = 1;
constexpr size_t kSettingsDisplayHandednessIndex = 2;
constexpr size_t kSettingsDisplayThemeIndex = 3;
constexpr size_t kSettingsDisplayBrightnessIndex = 4;
constexpr size_t kSettingsDisplayLanguageIndex = 5;
constexpr size_t kSettingsPacingLongWordsIndex = 1;
constexpr size_t kSettingsPacingComplexityIndex = 2;
constexpr size_t kSettingsPacingPunctuationIndex = 3;
constexpr size_t kSettingsPacingTimeEstimateIndex = 4;
constexpr size_t kSettingsPacingResetIndex = 5;
constexpr size_t kWifiSettingsNetworkIndex = 1;
constexpr size_t kWifiSettingsChooseIndex = 2;
constexpr size_t kWifiSettingsAutoUpdateIndex = 3;
constexpr size_t kWifiSettingsForgetIndex = 4;
constexpr size_t kWifiSettingsOtaOwnerIndex = 5;

constexpr size_t kBookPickerBackIndex = 0;
constexpr size_t kChapterPickerBackIndex = 0;
constexpr size_t kChapterPickerFallbackIndex = 1;
constexpr size_t kWifiNetworksBackIndex = 0;
constexpr size_t kWifiNetworksFirstItemIndex = 1;
constexpr size_t kFocusTimerGenreBackIndex = 0;
constexpr size_t kFocusTimerGenreFirstIndex = 1;
constexpr const char *kPrefsNamespace = "rsvp";
constexpr const char *kPrefBookPath = "book";
constexpr const char *kPrefLegacyWordIndex = "word";
constexpr const char *kPrefWpm = "wpm";
constexpr const char *kPrefBrightness = "bright";
constexpr const char *kPrefDarkMode = "dark";
constexpr const char *kPrefNightMode = "night";
constexpr const char *kPrefUiLanguage = "ui_lang";
constexpr const char *kPrefReaderMode = "read_mode";
constexpr const char *kPrefHandedness = "handed";
constexpr const char *kPrefPhantomWords = "phantom_on";
constexpr const char *kPrefFooterMetricMode = "prog_md";
constexpr const char *kPrefReaderFontSize = "font_size";
constexpr const char *kPrefReaderTypeface = "typeface";
constexpr const char *kPrefTypographyFocusHighlight = "type_hlt";
constexpr const char *kPrefLegacyPacingLong = "pace_len";
constexpr const char *kPrefLegacyPacingComplex = "pace_cpx";
constexpr const char *kPrefLegacyPacingPunctuation = "pace_pnc";
constexpr const char *kPrefPacingLongMs = "pace_lms";
constexpr const char *kPrefPacingComplexMs = "pace_cms";
constexpr const char *kPrefPacingPunctuationMs = "pace_pms";
constexpr const char *kPrefAccurateTime = "time_est_a";
constexpr const char *kPrefTypographyTracking = "type_trk";
constexpr const char *kPrefTypographyAnchor = "type_anc";
constexpr const char *kPrefTypographyGuideWidth = "type_wid";
constexpr const char *kPrefTypographyGuideGap = "type_gap";
constexpr const char *kPrefRecentSeq = "seq";
constexpr const char *kPrefWifiSsid = "wifi_ssid";
constexpr const char *kPrefWifiPass = "wifi_pass";
constexpr const char *kPrefOtaAuto = "ota_auto";
constexpr const char *kPrefOtaOwner = "ota_owner";
constexpr size_t kReaderFontSizeCount = 3;
constexpr size_t kPhantomBeforeCharTargets[] = {64, 96, 144};
constexpr size_t kPhantomAfterCharTargets[] = {96, 144, 208};
constexpr uint32_t kNoSavedWordIndex = 0xFFFFFFFFUL;
constexpr uint16_t kPacingDelayMinMs = 0;
constexpr uint16_t kPacingDelayMaxMs = 600;
constexpr uint16_t kPacingDelayStepMs = 50;
constexpr uint16_t kDefaultPacingDelayMs = 200;
constexpr int8_t kTypographyTrackingMin = -2;
constexpr int8_t kTypographyTrackingMax = 3;
constexpr uint8_t kTypographyAnchorMin = 30;
constexpr uint8_t kTypographyAnchorMax = 40;
constexpr uint8_t kLeftHandAnchorOffset = 20;
constexpr uint8_t kLeftHandAnchorMin = kTypographyAnchorMin + kLeftHandAnchorOffset;
constexpr uint8_t kLeftHandAnchorMax = kTypographyAnchorMax + kLeftHandAnchorOffset;
constexpr uint8_t kTypographyGuideWidthMin = 12;
constexpr uint8_t kTypographyGuideWidthMax = 30;
constexpr uint8_t kTypographyGuideWidthStep = 2;
constexpr uint8_t kTypographyGuideGapMin = 2;
constexpr uint8_t kTypographyGuideGapMax = 8;
constexpr const char *kTypographyPreviewWords[] = {
    "minimum",
    "encyclopaedia",
    "state-of-the-art",
    "HTTP/2",
    "well-known",
    "rhythms",
    "illumination",
    "WAVEFORM",
    "I",
};
constexpr size_t kTypographyPreviewWordCount =
    sizeof(kTypographyPreviewWords) / sizeof(kTypographyPreviewWords[0]);
constexpr size_t kWifiPasswordMaxLength = 63;
constexpr uint16_t kKeyboardMarginX = 8;
constexpr uint16_t kKeyboardTopY = 48;
constexpr uint16_t kKeyboardRowGap = 4;
constexpr uint16_t kKeyboardRowHeight = 27;

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

int clampIntSetting(int value, int minValue, int maxValue) {
  return std::max(minValue, std::min(maxValue, value));
}

int nextCyclicSetting(int value, int minValue, int maxValue, int step = 1) {
  step = std::max(1, step);
  const int normalized = clampIntSetting(value, minValue, maxValue);
  int next = normalized + step;
  if (next > maxValue) {
    next = minValue;
  }
  return next;
}

DisplayManager::TypographyConfig defaultTypographyConfig() {
  return DisplayManager::TypographyConfig();
}

bool wifiNetworkRequiresPassword(uint8_t authMode) {
  return static_cast<wifi_auth_mode_t>(authMode) != WIFI_AUTH_OPEN;
}

String wifiSecurityLabel(uint8_t authMode) {
  return wifiNetworkRequiresPassword(authMode) ? "Secure" : "Open";
}

String maskedValue(const String &value) {
  String masked;
  masked.reserve(value.length());
  for (size_t i = 0; i < value.length(); ++i) {
    masked += '*';
  }
  return masked;
}

const char *keyboardRowText(uint8_t modeValue, size_t rowIndex) {
  static constexpr const char *kLowerRows[] = {
      "qwertyuiop",
      "asdfghjkl",
      "zxcvbnm",
  };
  static constexpr const char *kUpperRows[] = {
      "QWERTYUIOP",
      "ASDFGHJKL",
      "ZXCVBNM",
  };
  static constexpr const char *kSymbolRows[] = {
      "1234567890",
      "!@#$%^&*?",
      "-_=+/:;.,",
  };

  if (rowIndex >= 3) {
    return "";
  }

  switch (modeValue) {
    case 1:
      return kUpperRows[rowIndex];
    case 2:
      return kSymbolRows[rowIndex];
    default:
      return kLowerRows[rowIndex];
  }
}

String storedOrFallbackLabel(const String &value, const String &fallback) {
  return value.isEmpty() ? fallback : value;
}

DisplayManager::ReaderTypeface readerTypefaceFromSetting(uint8_t value) {
  switch (static_cast<DisplayManager::ReaderTypeface>(value)) {
    case DisplayManager::ReaderTypeface::Standard:
    case DisplayManager::ReaderTypeface::OpenDyslexic:
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible:
      return static_cast<DisplayManager::ReaderTypeface>(value);
  }
  return DisplayManager::ReaderTypeface::Standard;
}

DisplayManager::ReaderTypeface nextReaderTypeface(DisplayManager::ReaderTypeface current) {
  switch (readerTypefaceFromSetting(static_cast<uint8_t>(current))) {
    case DisplayManager::ReaderTypeface::Standard:
      return DisplayManager::ReaderTypeface::AtkinsonHyperlegible;
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible:
      return DisplayManager::ReaderTypeface::OpenDyslexic;
    case DisplayManager::ReaderTypeface::OpenDyslexic:
    default:
      return DisplayManager::ReaderTypeface::Standard;
  }
}

App::ReaderMode readerModeFromSetting(uint8_t value) {
  switch (value) {
    case static_cast<uint8_t>(App::ReaderMode::Scroll):
    case 2:  // Migrate the removed word-scroll mode to page scroll.
      return App::ReaderMode::Scroll;
    case static_cast<uint8_t>(App::ReaderMode::Rsvp):
    default:
      return App::ReaderMode::Rsvp;
  }
}

App::HandednessMode handednessModeFromSetting(uint8_t value) {
  switch (value) {
    case static_cast<uint8_t>(App::HandednessMode::Left):
      return App::HandednessMode::Left;
    case static_cast<uint8_t>(App::HandednessMode::Right):
    default:
      return App::HandednessMode::Right;
  }
}

App::HandednessMode nextHandednessMode(App::HandednessMode current) {
  switch (handednessModeFromSetting(static_cast<uint8_t>(current))) {
    case App::HandednessMode::Left:
      return App::HandednessMode::Right;
    case App::HandednessMode::Right:
    default:
      return App::HandednessMode::Left;
  }
}

App::ReaderMode nextReaderMode(App::ReaderMode current) {
  switch (readerModeFromSetting(static_cast<uint8_t>(current))) {
    case App::ReaderMode::Rsvp:
      return App::ReaderMode::Scroll;
    case App::ReaderMode::Scroll:
    default:
      return App::ReaderMode::Rsvp;
  }
}

uint16_t pacingDelayMsForLegacyLevel(uint8_t levelIndex) {
  constexpr uint16_t kLegacyPacingDelayMs[] = {100, 150, 200, 250, 300};
  constexpr size_t kLegacyPacingLevelCount =
      sizeof(kLegacyPacingDelayMs) / sizeof(kLegacyPacingDelayMs[0]);

  if (levelIndex >= kLegacyPacingLevelCount) {
    levelIndex = 2;
  }
  return kLegacyPacingDelayMs[levelIndex];
}

uint16_t loadPacingDelayMs(Preferences &preferences, const char *key, const char *legacyKey) {
  if (preferences.isKey(key)) {
    return static_cast<uint16_t>(
        clampIntSetting(preferences.getUShort(key, kDefaultPacingDelayMs), kPacingDelayMinMs,
                        kPacingDelayMaxMs));
  }

  if (preferences.isKey(legacyKey)) {
    const uint16_t migratedDelayMs =
        pacingDelayMsForLegacyLevel(preferences.getUChar(legacyKey, 2));
    preferences.putUShort(key, migratedDelayMs);
    return migratedDelayMs;
  }

  return kDefaultPacingDelayMs;
}

}  // namespace

App::App() : button_(BoardConfig::PIN_BOOT_BUTTON), powerButton_(BoardConfig::PIN_PWR_BUTTON) {}

void App::begin() {
  BoardConfig::begin();
  button_.begin();
  powerButton_.begin();
  bootButtonReleasedSinceBoot_ = !button_.isHeld();
  bootButtonLongPressHandled_ = false;
  powerButtonReleasedSinceBoot_ = !powerButton_.isHeld();
  powerButtonLongPressHandled_ = false;
  storage_.setStatusCallback(&App::handleStorageStatus, this);
  preferences_.begin(kPrefsNamespace, false);
  brightnessLevelIndex_ = preferences_.getUChar(kPrefBrightness, brightnessLevelIndex_);
  if (brightnessLevelIndex_ >= kBrightnessLevelCount) {
    brightnessLevelIndex_ = kBrightnessLevelCount - 1;
  }
  phantomWordsEnabled_ = preferences_.getBool(kPrefPhantomWords, phantomWordsEnabled_);
  uiLanguage_ =
      Localization::sanitizeLanguage(preferences_.getUChar(
          kPrefUiLanguage, static_cast<uint8_t>(uiLanguage_)));
  readerMode_ = readerModeFromSetting(
      preferences_.getUChar(kPrefReaderMode, static_cast<uint8_t>(readerMode_)));
  handednessMode_ = handednessModeFromSetting(
      preferences_.getUChar(kPrefHandedness, static_cast<uint8_t>(handednessMode_)));
  readerFontSizeIndex_ = preferences_.getUChar(kPrefReaderFontSize, readerFontSizeIndex_);
  if (readerFontSizeIndex_ >= kReaderFontSizeCount) {
    readerFontSizeIndex_ = 0;
  }
  switch (preferences_.getUChar(kPrefFooterMetricMode,
                                static_cast<uint8_t>(footerMetricMode_))) {
    case static_cast<uint8_t>(FooterMetricMode::ChapterTime):
      footerMetricMode_ = FooterMetricMode::ChapterTime;
      break;
    case static_cast<uint8_t>(FooterMetricMode::BookTime):
      footerMetricMode_ = FooterMetricMode::BookTime;
      break;
    case static_cast<uint8_t>(FooterMetricMode::Percentage):
    default:
      footerMetricMode_ = FooterMetricMode::Percentage;
      break;
  }
  pacingLongWordDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingLongMs, kPrefLegacyPacingLong);
  pacingComplexWordDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingComplexMs, kPrefLegacyPacingComplex);
  pacingPunctuationDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingPunctuationMs, kPrefLegacyPacingPunctuation);
  accurateTimeEstimateEnabled_ = preferences_.getBool(kPrefAccurateTime, true);
  typographyConfig_ = defaultTypographyConfig();
  typographyConfig_.typeface = readerTypefaceFromSetting(
      preferences_.getUChar(kPrefReaderTypeface, static_cast<uint8_t>(typographyConfig_.typeface)));
  typographyConfig_.focusHighlight =
      preferences_.getBool(kPrefTypographyFocusHighlight, typographyConfig_.focusHighlight);
  typographyConfig_.trackingPx = static_cast<int8_t>(clampIntSetting(
      preferences_.getChar(kPrefTypographyTracking, typographyConfig_.trackingPx),
      kTypographyTrackingMin, kTypographyTrackingMax));
  typographyConfig_.anchorPercent = static_cast<uint8_t>(clampIntSetting(
      preferences_.getUChar(kPrefTypographyAnchor, typographyConfig_.anchorPercent),
      kTypographyAnchorMin, kTypographyAnchorMax));
  typographyConfig_.guideHalfWidth = static_cast<uint8_t>(clampIntSetting(
      preferences_.getUChar(kPrefTypographyGuideWidth, typographyConfig_.guideHalfWidth),
      kTypographyGuideWidthMin, kTypographyGuideWidthMax));
  typographyConfig_.guideGap = static_cast<uint8_t>(clampIntSetting(
      preferences_.getUChar(kPrefTypographyGuideGap, typographyConfig_.guideGap),
      kTypographyGuideGapMin, kTypographyGuideGapMax));
  darkMode_ = preferences_.getBool(kPrefDarkMode, darkMode_);
  nightMode_ = preferences_.getBool(kPrefNightMode, nightMode_);
  applyHandednessSettings(0, false);
  applyDisplayPreferences(0, false);
  applyTypographySettings(0, false);
  applyPacingSettings();
  bootStartedMs_ = millis();
  lastStateLogMs_ = bootStartedMs_;
  lastScrollAnimationRenderMs_ = 0;
  Serial.printf("[app] version=%s\n", otaUpdater_.currentVersion().c_str());

  logApp("Initializing hardware modules");
  const bool displayReady = display_.begin();
  updateBatteryStatus(bootStartedMs_, true);

  if (displayReady) {
    display_.renderCenteredWord("READY");
    logApp("Display init ok");
  } else {
    ESP_LOGE(kAppTag, "Display init failed");
    Serial.println("[app] Display init failed");
  }

  touchInitialized_ = touch_.begin();
  audio_.begin();
  focusTimer_.begin();

#if RSVP_USB_TRANSFER_ENABLED && RSVP_USB_TRANSFER_AUTO_START
  state_ = AppState::Booting;
  Serial.println("[app] USB transfer auto-start active");
  enterUsbTransfer(millis());
  return;
#endif

  display_.renderProgress("SD", "Loading books", "Use SD converter for EPUB", 0);
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
    paragraphStarts_.clear();
    currentBookPath_ = "";
    currentBookTitle_ = "Demo";
    reader_.begin(bootStartedMs_);
    invalidateContextPreviewWindow();
    rebuildTimeEstimateCache();
    Serial.println("[app] using built-in demo text");
  }

  maybeAutoCheckForUpdates(bootStartedMs_);
  Serial.printf("[app] WPM=%u interval=%lu ms\n", reader_.wpm(),
                static_cast<unsigned long>(reader_.wordIntervalMs()));

  state_ = AppState::Booting;
  Serial.println("[app] READY splash active");
}

void App::update(uint32_t nowMs) {
  button_.update(nowMs);
  powerButton_.update(nowMs);
  handleBootButton(nowMs);
  handlePowerButton(nowMs);
  if (powerOffStarted_) {
    return;
  }

  const bool batteryChanged = updateBatteryStatus(nowMs);
  updateState(nowMs);
  updateFocusTimer(nowMs);
  updateReader(nowMs);
  handleTouch(nowMs);
  updateWpmFeedback(nowMs);
  maybeSaveReadingPosition(nowMs);

  if (batteryChanged && (state_ == AppState::Paused || state_ == AppState::Playing)) {
    renderActiveReader(nowMs);
  } else if (batteryChanged && state_ == AppState::Menu) {
    renderMenu();
  }

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
  if (previousState == AppState::Menu && nextState != AppState::Menu) {
    flushPendingTimeEstimateRebuild();
  }

  if (nextState != AppState::Paused) {
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
    contextViewVisible_ = false;
    invalidateContextPreviewWindow();
    wpmFeedbackVisible_ = false;
  }
  if (nextState != AppState::Playing) {
    touchPlayHeld_ = false;
    playLocked_ = false;
    pauseAtSentenceEndRequested_ = false;
  }
  if (nextState != AppState::Paused && nextState != AppState::Playing) {
    resetReaderTapTracking();
  }

  state_ = nextState;

  switch (state_) {
    case AppState::Paused:
      renderActiveReader(nowMs);
      break;
    case AppState::Playing:
      reader_.start(nowMs);
      renderActiveReader(nowMs);
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

    setState((touchPlayHeld_ || playLocked_ || pauseAtSentenceEndRequested_) ? AppState::Playing
                                                                              : AppState::Paused,
             nowMs);
    return;
  }

  if (state_ == AppState::UsbTransfer) {
    updateUsbTransfer(nowMs);
    return;
  }

  if (state_ == AppState::Menu || state_ == AppState::Sleeping) {
    // Menu and sleeping state changes are driven by direct input and power events.
    return;
  }

  if (touchPlayHeld_ || playLocked_ || pauseAtSentenceEndRequested_) {
    setState(AppState::Playing, nowMs);
    return;
  }

  setState(AppState::Paused, nowMs);
}

void App::updateReader(uint32_t nowMs) {
  if (state_ != AppState::Playing) {
    return;
  }

  if (shouldFinalizeReaderPause(nowMs)) {
    finalizeReaderPause(nowMs);
    return;
  }

  const bool changed = reader_.update(nowMs, !pauseAtSentenceEndRequested_);
  if (scrollModeEnabled()) {
    if (changed || nowMs - lastScrollAnimationRenderMs_ >= kScrollAnimationFrameMs) {
      renderScrollReader(nowMs);
      lastScrollAnimationRenderMs_ = nowMs;
    }
    return;
  }

  if (changed) {
    renderReaderWord();
  }
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

void App::handleBootButton(uint32_t nowMs) {
  if (state_ == AppState::UsbTransfer || state_ == AppState::Sleeping || powerOffStarted_) {
    return;
  }

  if (!bootButtonReleasedSinceBoot_) {
    if (!button_.isHeld()) {
      bootButtonReleasedSinceBoot_ = true;
    }
    return;
  }

  if (button_.isHeld() && !bootButtonLongPressHandled_ &&
      button_.heldDurationMs(nowMs) >= kThemeToggleHoldMs) {
    bootButtonLongPressHandled_ = true;
    cycleThemeMode(nowMs);
    return;
  }

  if (!button_.wasReleasedEvent()) {
    return;
  }

  if (bootButtonLongPressHandled_) {
    bootButtonLongPressHandled_ = false;
    return;
  }

  if (button_.lastHoldDurationMs() < kThemeToggleHoldMs) {
    cycleBrightness();
  }
}

void App::handlePowerButton(uint32_t nowMs) {
  if (!powerButtonReleasedSinceBoot_) {
    if (!powerButton_.isHeld()) {
      powerButtonReleasedSinceBoot_ = true;
    }
    return;
  }

  if (state_ == AppState::UsbTransfer || powerOffStarted_) {
    return;
  }

  if (powerButton_.isHeld() && nowMs - powerButton_.lastEdgeMs() >= kPowerOffHoldMs) {
    powerButtonLongPressHandled_ = true;
    enterPowerOff(nowMs);
    return;
  }

  if (!powerButton_.wasReleasedEvent()) {
    return;
  }

  if (powerButtonLongPressHandled_) {
    powerButtonLongPressHandled_ = false;
    return;
  }

  toggleMenuFromPowerButton(nowMs);
}

void App::toggleMenuFromPowerButton(uint32_t nowMs) {
  if (state_ == AppState::Booting || state_ == AppState::UsbTransfer ||
      state_ == AppState::Sleeping) {
    return;
  }

  if (state_ == AppState::Menu) {
    if (menuScreen_ == MenuScreen::Main) {
      setState(AppState::Paused, nowMs);
    } else {
      if (isFocusTimerMenuScreen(menuScreen_)) {
        resetFocusTimer();
      }
      menuScreen_ = MenuScreen::Main;
      renderMainMenu();
    }
    return;
  }

  openMainMenu(nowMs);
}

void App::openMainMenu(uint32_t nowMs) {
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  touchPlayHeld_ = false;
  menuScreen_ = MenuScreen::Main;
  menuSelectedIndex_ = MenuResume;
  wpmFeedbackVisible_ = false;
  contextViewVisible_ = false;
  if (state_ == AppState::Playing) {
    saveReadingPosition(true);
  }
  setState(AppState::Menu, nowMs);
}

uint8_t App::currentBrightnessPercent() const {
  return nightMode_ ? kNightBrightnessLevels[brightnessLevelIndex_]
                    : kBrightnessLevels[brightnessLevelIndex_];
}

void App::applyDisplayPreferences(uint32_t nowMs, bool rerender) {
  display_.setDarkMode(darkMode_);
  display_.setNightMode(nightMode_);
  display_.setBrightnessPercent(currentBrightnessPercent());

  if (!rerender) {
    return;
  }

  if (state_ == AppState::Menu) {
    if (menuScreen_ == MenuScreen::SettingsHome || menuScreen_ == MenuScreen::SettingsDisplay ||
        menuScreen_ == MenuScreen::SettingsPacing || menuScreen_ == MenuScreen::WifiSettings) {
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    }
    renderMenu();
    return;
  }

  if (state_ == AppState::Paused || state_ == AppState::Playing) {
    renderActiveReader(nowMs);
    return;
  }

  if (state_ == AppState::Booting) {
    display_.renderCenteredWord("READY");
  }
}

void App::applyHandednessSettings(uint32_t nowMs, bool rerender) {
  (void)nowMs;
  applyReaderUiOrientation();

  if (!rerender) {
    return;
  }

  if (state_ == AppState::Menu &&
      (menuScreen_ == MenuScreen::SettingsHome || menuScreen_ == MenuScreen::SettingsDisplay ||
       menuScreen_ == MenuScreen::SettingsPacing || menuScreen_ == MenuScreen::WifiSettings)) {
    rebuildSettingsMenuItems();
  }

  applyTypographySettings(nowMs);
}

void App::applyTypographySettings(uint32_t nowMs, bool rerender) {
  display_.setTypographyConfig(effectiveTypographyConfig());

  Serial.printf("[typography] face=%s highlight=%s track=%d anchor=%u guideWidth=%u guideGap=%u\n",
                readerTypefaceLabel().c_str(),
                focusHighlightLabel().c_str(),
                static_cast<int>(typographyConfig_.trackingPx),
                static_cast<unsigned int>(effectiveAnchorPercent()),
                static_cast<unsigned int>(typographyConfig_.guideHalfWidth),
                static_cast<unsigned int>(typographyConfig_.guideGap));

  if (!rerender) {
    return;
  }

  if (state_ == AppState::Menu) {
    renderMenu();
    return;
  }

  if (state_ == AppState::Paused || state_ == AppState::Playing) {
    renderActiveReader(nowMs);
  }
}

void App::cycleBrightness() {
  brightnessLevelIndex_ = static_cast<uint8_t>((brightnessLevelIndex_ + 1) % kBrightnessLevelCount);
  preferences_.putUChar(kPrefBrightness, brightnessLevelIndex_);
  const uint8_t percent = currentBrightnessPercent();
  Serial.printf("[display] brightness level %u/%u (%u%%)\n",
                static_cast<unsigned int>(brightnessLevelIndex_ + 1),
                static_cast<unsigned int>(kBrightnessLevelCount),
                static_cast<unsigned int>(percent));
  applyDisplayPreferences(millis());
}

void App::cycleThemeMode(uint32_t nowMs) {
  if (nightMode_) {
    nightMode_ = false;
    darkMode_ = true;
  } else if (darkMode_) {
    darkMode_ = false;
  } else {
    darkMode_ = true;
    nightMode_ = true;
  }

  preferences_.putBool(kPrefDarkMode, darkMode_);
  preferences_.putBool(kPrefNightMode, nightMode_);
  Serial.printf("[display] theme=%s\n", themeModeLabel().c_str());
  applyDisplayPreferences(nowMs);
}

void App::cycleUiLanguage(uint32_t nowMs) {
  uiLanguage_ = Localization::nextLanguage(uiLanguage_);
  preferences_.putUChar(kPrefUiLanguage, static_cast<uint8_t>(uiLanguage_));
  Serial.printf("[display] language=%s\n", uiLanguageLabel().c_str());

  if (state_ == AppState::Menu) {
    if (menuScreen_ == MenuScreen::SettingsHome || menuScreen_ == MenuScreen::SettingsDisplay ||
        menuScreen_ == MenuScreen::SettingsPacing || menuScreen_ == MenuScreen::WifiSettings) {
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    }
    renderMenu();
    return;
  }

  if (state_ == AppState::Paused || state_ == AppState::Playing) {
    renderActiveReader(nowMs);
  }
}

void App::cycleReaderMode(uint32_t nowMs) {
  readerMode_ = nextReaderMode(readerMode_);
  preferences_.putUChar(kPrefReaderMode, static_cast<uint8_t>(readerMode_));
  Serial.printf("[display] reader mode=%s\n", readerModeLabel().c_str());
  invalidateContextPreviewWindow();

  if (state_ == AppState::Menu) {
    rebuildSettingsMenuItems();
    renderSettings();
    return;
  }

  renderActiveReader(nowMs);
}

void App::cycleHandednessMode(uint32_t nowMs) {
  handednessMode_ = nextHandednessMode(handednessMode_);
  preferences_.putUChar(kPrefHandedness, static_cast<uint8_t>(handednessMode_));
  Serial.printf("[display] handedness=%s rotation180=%u\n", handednessLabel().c_str(),
                uiRotated180() ? 1U : 0U);
  applyHandednessSettings(nowMs);
}

void App::togglePhantomWords(uint32_t nowMs) {
  phantomWordsEnabled_ = !phantomWordsEnabled_;
  preferences_.putBool(kPrefPhantomWords, phantomWordsEnabled_);
  Serial.printf("[display] phantom words=%s\n", phantomWordsLabel().c_str());
  applyDisplayPreferences(nowMs);
}

void App::cycleReaderFontSize(uint32_t nowMs) {
  readerFontSizeIndex_ = static_cast<uint8_t>((readerFontSizeIndex_ + 1) % kReaderFontSizeCount);
  preferences_.putUChar(kPrefReaderFontSize, readerFontSizeIndex_);
  Serial.printf("[display] font size=%s\n", readerFontSizeLabel().c_str());
  applyDisplayPreferences(nowMs);
}

bool App::updateBatteryStatus(uint32_t nowMs, bool force) {
  // Battery sampling toggles shared board hardware; avoid doing that during active reading.
  if (!force && state_ == AppState::Playing) {
    return false;
  }

  if (!force && nowMs - lastBatterySampleMs_ < kBatterySampleIntervalMs) {
    return false;
  }

  lastBatterySampleMs_ = nowMs;

  BoardConfig::BatteryStatus status;
  String nextLabel;
  if (BoardConfig::readBatteryStatus(status)) {
    nextLabel = String(status.percent) + "%";
  } else {
    nextLabel = "";
  }

  if (nextLabel == batteryLabel_) {
    return false;
  }

  batteryLabel_ = nextLabel;
  display_.setBatteryLabel(batteryLabel_);
  if (!batteryLabel_.isEmpty()) {
    Serial.printf("[power] battery %.2f V (%u%%)\n", status.voltage,
                  static_cast<unsigned int>(status.percent));
  } else {
    Serial.println("[power] battery not detected");
  }
  return true;
}

void App::updateWpmFeedback(uint32_t nowMs) {
  if (!wpmFeedbackVisible_ || state_ != AppState::Paused) {
    return;
  }

  if (nowMs < wpmFeedbackUntilMs_) {
    return;
  }

  wpmFeedbackVisible_ = false;
  renderActiveReader(nowMs);
}

void App::resetReaderTapTracking() { lastReaderTapValid_ = false; }

bool App::isFooterMetricTap(uint16_t x, uint16_t y) const {
  return x >= BoardConfig::DISPLAY_WIDTH - kFooterMetricTapWidthPx &&
         y >= BoardConfig::DISPLAY_HEIGHT - kFooterMetricTapHeightPx;
}

bool App::isPreviousSentenceTap(uint16_t x) const { return x < kPreviousSentenceTapWidthPx; }

bool App::readerFooterVisible() const {
  return scrollModeEnabled() || state_ != AppState::Playing || contextViewVisible_ ||
         wpmFeedbackVisible_;
}

void App::rewindReaderSentence(uint32_t nowMs) {
  resetReaderTapTracking();
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;
  reader_.rewindSentence();

  if (state_ == AppState::Playing) {
    setState(AppState::Paused, nowMs);
  } else {
    renderActiveReader(nowMs);
    saveReadingPosition(true);
  }

  Serial.printf("[app] sentence rewind index=%u word=%s\n",
                static_cast<unsigned int>(reader_.currentIndex()), reader_.currentWord().c_str());
}

bool App::handleFooterMetricTap(uint16_t x, uint16_t y, uint32_t nowMs) {
  if (!readerFooterVisible() || !isFooterMetricTap(x, y)) {
    return false;
  }

  switch (footerMetricMode_) {
    case FooterMetricMode::Percentage:
      footerMetricMode_ = FooterMetricMode::ChapterTime;
      break;
    case FooterMetricMode::ChapterTime:
      footerMetricMode_ = FooterMetricMode::BookTime;
      break;
    case FooterMetricMode::BookTime:
    default:
      footerMetricMode_ = FooterMetricMode::Percentage;
      break;
  }

  preferences_.putUChar(kPrefFooterMetricMode, static_cast<uint8_t>(footerMetricMode_));
  resetReaderTapTracking();
  renderActiveReader(nowMs);
  const char *modeName = "percent";
  switch (footerMetricMode_) {
    case FooterMetricMode::ChapterTime:
      modeName = "chapter";
      break;
    case FooterMetricMode::BookTime:
      modeName = "book";
      break;
    case FooterMetricMode::Percentage:
    default:
      modeName = "percent";
      break;
  }
  Serial.printf("[reader] footer metric=%s\n", modeName);
  return true;
}

void App::handleReaderTap(uint16_t x, uint16_t y, uint32_t nowMs) {
  if (lastReaderTapValid_ && nowMs - lastReaderTapMs_ <= kReaderDoubleTapWindowMs &&
      abs(static_cast<int>(x) - static_cast<int>(lastReaderTapX_)) <=
          static_cast<int>(kReaderDoubleTapSlopPx) &&
      abs(static_cast<int>(y) - static_cast<int>(lastReaderTapY_)) <=
          static_cast<int>(kReaderDoubleTapSlopPx)) {
    resetReaderTapTracking();

    if (state_ == AppState::Playing) {
      requestReaderPauseAtSentenceEnd(nowMs);
    } else if (state_ == AppState::Paused) {
      playLocked_ = true;
      pauseAtSentenceEndRequested_ = false;
      wpmFeedbackVisible_ = false;
      setState(AppState::Playing, nowMs);
    }
    return;
  }

  lastReaderTapValid_ = true;
  lastReaderTapMs_ = nowMs;
  lastReaderTapX_ = x;
  lastReaderTapY_ = y;
}

void App::requestReaderPauseAtSentenceEnd(uint32_t nowMs) {
  if (state_ != AppState::Playing) {
    return;
  }

  playLocked_ = false;
  touchPlayHeld_ = false;
  if (!pauseAtSentenceEndRequested_) {
    pauseAtSentenceEndRequested_ = true;
    Serial.println("[app] pause requested at sentence end");
  }

  if (shouldFinalizeReaderPause(nowMs)) {
    finalizeReaderPause(nowMs);
  }
}

bool App::shouldFinalizeReaderPause(uint32_t nowMs) const {
  if (state_ != AppState::Playing || !pauseAtSentenceEndRequested_) {
    return false;
  }

  const uint32_t durationMs = reader_.currentWordDurationMs();
  if (durationMs == 0 || reader_.elapsedInCurrentWordMs(nowMs) < durationMs) {
    return false;
  }

  return reader_.currentWordEndsSentence() || reader_.atEnd();
}

void App::finalizeReaderPause(uint32_t nowMs) {
  pauseAtSentenceEndRequested_ = false;
  playLocked_ = false;
  touchPlayHeld_ = false;
  setState(AppState::Paused, nowMs);
}

void App::handleTouch(uint32_t nowMs) {
  if (!touchInitialized_) {
    return;
  }

  if (state_ == AppState::Booting || state_ == AppState::UsbTransfer ||
      state_ == AppState::Sleeping) {
    touch_.cancel();
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
    touchPlayHeld_ = false;
    resetReaderTapTracking();
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
    if (menuScreen_ == MenuScreen::FocusTimerSession) {
      applyFocusTimerTouch(ev, nowMs);
    } else {
      applyMenuTouchGesture(ev, nowMs);
    }
  } else {
    applyPausedTouchGesture(ev, nowMs);
  }
}

void App::applyPausedTouchGesture(const TouchEvent &event, uint32_t nowMs) {
  if (event.phase == TouchPhase::End && touchPlayHeld_) {
    resetReaderTapTracking();
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
    requestReaderPauseAtSentenceEnd(nowMs);
    return;
  }

  if (event.phase == TouchPhase::Start) {
    pausedTouch_.active = true;
    pausedTouchIntent_ = TouchIntent::None;
    if (state_ != AppState::Playing) {
      invalidateContextPreviewWindow();
    }
    pausedTouch_.startX = event.x;
    pausedTouch_.startY = event.y;
    pausedTouch_.lastX = event.x;
    pausedTouch_.lastY = event.y;
    pausedTouch_.startMs = nowMs;
    pausedTouch_.lastMs = nowMs;
    pausedTouch_.startWordIndex = reader_.currentIndex();
    pausedTouch_.gestureStepsApplied = 0;
    pausedTouch_.browseOffsetPermille = 0;
    return;
  }

  if (!pausedTouch_.active) {
    return;
  }

  const uint32_t elapsedSinceLastEventMs = nowMs - pausedTouch_.lastMs;
  pausedTouch_.lastX = event.x;
  pausedTouch_.lastY = event.y;
  pausedTouch_.lastMs = nowMs;

  const int deltaX = static_cast<int>(pausedTouch_.lastX) - static_cast<int>(pausedTouch_.startX);
  const int deltaY = static_cast<int>(pausedTouch_.lastY) - static_cast<int>(pausedTouch_.startY);
  const int absDeltaX = abs(deltaX);
  const int absDeltaY = abs(deltaY);
  const uint32_t pressDurationMs = nowMs - pausedTouch_.startMs;
  const bool ended = event.phase == TouchPhase::End;
  const bool tapLike = absDeltaX <= static_cast<int>(kTapSlopPx) &&
                       absDeltaY <= static_cast<int>(kTapSlopPx);
  const bool previewBrowseMode = contextViewVisible_ && !scrollModeEnabled();

  if (state_ == AppState::Playing) {
    if (ended) {
      pausedTouch_.active = false;
      pausedTouchIntent_ = TouchIntent::None;
      if (tapLike) {
        if (handleFooterMetricTap(event.x, event.y, nowMs)) {
          return;
        }
        if (isPreviousSentenceTap(event.x)) {
          rewindReaderSentence(nowMs);
          return;
        }
        if (playLocked_ || pauseAtSentenceEndRequested_) {
          resetReaderTapTracking();
          requestReaderPauseAtSentenceEnd(nowMs);
        } else {
          handleReaderTap(event.x, event.y, nowMs);
        }
      } else {
        resetReaderTapTracking();
      }
    }
    return;
  }

  if (!previewBrowseMode && !ended && pausedTouchIntent_ == TouchIntent::None &&
      pressDurationMs >= kTouchPlayHoldMs && tapLike) {
    resetReaderTapTracking();
    touchPlayHeld_ = true;
    pausedTouchIntent_ = TouchIntent::PlayHold;
    wpmFeedbackVisible_ = false;
    setState(AppState::Playing, nowMs);
    return;
  }

  if (pausedTouchIntent_ == TouchIntent::None) {
    if (absDeltaX >= static_cast<int>(kSwipeThresholdPx) &&
        absDeltaX > absDeltaY + static_cast<int>(kAxisBiasPx)) {
      resetReaderTapTracking();
      pausedTouchIntent_ = TouchIntent::Scrub;
    } else if (previewBrowseMode && !ended && pressDurationMs >= kPreviewBrowseHoldMs &&
               absDeltaY > absDeltaX + static_cast<int>(kAxisBiasPx)) {
      resetReaderTapTracking();
      pausedTouchIntent_ = TouchIntent::BrowseScroll;
    } else if (!previewBrowseMode && absDeltaY >= static_cast<int>(kSwipeThresholdPx) &&
               absDeltaY > absDeltaX + static_cast<int>(kAxisBiasPx)) {
      resetReaderTapTracking();
      pausedTouchIntent_ = TouchIntent::Wpm;
    }
  }

  if (pausedTouchIntent_ == TouchIntent::Scrub) {
    applyScrubTarget(scrubStepsForDrag(deltaX), nowMs);
    if (ended) {
      pausedTouch_.active = false;
      pausedTouchIntent_ = TouchIntent::None;
      saveReadingPosition(true);
    }
    return;
  }

  if (pausedTouchIntent_ == TouchIntent::BrowseScroll) {
    applyBrowseHoldScroll(event.y, elapsedSinceLastEventMs, nowMs);
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
    if (tapLike && handleFooterMetricTap(event.x, event.y, nowMs)) {
      return;
    }
    if (tapLike && !previewBrowseMode && isPreviousSentenceTap(event.x)) {
      rewindReaderSentence(nowMs);
      return;
    }
    if (tapLike && previewBrowseMode) {
      resetReaderTapTracking();
      contextViewVisible_ = false;
      renderActiveReader(nowMs);
    } else if (tapLike) {
      handleReaderTap(event.x, event.y, nowMs);
    } else {
      resetReaderTapTracking();
    }
  }
}

int App::scrubStepsForDrag(int deltaX) const {
  const int absDeltaX = abs(deltaX);
  if (absDeltaX < static_cast<int>(kSwipeThresholdPx)) {
    return 0;
  }

  int steps = 1 + ((absDeltaX - static_cast<int>(kSwipeThresholdPx)) /
                   static_cast<int>(kScrubStepPx));
  steps = std::min(steps, kMaxScrubStepsPerGesture);

  return (deltaX > 0) ? steps : -steps;
}

void App::applyScrubTarget(int targetSteps, uint32_t nowMs) {
  if (targetSteps == pausedTouch_.gestureStepsApplied) {
    return;
  }

  reader_.seekRelative(pausedTouch_.startWordIndex, targetSteps);
  pausedTouch_.gestureStepsApplied = targetSteps;
  if (!scrollModeEnabled()) {
    contextViewVisible_ = true;
  }
  wpmFeedbackVisible_ = false;
  renderActiveReader(nowMs);
  Serial.printf("[app] scrub target=%d word=%s\n", targetSteps, reader_.currentWord().c_str());
}

int App::browseScrollRatePermille(uint16_t y) const {
  const int centerY = BoardConfig::DISPLAY_HEIGHT / 2;
  const int signedDistance = static_cast<int>(y) - centerY;
  const int absDistance = abs(signedDistance);
  if (absDistance <= static_cast<int>(kBrowseNeutralZonePx)) {
    return 0;
  }

  const int activeRange = std::max(1, centerY - static_cast<int>(kBrowseNeutralZonePx));
  const int activeDistance =
      std::min(activeRange, absDistance - static_cast<int>(kBrowseNeutralZonePx));
  const uint32_t speedPermille =
      kBrowseMinWordsPerSecondPermille +
      ((kBrowseMaxWordsPerSecondPermille - kBrowseMinWordsPerSecondPermille) *
       static_cast<uint32_t>(activeDistance)) /
          static_cast<uint32_t>(activeRange);

  return signedDistance < 0 ? -static_cast<int>(speedPermille) : static_cast<int>(speedPermille);
}

void App::renderContextBrowsePreview(size_t currentIndex, uint16_t scrollProgressPermille) {
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    renderReaderWord();
    return;
  }

  if (currentIndex >= wordCount) {
    currentIndex = wordCount - 1;
    scrollProgressPermille = 0;
  }

  updateContextPreviewWindow(currentIndex);
  contextViewVisible_ = true;
  display_.renderScrollView(contextPreviewWords_, currentReaderContentToken(),
                            contextPreviewStartIndex_, currentIndex, scrollProgressPermille,
                            currentChapterLabel(), readingProgressPercent(), "",
                            currentFooterMetricLabel());
}

void App::applyBrowseHoldScroll(uint16_t y, uint32_t elapsedMs, uint32_t nowMs) {
  if (elapsedMs == 0) {
    return;
  }

  const int ratePermille = browseScrollRatePermille(y);
  pausedTouch_.browseOffsetPermille +=
      (static_cast<int32_t>(ratePermille) * static_cast<int32_t>(elapsedMs)) / 1000;

  int targetWords = pausedTouch_.browseOffsetPermille / 1000;
  int32_t remainderPermille = pausedTouch_.browseOffsetPermille % 1000;
  if (remainderPermille < 0) {
    remainderPermille += 1000;
    --targetWords;
  }

  reader_.seekRelative(pausedTouch_.startWordIndex, targetWords);
  pausedTouch_.gestureStepsApplied = targetWords;
  contextViewVisible_ = true;
  wpmFeedbackVisible_ = false;
  renderContextBrowsePreview(reader_.currentIndex(),
                             static_cast<uint16_t>(remainderPermille));
  Serial.printf("[app] browse hold target=%d progress=%ld word=%s\n", targetWords,
                static_cast<long>(remainderPermille), reader_.currentWord().c_str());
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

  if (menuScreen_ == MenuScreen::TextEntry) {
    if (absDeltaX <= static_cast<int>(kTapSlopPx) && absDeltaY <= static_cast<int>(kTapSlopPx)) {
      handleTextEntryTap(event.x, event.y, nowMs);
    }
    return;
  }

  if (menuScreen_ == MenuScreen::TypographyTuning &&
      absDeltaX >= static_cast<int>(kSwipeThresholdPx) &&
      absDeltaX > absDeltaY + static_cast<int>(kAxisBiasPx)) {
    cycleTypographyPreviewSample(deltaX < 0 ? 1 : -1);
    return;
  }

  if (absDeltaY >= static_cast<int>(kSwipeThresholdPx) &&
      absDeltaY > absDeltaX + static_cast<int>(kAxisBiasPx)) {
    moveMenuSelection(deltaY < 0 ? -1 : 1);
    return;
  }

  if (absDeltaX <= static_cast<int>(kTapSlopPx) && absDeltaY <= static_cast<int>(kTapSlopPx)) {
    selectMenuItem(nowMs);
  }
}

void App::applyFocusTimerTouch(const TouchEvent &event, uint32_t nowMs) {
  if (event.phase == TouchPhase::Start) {
    pausedTouch_.active = true;
    pausedTouch_.startX = event.x;
    pausedTouch_.startY = event.y;
    pausedTouch_.lastX = event.x;
    pausedTouch_.lastY = event.y;
    pausedTouch_.startMs = nowMs;
    pausedTouch_.lastMs = nowMs;
    focusTimerCancelHoldTriggered_ = false;
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
  const bool tapLike = absDeltaX <= static_cast<int>(kTapSlopPx) &&
                       absDeltaY <= static_cast<int>(kTapSlopPx);

  if (focusTimer_.isActiveTimerRunning() && !focusTimerCancelHoldTriggered_ &&
      event.phase != TouchPhase::End &&
      absDeltaX <= static_cast<int>(kFocusTimerCancelHoldMaxDriftPx) &&
      absDeltaY <= static_cast<int>(kFocusTimerCancelHoldMaxDriftPx) &&
      nowMs - pausedTouch_.startMs >= kFocusTimerCancelHoldMs) {
    focusTimer_.cancelActiveTimer(nowMs);
    pausedTouch_.active = false;
    focusTimerCancelHoldTriggered_ = true;
    renderFocusTimerSession();
    return;
  }

  if (event.phase != TouchPhase::End) {
    return;
  }

  pausedTouch_.active = false;

  if (focusTimerCancelHoldTriggered_) {
    focusTimerCancelHoldTriggered_ = false;
    return;
  }

  (void)tapLike;
}

void App::openFocusTimer() {
  focusTimer_.open();
  rebuildFocusTimerGenreMenuItems();
  focusTimerGenreSelectedIndex_ =
      focusTimerGenreMenuItems_.size() > 1 ? kFocusTimerGenreFirstIndex : kFocusTimerGenreBackIndex;
  focusTimerCancelHoldTriggered_ = false;
  menuScreen_ = (focusTimer_.state() == FocusTimer::State::GenreSelect)
                    ? MenuScreen::FocusTimerGenres
                    : MenuScreen::FocusTimerSession;
  renderMenu();
}

void App::updateFocusTimer(uint32_t nowMs) {
  if (state_ != AppState::Menu || menuScreen_ != MenuScreen::FocusTimerSession) {
    return;
  }

  focusTimer_.update(nowMs);
  if (focusTimer_.consumeCompletionCue()) {
    playFocusTimerCompletionCue();
  }
  if (focusTimer_.state() == FocusTimer::State::GenreSelect) {
    menuScreen_ = MenuScreen::FocusTimerGenres;
    rebuildFocusTimerGenreMenuItems();
    renderFocusTimerGenres();
    return;
  }

  renderFocusTimerSession();
}

void App::resetFocusTimer() {
  focusTimer_.abandon();
  focusTimerCancelHoldTriggered_ = false;
  pausedTouch_.active = false;
  focusTimerGenreSelectedIndex_ = kFocusTimerGenreBackIndex;
}

void App::rebuildFocusTimerGenreMenuItems() {
  focusTimerGenreMenuItems_.clear();
  focusTimerGenreMenuItems_.push_back(uiText(UiText::Back));
  focusTimerGenreMenuItems_.push_back("Chores");
  focusTimerGenreMenuItems_.push_back("Work");
  focusTimerGenreMenuItems_.push_back("Fitness");
  focusTimerGenreMenuItems_.push_back("Self Care");
  focusTimerGenreMenuItems_.push_back("Other");

  if (focusTimerGenreSelectedIndex_ >= focusTimerGenreMenuItems_.size()) {
    focusTimerGenreSelectedIndex_ =
        focusTimerGenreMenuItems_.size() > 1 ? kFocusTimerGenreFirstIndex : kFocusTimerGenreBackIndex;
  }
}

void App::selectFocusTimerGenre(uint32_t nowMs) {
  if (focusTimerGenreMenuItems_.empty()) {
    rebuildFocusTimerGenreMenuItems();
  }

  if (focusTimerGenreSelectedIndex_ == kFocusTimerGenreBackIndex) {
    resetFocusTimer();
    menuScreen_ = MenuScreen::Main;
    renderMainMenu();
    return;
  }

  FocusTimer::Genre genre = FocusTimer::Genre::None;
  switch (focusTimerGenreSelectedIndex_) {
    case 1:
      genre = FocusTimer::Genre::Chores;
      break;
    case 2:
      genre = FocusTimer::Genre::RsvpNano;
      break;
    case 3:
      genre = FocusTimer::Genre::StrengthLabs;
      break;
    case 4:
      genre = FocusTimer::Genre::SelfCare;
      break;
    case 5:
      genre = FocusTimer::Genre::Other;
      break;
    default:
      break;
  }

  if (genre == FocusTimer::Genre::None) {
    return;
  }

  focusTimer_.chooseGenre(genre, nowMs);
  focusTimerCancelHoldTriggered_ = false;
  menuScreen_ = MenuScreen::FocusTimerSession;
  renderFocusTimerSession();
}

void App::moveMenuSelection(int direction) {
  if (direction == 0 || menuScreen_ == MenuScreen::TextEntry) {
    return;
  }

  size_t *selectedIndex = &menuSelectedIndex_;
  size_t itemCount = MenuItemCount;
  if (menuScreen_ == MenuScreen::SettingsHome || menuScreen_ == MenuScreen::SettingsDisplay ||
      menuScreen_ == MenuScreen::SettingsPacing || menuScreen_ == MenuScreen::WifiSettings) {
    selectedIndex = &settingsSelectedIndex_;
    itemCount = settingsMenuItems_.size();
  } else if (menuScreen_ == MenuScreen::WifiNetworks) {
    selectedIndex = &wifiNetworkSelectedIndex_;
    itemCount = wifiNetworkMenuItems_.size();
  } else if (menuScreen_ == MenuScreen::TypographyTuning) {
    selectedIndex = &typographyTuningSelectedIndex_;
    itemCount = TypographyTuningItemCount;
  } else if (menuScreen_ == MenuScreen::BookPicker) {
    selectedIndex = &bookPickerSelectedIndex_;
    itemCount = bookMenuItems_.size();
  } else if (menuScreen_ == MenuScreen::ChapterPicker) {
    selectedIndex = &chapterPickerSelectedIndex_;
    itemCount = chapterMenuItems_.size();
  } else if (menuScreen_ == MenuScreen::RestartConfirm) {
    selectedIndex = &restartConfirmSelectedIndex_;
    itemCount = RestartConfirmItemCount;
  } else if (menuScreen_ == MenuScreen::FocusTimerGenres) {
    selectedIndex = &focusTimerGenreSelectedIndex_;
    itemCount = focusTimerGenreMenuItems_.size();
  }

  if (itemCount == 0) {
    return;
  }

  const int next = static_cast<int>(*selectedIndex) + direction;
  if (next < 0) {
    *selectedIndex = itemCount - 1;
  } else if (next >= static_cast<int>(itemCount)) {
    *selectedIndex = 0;
  } else {
    *selectedIndex = static_cast<size_t>(next);
  }

  renderMenu();
  if (menuScreen_ == MenuScreen::SettingsHome || menuScreen_ == MenuScreen::SettingsDisplay ||
      menuScreen_ == MenuScreen::SettingsPacing || menuScreen_ == MenuScreen::WifiSettings) {
    Serial.printf("[settings] selected=%s\n", settingsMenuItems_[settingsSelectedIndex_].c_str());
  } else if (menuScreen_ == MenuScreen::WifiNetworks) {
    Serial.printf("[wifi] selected=%s\n", wifiNetworkMenuItems_[wifiNetworkSelectedIndex_].title.c_str());
  } else if (menuScreen_ == MenuScreen::TypographyTuning) {
    Serial.printf("[typography] selected=%s\n", typographyTuningLabel().c_str());
  } else if (menuScreen_ == MenuScreen::BookPicker) {
    Serial.printf("[book-picker] selected=%s\n",
                  bookMenuItems_[bookPickerSelectedIndex_].title.c_str());
  } else if (menuScreen_ == MenuScreen::ChapterPicker) {
    Serial.printf("[chapter-picker] selected=%s\n",
                  chapterMenuItems_[chapterPickerSelectedIndex_].c_str());
  } else if (menuScreen_ == MenuScreen::RestartConfirm) {
    String selectedLabel = uiText(UiText::AreYouSure);
    switch (restartConfirmSelectedIndex_) {
      case RestartConfirmNo:
        selectedLabel = uiText(UiText::NoKeepPlace);
        break;
      case RestartConfirmYes:
        selectedLabel = uiText(UiText::YesRestart);
        break;
      default:
        break;
    }
    Serial.printf("[restart] selected=%s\n", selectedLabel.c_str());
  } else if (menuScreen_ == MenuScreen::FocusTimerGenres) {
    Serial.printf("[timer] selected genre=%s\n",
                  focusTimerGenreMenuItems_[focusTimerGenreSelectedIndex_].c_str());
  } else {
    String selectedLabel = uiText(UiText::Resume);
    switch (menuSelectedIndex_) {
      case MenuResume:
        selectedLabel = uiText(UiText::Resume);
        break;
      case MenuChapters:
        selectedLabel = uiText(UiText::Chapters);
        break;
      case MenuChangeBook:
        selectedLabel = uiText(UiText::Library);
        break;
      case MenuFocusTimer:
        selectedLabel = "Focus Timer";
        break;
      case MenuSettings:
        selectedLabel = uiText(UiText::Settings);
        break;
#if RSVP_USB_TRANSFER_ENABLED
      case MenuUsbTransfer:
        selectedLabel = uiText(UiText::UsbTransfer);
        break;
#endif
      case MenuPowerOff:
        selectedLabel = uiText(UiText::PowerOff);
        break;
      default:
        break;
    }
    Serial.printf("[menu] selected=%s\n", selectedLabel.c_str());
  }
}

void App::selectMenuItem(uint32_t nowMs) {
  if (menuScreen_ == MenuScreen::SettingsHome || menuScreen_ == MenuScreen::SettingsDisplay ||
      menuScreen_ == MenuScreen::SettingsPacing || menuScreen_ == MenuScreen::WifiSettings) {
    selectSettingsItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::WifiNetworks) {
    selectWifiNetworkItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::TypographyTuning) {
    selectTypographyTuningItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::BookPicker) {
    selectBookPickerItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::ChapterPicker) {
    selectChapterPickerItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::RestartConfirm) {
    selectRestartConfirmItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::FocusTimerGenres) {
    selectFocusTimerGenre(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::FocusTimerSession) {
    return;
  }

  switch (menuSelectedIndex_) {
    case MenuResume:
      setState(AppState::Paused, nowMs);
      return;
    case MenuPowerOff:
      enterPowerOff(nowMs);
      return;
#if RSVP_USB_TRANSFER_ENABLED
    case MenuUsbTransfer:
      enterUsbTransfer(nowMs);
      return;
#endif
    case MenuChapters:
      openChapterPicker();
      return;
    case MenuChangeBook:
      openBookPicker();
      return;
    case MenuFocusTimer:
      openFocusTimer();
      return;
    case MenuSettings:
      openSettings();
      return;
    default:
      return;
  }
}

void App::openSettings() {
  settingsSelectedIndex_ = kSettingsHomeDisplayIndex;
  menuScreen_ = MenuScreen::SettingsHome;
  rebuildSettingsMenuItems();
  renderSettings();
}

void App::selectSettingsItem(uint32_t nowMs) {
  if (settingsMenuItems_.empty()) {
    if (menuScreen_ == MenuScreen::WifiSettings) {
      openWifiSettings();
    } else {
      openSettings();
    }
    return;
  }

  if (menuScreen_ == MenuScreen::SettingsHome) {
    switch (settingsSelectedIndex_) {
      case kSettingsBackIndex:
        menuScreen_ = MenuScreen::Main;
        renderMainMenu();
        return;
      case kSettingsHomeDisplayIndex:
        settingsSelectedIndex_ = kSettingsDisplayReadingModeIndex;
        menuScreen_ = MenuScreen::SettingsDisplay;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsHomeTypographyIndex:
        openTypographyTuning();
        return;
      case kSettingsHomePacingIndex:
        settingsSelectedIndex_ = kSettingsPacingLongWordsIndex;
        menuScreen_ = MenuScreen::SettingsPacing;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsHomeWifiIndex:
        openWifiSettings();
        return;
      case kSettingsHomeUpdateIndex: {
        runFirmwareUpdate(preferredOtaConfig(), false, nowMs);
        return;
      }
      default:
        return;
    }
  }

  if (menuScreen_ == MenuScreen::WifiSettings) {
    selectWifiSettingsItem(nowMs);
    return;
  }

  if (menuScreen_ == MenuScreen::SettingsDisplay) {
    switch (settingsSelectedIndex_) {
      case kSettingsBackIndex:
        settingsSelectedIndex_ = kSettingsHomeDisplayIndex;
        menuScreen_ = MenuScreen::SettingsHome;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayReadingModeIndex:
        cycleReaderMode(nowMs);
        return;
      case kSettingsDisplayHandednessIndex:
        cycleHandednessMode(nowMs);
        return;
      case kSettingsDisplayThemeIndex:
        cycleThemeMode(nowMs);
        return;
      case kSettingsDisplayBrightnessIndex:
        cycleBrightness();
        return;
      case kSettingsDisplayLanguageIndex:
        cycleUiLanguage(nowMs);
        return;
      default:
        return;
    }
  }

  if (menuScreen_ != MenuScreen::SettingsPacing) {
    return;
  }

  switch (settingsSelectedIndex_) {
    case kSettingsBackIndex:
      flushPendingTimeEstimateRebuild();
      settingsSelectedIndex_ = kSettingsHomePacingIndex;
      menuScreen_ = MenuScreen::SettingsHome;
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kSettingsPacingLongWordsIndex:
      pacingLongWordDelayMs_ = static_cast<uint16_t>(nextCyclicSetting(
          pacingLongWordDelayMs_, kPacingDelayMinMs, kPacingDelayMaxMs, kPacingDelayStepMs));
      preferences_.putUShort(kPrefPacingLongMs, pacingLongWordDelayMs_);
      break;
    case kSettingsPacingComplexityIndex:
      pacingComplexWordDelayMs_ = static_cast<uint16_t>(nextCyclicSetting(
          pacingComplexWordDelayMs_, kPacingDelayMinMs, kPacingDelayMaxMs, kPacingDelayStepMs));
      preferences_.putUShort(kPrefPacingComplexMs, pacingComplexWordDelayMs_);
      break;
    case kSettingsPacingPunctuationIndex:
      pacingPunctuationDelayMs_ = static_cast<uint16_t>(nextCyclicSetting(
          pacingPunctuationDelayMs_, kPacingDelayMinMs, kPacingDelayMaxMs, kPacingDelayStepMs));
      preferences_.putUShort(kPrefPacingPunctuationMs, pacingPunctuationDelayMs_);
      break;
    case kSettingsPacingTimeEstimateIndex:
      accurateTimeEstimateEnabled_ = !accurateTimeEstimateEnabled_;
      preferences_.putBool(kPrefAccurateTime, accurateTimeEstimateEnabled_);
      if (accurateTimeEstimateEnabled_) {
        rebuildTimeEstimateCache();
      } else {
        invalidateTimeEstimateCache();
      }
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kSettingsPacingResetIndex:
      pacingLongWordDelayMs_ = kDefaultPacingDelayMs;
      pacingComplexWordDelayMs_ = kDefaultPacingDelayMs;
      pacingPunctuationDelayMs_ = kDefaultPacingDelayMs;
      preferences_.putUShort(kPrefPacingLongMs, pacingLongWordDelayMs_);
      preferences_.putUShort(kPrefPacingComplexMs, pacingComplexWordDelayMs_);
      preferences_.putUShort(kPrefPacingPunctuationMs, pacingPunctuationDelayMs_);
      break;
    default:
      return;
  }

  applyPacingSettings();
  rebuildSettingsMenuItems();
  renderSettings();
}

void App::openWifiSettings() {
  settingsSelectedIndex_ = configuredWifiSsid().isEmpty() ? kWifiSettingsChooseIndex
                                                          : kWifiSettingsAutoUpdateIndex;
  menuScreen_ = MenuScreen::WifiSettings;
  rebuildSettingsMenuItems();
  renderSettings();
}

void App::selectWifiSettingsItem(uint32_t nowMs) {
  (void)nowMs;

  switch (settingsSelectedIndex_) {
    case kSettingsBackIndex:
      settingsSelectedIndex_ = kSettingsHomeWifiIndex;
      menuScreen_ = MenuScreen::SettingsHome;
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kWifiSettingsNetworkIndex:
    case kWifiSettingsChooseIndex:
      scanWifiNetworks();
      return;
    case kWifiSettingsAutoUpdateIndex:
      preferences_.putBool(kPrefOtaAuto, !otaAutoCheckEnabled());
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kWifiSettingsForgetIndex:
      preferences_.remove(kPrefWifiSsid);
      preferences_.remove(kPrefWifiPass);
      display_.renderStatus("Wi-Fi", "Credentials cleared", "");
      delay(900);
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kWifiSettingsOtaOwnerIndex:
      openTextEntry(TextEntryPurpose::OtaOwner, "OTA Source", "GitHub owner", "",
                    preferences_.getString(kPrefOtaOwner, ""), "", false, 39,
                    MenuScreen::WifiSettings);
      return;
    default:
      return;
  }
}

void App::scanWifiNetworks() {
  display_.renderProgress("Wi-Fi", "Scanning networks", "", 5);

  WiFi.persistent(false);
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_STA);
  WiFi.scanDelete();

  const int networkCount = WiFi.scanNetworks(false, true);
  wifiNetworks_.clear();
  wifiNetworkMenuItems_.clear();
  wifiNetworkMenuItems_.push_back({uiText(UiText::Back), ""});

  if (networkCount > 0) {
    for (int i = 0; i < networkCount; ++i) {
      const String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) {
        continue;
      }

      WifiNetworkInfo network;
      network.ssid = ssid;
      network.rssi = WiFi.RSSI(i);
      network.authMode = static_cast<uint8_t>(WiFi.encryptionType(i));
      wifiNetworks_.push_back(network);
    }
  }

  WiFi.scanDelete();
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);

  if (wifiNetworks_.empty()) {
    display_.renderStatus("Wi-Fi", "No networks found", "");
    delay(1200);
    openWifiSettings();
    return;
  }

  const String savedSsid = configuredWifiSsid();
  std::stable_sort(wifiNetworks_.begin(), wifiNetworks_.end(),
                   [&savedSsid](const WifiNetworkInfo &left, const WifiNetworkInfo &right) {
                     const bool leftSaved = !savedSsid.isEmpty() && left.ssid == savedSsid;
                     const bool rightSaved = !savedSsid.isEmpty() && right.ssid == savedSsid;
                     if (leftSaved != rightSaved) {
                       return leftSaved;
                     }
                     if (left.rssi != right.rssi) {
                       return left.rssi > right.rssi;
                     }
                     return left.ssid < right.ssid;
                   });

  wifiNetworkMenuItems_.reserve(wifiNetworks_.size() + 1);
  for (const WifiNetworkInfo &network : wifiNetworks_) {
    wifiNetworkMenuItems_.push_back(
        {network.ssid, wifiSecurityLabel(network.authMode) + "  " + String(network.rssi) + " dBm"});
  }

  wifiNetworkSelectedIndex_ =
      wifiNetworkMenuItems_.size() > 1 ? kWifiNetworksFirstItemIndex : kWifiNetworksBackIndex;
  menuScreen_ = MenuScreen::WifiNetworks;
  renderWifiNetworks();
}

void App::renderWifiNetworks() {
  if (wifiNetworkMenuItems_.empty()) {
    display_.renderStatus("Wi-Fi", "No networks found", "");
    return;
  }

  display_.renderLibrary(wifiNetworkMenuItems_, wifiNetworkSelectedIndex_);
}

void App::selectWifiNetworkItem(uint32_t nowMs) {
  (void)nowMs;

  if (wifiNetworkSelectedIndex_ == kWifiNetworksBackIndex || wifiNetworkMenuItems_.size() <= 1) {
    openWifiSettings();
    return;
  }

  const size_t networkIndex = wifiNetworkSelectedIndex_ - kWifiNetworksFirstItemIndex;
  if (networkIndex >= wifiNetworks_.size()) {
    openWifiSettings();
    return;
  }

  const WifiNetworkInfo &network = wifiNetworks_[networkIndex];
  if (wifiNetworkRequiresPassword(network.authMode)) {
    String initialValue;
    if (configuredWifiSsid() == network.ssid) {
      initialValue = preferredOtaConfig().wifiPassword;
    }
    openTextEntry(TextEntryPurpose::WifiPassword, network.ssid, "Password", "",
                  initialValue, network.ssid, true, kWifiPasswordMaxLength,
                  MenuScreen::WifiNetworks);
    return;
  }

  preferences_.putString(kPrefWifiSsid, network.ssid);
  preferences_.putString(kPrefWifiPass, "");
  display_.renderStatus("Wi-Fi", "Network saved", network.ssid);
  delay(900);
  openWifiSettings();
}

void App::openTextEntry(TextEntryPurpose purpose, const String &title, const String &prompt,
                        const String &helperText, const String &initialValue,
                        const String &contextValue, bool masked, size_t maxLength,
                        MenuScreen returnScreen) {
  textEntrySession_ = TextEntrySession();
  textEntrySession_.active = true;
  textEntrySession_.purpose = purpose;
  textEntrySession_.mode = KeyboardMode::Lower;
  textEntrySession_.returnScreen = returnScreen;
  textEntrySession_.title = title;
  textEntrySession_.prompt = prompt;
  textEntrySession_.helperText = helperText;
  textEntrySession_.value = initialValue;
  textEntrySession_.contextValue = contextValue;
  textEntrySession_.maxLength = maxLength;
  textEntrySession_.masked = masked;
  textEntrySession_.revealValue = false;
  menuScreen_ = MenuScreen::TextEntry;
  rebuildTextEntryButtons();
  renderTextEntry();
}

void App::rebuildTextEntryButtons() {
  textEntryButtons_.clear();
  if (!textEntrySession_.active) {
    return;
  }

  const uint16_t rowPitch = kKeyboardRowHeight + kKeyboardRowGap;
  for (size_t rowIndex = 0; rowIndex < 3; ++rowIndex) {
    const String rowChars = keyboardRowText(static_cast<uint8_t>(textEntrySession_.mode), rowIndex);
    const size_t keyCount = rowChars.length();
    if (keyCount == 0) {
      continue;
    }

    const int availableWidth =
        BoardConfig::DISPLAY_WIDTH - (2 * kKeyboardMarginX) -
        static_cast<int>((keyCount - 1) * kKeyboardRowGap);
    const int keyWidth = std::max(28, availableWidth / static_cast<int>(keyCount));
    const int totalWidth =
        keyWidth * static_cast<int>(keyCount) + static_cast<int>((keyCount - 1) * kKeyboardRowGap);
    int x = std::max(0, (BoardConfig::DISPLAY_WIDTH - totalWidth) / 2);
    const int y = kKeyboardTopY + static_cast<int>(rowIndex * rowPitch);

    for (size_t charIndex = 0; charIndex < keyCount; ++charIndex) {
      TextEntryButton button;
      button.view.label = String(rowChars[charIndex]);
      button.view.x = static_cast<uint16_t>(x);
      button.view.y = static_cast<uint16_t>(y);
      button.view.width = static_cast<uint16_t>(keyWidth);
      button.view.height = kKeyboardRowHeight;
      button.action = TextEntryAction::Insert;
      button.payload = String(rowChars[charIndex]);
      textEntryButtons_.push_back(button);
      x += keyWidth + kKeyboardRowGap;
    }
  }

  struct ControlButtonDef {
    String label;
    TextEntryAction action;
    uint16_t units;
    bool accent;
    bool active;
  };

  const bool revealActive = textEntrySession_.masked && textEntrySession_.revealValue;
  const ControlButtonDef controls[] = {
      {"abc", TextEntryAction::SetLower, 11, false,
       textEntrySession_.mode == KeyboardMode::Lower},
      {"ABC", TextEntryAction::SetUpper, 11, false,
       textEntrySession_.mode == KeyboardMode::Upper},
      {"123", TextEntryAction::SetSymbols, 11, false,
       textEntrySession_.mode == KeyboardMode::Symbols},
      {"space", TextEntryAction::Space, 24, false, false},
      {"back", TextEntryAction::Backspace, 13, false, false},
      {textEntrySession_.masked ? (revealActive ? "hide" : "show") : "clear",
       textEntrySession_.masked ? TextEntryAction::ToggleMask : TextEntryAction::Clear, 13, false,
       revealActive},
      {"save", TextEntryAction::Save, 12, true, false},
      {"cancel", TextEntryAction::Cancel, 14, false, false},
  };

  uint16_t totalUnits = 0;
  for (const ControlButtonDef &control : controls) {
    totalUnits += control.units;
  }

  const size_t controlCount = sizeof(controls) / sizeof(controls[0]);
  const int totalGapWidth = static_cast<int>((controlCount - 1) * kKeyboardRowGap);
  const int availableWidth = BoardConfig::DISPLAY_WIDTH - (2 * kKeyboardMarginX) - totalGapWidth;
  int remainingWidth = availableWidth;
  uint16_t x = kKeyboardMarginX;
  const uint16_t y = kKeyboardTopY + static_cast<uint16_t>(3 * rowPitch);

  for (size_t i = 0; i < controlCount; ++i) {
    const ControlButtonDef &control = controls[i];
    int width = remainingWidth;
    if (i + 1 < controlCount) {
      width = (availableWidth * control.units) / totalUnits;
      remainingWidth -= width;
    }

    TextEntryButton button;
    button.view.label = control.label;
    button.view.x = x;
    button.view.y = y;
    button.view.width = static_cast<uint16_t>(std::max(28, width));
    button.view.height = kKeyboardRowHeight;
    button.view.accent = control.accent;
    button.view.active = control.active;
    button.action = control.action;
    textEntryButtons_.push_back(button);

    x = static_cast<uint16_t>(x + button.view.width + kKeyboardRowGap);
  }
}

void App::renderTextEntry() {
  if (!textEntrySession_.active) {
    return;
  }

  const String visibleValue =
      (textEntrySession_.masked && !textEntrySession_.revealValue)
          ? maskedValue(textEntrySession_.value)
          : textEntrySession_.value;

  std::vector<DisplayManager::Button> buttons;
  buttons.reserve(textEntryButtons_.size());
  for (const TextEntryButton &button : textEntryButtons_) {
    buttons.push_back(button.view);
  }

  display_.renderTextEntry(textEntrySession_.title, textEntrySession_.prompt, visibleValue,
                           textEntrySession_.helperText, buttons);
}

bool App::handleTextEntryTap(uint16_t x, uint16_t y, uint32_t nowMs) {
  if (!textEntrySession_.active) {
    return false;
  }

  for (size_t i = 0; i < textEntryButtons_.size(); ++i) {
    const DisplayManager::Button &button = textEntryButtons_[i].view;
    const uint16_t maxX = button.x + button.width;
    const uint16_t maxY = button.y + button.height;
    if (x < button.x || x > maxX || y < button.y || y > maxY) {
      continue;
    }

    activateTextEntryButton(i, nowMs);
    return true;
  }

  return false;
}

void App::activateTextEntryButton(size_t buttonIndex, uint32_t nowMs) {
  if (buttonIndex >= textEntryButtons_.size()) {
    return;
  }

  TextEntryButton &button = textEntryButtons_[buttonIndex];
  switch (button.action) {
    case TextEntryAction::Insert:
      if (textEntrySession_.value.length() < textEntrySession_.maxLength) {
        textEntrySession_.value += button.payload;
      }
      break;
    case TextEntryAction::SetLower:
      textEntrySession_.mode = KeyboardMode::Lower;
      break;
    case TextEntryAction::SetUpper:
      textEntrySession_.mode = KeyboardMode::Upper;
      break;
    case TextEntryAction::SetSymbols:
      textEntrySession_.mode = KeyboardMode::Symbols;
      break;
    case TextEntryAction::Space:
      if (textEntrySession_.value.length() < textEntrySession_.maxLength) {
        textEntrySession_.value += ' ';
      }
      break;
    case TextEntryAction::Backspace:
      if (!textEntrySession_.value.isEmpty()) {
        textEntrySession_.value.remove(textEntrySession_.value.length() - 1);
      }
      break;
    case TextEntryAction::Clear:
      textEntrySession_.value = "";
      break;
    case TextEntryAction::ToggleMask:
      if (textEntrySession_.masked) {
        textEntrySession_.revealValue = !textEntrySession_.revealValue;
      }
      break;
    case TextEntryAction::Save:
      commitTextEntry(nowMs);
      return;
    case TextEntryAction::Cancel:
      menuScreen_ = textEntrySession_.returnScreen;
      textEntrySession_ = TextEntrySession();
      textEntryButtons_.clear();
      renderMenu();
      return;
  }

  rebuildTextEntryButtons();
  renderTextEntry();
}

void App::commitTextEntry(uint32_t nowMs) {
  (void)nowMs;

  switch (textEntrySession_.purpose) {
    case TextEntryPurpose::WifiPassword: {
      if (textEntrySession_.value.isEmpty()) {
        display_.renderStatus("Wi-Fi", "Password required", textEntrySession_.contextValue);
        delay(1000);
        renderTextEntry();
        return;
      }

      const String ssid = textEntrySession_.contextValue;
      preferences_.putString(kPrefWifiSsid, ssid);
      preferences_.putString(kPrefWifiPass, textEntrySession_.value);
      textEntrySession_ = TextEntrySession();
      textEntryButtons_.clear();
      display_.renderStatus("Wi-Fi", "Network saved", ssid);
      delay(900);
      openWifiSettings();
      return;
    }
    case TextEntryPurpose::OtaOwner: {
      const String owner = textEntrySession_.value;
      if (owner.isEmpty()) {
        preferences_.remove(kPrefOtaOwner);
        display_.renderStatus("OTA", "Reset to default", "");
      } else {
        preferences_.putString(kPrefOtaOwner, owner);
        display_.renderStatus("OTA", "Owner saved", owner);
      }
      delay(900);
      textEntrySession_ = TextEntrySession();
      textEntryButtons_.clear();
      openWifiSettings();
      return;
    }
    case TextEntryPurpose::None:
    default:
      menuScreen_ = textEntrySession_.returnScreen;
      textEntrySession_ = TextEntrySession();
      textEntryButtons_.clear();
      renderMenu();
      return;
  }
}

void App::openTypographyTuning() {
  if (typographyTuningSelectedIndex_ >= TypographyTuningItemCount) {
    typographyTuningSelectedIndex_ = TypographyTuningFontSize;
  }
  if (typographyTuningSelectedIndex_ == TypographyTuningBack) {
    typographyTuningSelectedIndex_ = TypographyTuningFontSize;
  }
  menuScreen_ = MenuScreen::TypographyTuning;
  renderTypographyTuning();
}

void App::selectTypographyTuningItem(uint32_t nowMs) {
  switch (typographyTuningSelectedIndex_) {
    case TypographyTuningBack:
      settingsSelectedIndex_ = kSettingsHomeTypographyIndex;
      menuScreen_ = MenuScreen::SettingsHome;
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case TypographyTuningFontSize:
      cycleReaderFontSize(nowMs);
      return;
    case TypographyTuningTypeface:
      typographyConfig_.typeface = nextReaderTypeface(typographyConfig_.typeface);
      preferences_.putUChar(kPrefReaderTypeface, static_cast<uint8_t>(typographyConfig_.typeface));
      break;
    case TypographyTuningPhantomWords:
      togglePhantomWords(nowMs);
      return;
    case TypographyTuningFocusHighlight:
      typographyConfig_.focusHighlight = !typographyConfig_.focusHighlight;
      preferences_.putBool(kPrefTypographyFocusHighlight, typographyConfig_.focusHighlight);
      break;
    case TypographyTuningTracking:
      typographyConfig_.trackingPx = static_cast<int8_t>(
          nextCyclicSetting(typographyConfig_.trackingPx, kTypographyTrackingMin,
                            kTypographyTrackingMax));
      preferences_.putChar(kPrefTypographyTracking, typographyConfig_.trackingPx);
      break;
    case TypographyTuningAnchor: {
      const uint8_t anchorMin =
          (handednessMode_ == HandednessMode::Left) ? kLeftHandAnchorMin : kTypographyAnchorMin;
      const uint8_t anchorMax =
          (handednessMode_ == HandednessMode::Left) ? kLeftHandAnchorMax : kTypographyAnchorMax;
      const uint8_t nextAnchorPercent = static_cast<uint8_t>(
          nextCyclicSetting(effectiveAnchorPercent(), anchorMin, anchorMax));
      typographyConfig_.anchorPercent = (handednessMode_ == HandednessMode::Left)
                                            ? static_cast<uint8_t>(nextAnchorPercent -
                                                                   kLeftHandAnchorOffset)
                                            : nextAnchorPercent;
      preferences_.putUChar(kPrefTypographyAnchor, typographyConfig_.anchorPercent);
      break;
    }
    case TypographyTuningGuideWidth:
      typographyConfig_.guideHalfWidth = static_cast<uint8_t>(nextCyclicSetting(
          typographyConfig_.guideHalfWidth, kTypographyGuideWidthMin,
          kTypographyGuideWidthMax, kTypographyGuideWidthStep));
      preferences_.putUChar(kPrefTypographyGuideWidth, typographyConfig_.guideHalfWidth);
      break;
    case TypographyTuningGuideGap:
      typographyConfig_.guideGap = static_cast<uint8_t>(nextCyclicSetting(
          typographyConfig_.guideGap, kTypographyGuideGapMin, kTypographyGuideGapMax));
      preferences_.putUChar(kPrefTypographyGuideGap, typographyConfig_.guideGap);
      break;
    case TypographyTuningReset:
      typographyConfig_ = defaultTypographyConfig();
      preferences_.putUChar(kPrefReaderTypeface, static_cast<uint8_t>(typographyConfig_.typeface));
      preferences_.putBool(kPrefTypographyFocusHighlight, typographyConfig_.focusHighlight);
      preferences_.putChar(kPrefTypographyTracking, typographyConfig_.trackingPx);
      preferences_.putUChar(kPrefTypographyAnchor, typographyConfig_.anchorPercent);
      preferences_.putUChar(kPrefTypographyGuideWidth, typographyConfig_.guideHalfWidth);
      preferences_.putUChar(kPrefTypographyGuideGap, typographyConfig_.guideGap);
      break;
    default:
      return;
  }

  applyTypographySettings(nowMs);
}

void App::cycleTypographyPreviewSample(int direction) {
  if (kTypographyPreviewWordCount == 0 || direction == 0) {
    return;
  }

  const int current = static_cast<int>(typographyPreviewSampleIndex_);
  int next = current + direction;
  if (next < 0) {
    next = static_cast<int>(kTypographyPreviewWordCount) - 1;
  } else if (next >= static_cast<int>(kTypographyPreviewWordCount)) {
    next = 0;
  }
  typographyPreviewSampleIndex_ = static_cast<size_t>(next);
  renderTypographyTuning();
}

void App::rebuildSettingsMenuItems() {
  settingsMenuItems_.clear();
  settingsMenuItems_.reserve(SettingsItemCount);
  if (menuScreen_ == MenuScreen::SettingsHome) {
    settingsMenuItems_.push_back(uiText(UiText::Back));
    settingsMenuItems_.push_back(uiText(UiText::Display));
    settingsMenuItems_.push_back(uiText(UiText::TypographyTune));
    settingsMenuItems_.push_back(uiText(UiText::WordPacing));
    settingsMenuItems_.push_back("Wi-Fi");
    settingsMenuItems_.push_back(firmwareUpdateMenuLabel());
  } else if (menuScreen_ == MenuScreen::SettingsDisplay) {
    settingsMenuItems_.push_back(uiText(UiText::Back));
    settingsMenuItems_.push_back(uiText(UiText::ReadingMode) + ": " + readerModeLabel());
    settingsMenuItems_.push_back("L/R Hand: " + handednessLabel());
    settingsMenuItems_.push_back(uiText(UiText::Theme) + ": " + themeModeLabel());
    settingsMenuItems_.push_back(uiText(UiText::Brightness) + ": " +
                                 String(currentBrightnessPercent()) + "%");
    settingsMenuItems_.push_back(uiText(UiText::Language) + ": " + uiLanguageLabel());
  } else if (menuScreen_ == MenuScreen::SettingsPacing) {
    settingsMenuItems_.push_back(uiText(UiText::Back));
    settingsMenuItems_.push_back(uiText(UiText::LongWords) + ": " +
                                 pacingDelayLabel(pacingLongWordDelayMs_));
    settingsMenuItems_.push_back(uiText(UiText::Complexity) + ": " +
                                 pacingDelayLabel(pacingComplexWordDelayMs_));
    settingsMenuItems_.push_back(uiText(UiText::Punctuation) + ": " +
                                 pacingDelayLabel(pacingPunctuationDelayMs_));
    settingsMenuItems_.push_back(uiText(UiText::TimeEstimate) + ": " + timeEstimateModeLabel());
    settingsMenuItems_.push_back(uiText(UiText::ResetPacing));
  } else if (menuScreen_ == MenuScreen::WifiSettings) {
    settingsMenuItems_.push_back(uiText(UiText::Back));
    settingsMenuItems_.push_back("Network: " + storedOrFallbackLabel(configuredWifiSsid(), "Not set"));
    settingsMenuItems_.push_back("Choose network");
    settingsMenuItems_.push_back("Auto OTA: " + String(otaAutoCheckEnabled() ? "On" : "Off"));
    settingsMenuItems_.push_back("Forget network");
    settingsMenuItems_.push_back("OTA Owner: " + otaOwnerLabel());
  }

  if (settingsSelectedIndex_ >= settingsMenuItems_.size()) {
    settingsSelectedIndex_ = kSettingsBackIndex;
  }
}

void App::applyPacingSettings() {
  ReadingLoop::PacingConfig pacingConfig;
  pacingConfig.longWordDelayMs = pacingLongWordDelayMs_;
  pacingConfig.complexWordDelayMs = pacingComplexWordDelayMs_;
  pacingConfig.punctuationDelayMs = pacingPunctuationDelayMs_;
  reader_.setPacingConfig(pacingConfig);

  Serial.printf("[settings] pacing long=%u ms complexity=%u ms punctuation=%u ms\n",
                static_cast<unsigned int>(pacingLongWordDelayMs_),
                static_cast<unsigned int>(pacingComplexWordDelayMs_),
                static_cast<unsigned int>(pacingPunctuationDelayMs_));
  if (state_ == AppState::Menu && menuScreen_ == MenuScreen::SettingsPacing) {
    pacingCacheDirty_ = true;
  } else {
    rebuildTimeEstimateCache();
  }
}

void App::flushPendingTimeEstimateRebuild() {
  if (!pacingCacheDirty_) {
    return;
  }
  rebuildTimeEstimateCache();
}

String App::otaOwnerLabel() {
  if (preferences_.isKey(kPrefOtaOwner)) {
    return preferences_.getString(kPrefOtaOwner, "");
  }
  OtaUpdater::Config cfg;
  otaUpdater_.loadConfig(cfg);
  return cfg.githubOwner;
}

OtaUpdater::Config App::preferredOtaConfig() {
  OtaUpdater::Config otaConfig;
  otaUpdater_.loadConfig(otaConfig);

  if (preferences_.isKey(kPrefWifiSsid)) {
    otaConfig.wifiSsid = preferences_.getString(kPrefWifiSsid, "");
  }
  if (preferences_.isKey(kPrefWifiPass)) {
    otaConfig.wifiPassword = preferences_.getString(kPrefWifiPass, "");
  }
  if (preferences_.isKey(kPrefOtaAuto)) {
    otaConfig.autoCheck = preferences_.getBool(kPrefOtaAuto, otaConfig.autoCheck);
  }
  if (preferences_.isKey(kPrefOtaOwner)) {
    otaConfig.githubOwner = preferences_.getString(kPrefOtaOwner, "");
  }

  return otaConfig;
}

String App::configuredWifiSsid() {
  String ssid = preferences_.getString(kPrefWifiSsid, "");
  if (ssid.isEmpty()) {
    OtaUpdater::Config otaConfig;
    otaUpdater_.loadConfig(otaConfig);
    ssid = otaConfig.wifiSsid;
  }
  ssid.trim();
  return ssid;
}

bool App::otaAutoCheckEnabled() {
  if (preferences_.isKey(kPrefOtaAuto)) {
    return preferences_.getBool(kPrefOtaAuto, false);
  }

  OtaUpdater::Config otaConfig;
  otaUpdater_.loadConfig(otaConfig);
  return otaConfig.autoCheck;
}

void App::maybeAutoCheckForUpdates(uint32_t nowMs) {
  OtaUpdater::Config otaConfig = preferredOtaConfig();
  if (!otaConfig.autoCheck || !otaUpdater_.isConfigured(otaConfig)) {
    return;
  }

  Serial.println("[ota] auto-check enabled");
  runFirmwareUpdate(otaConfig, true, nowMs);
}

void App::runFirmwareUpdate(const OtaUpdater::Config &config, bool automatic, uint32_t nowMs) {
  (void)nowMs;
  if (!otaUpdater_.isConfigured(config)) {
    if (!automatic) {
      display_.renderStatus("OTA", "Wi-Fi not set", "Settings -> Wi-Fi");
      delay(1600);
      rebuildSettingsMenuItems();
      renderSettings();
    }
    return;
  }

  saveReadingPosition(true);
  const OtaUpdater::Result result =
      otaUpdater_.checkAndInstall(config, &App::handleStorageStatus, this);

  Serial.printf("[ota] code=%u current=%s latest=%s summary=%s detail=%s\n",
                static_cast<unsigned int>(result.code), result.currentVersion.c_str(),
                result.latestVersion.c_str(), result.summary.c_str(), result.detail.c_str());

  if (result.rebootRequired) {
    display_.renderStatus("OTA", "Restarting", result.latestVersion);
    delay(300);
    ESP.restart();
    return;
  }

  if (automatic) {
    return;
  }

  const String line2 = result.detail.isEmpty() ? result.currentVersion : result.detail;
  display_.renderStatus("OTA", result.summary, line2);
  delay(1600);
  rebuildSettingsMenuItems();
  renderSettings();
}

String App::pacingDelayLabel(uint16_t delayMs) const { return String(delayMs) + " ms"; }

String App::firmwareUpdateMenuLabel() const { return "Firmware update"; }

String App::uiText(UiText key) const { return Localization::text(uiLanguage_, key); }

String App::themeModeLabel() const {
  if (nightMode_) {
    return uiText(UiText::Night);
  }
  return darkMode_ ? uiText(UiText::Dark) : uiText(UiText::Light);
}

String App::phantomWordsLabel() const {
  return phantomWordsEnabled_ ? uiText(UiText::On) : uiText(UiText::Off);
}

String App::focusHighlightLabel() const {
  return typographyConfig_.focusHighlight ? uiText(UiText::On) : uiText(UiText::Off);
}

String App::uiLanguageLabel() const { return Localization::languageName(uiLanguage_); }

String App::readerModeLabel() const {
  switch (readerMode_) {
    case ReaderMode::Scroll:
      return uiText(UiText::ScrollMode);
    case ReaderMode::Rsvp:
    default:
      return uiText(UiText::RsvpMode);
  }
}

String App::handednessLabel() const {
  return handednessMode_ == HandednessMode::Left ? "Left" : "Right";
}

String App::readerFontSizeLabel() const {
  uint8_t levelIndex = readerFontSizeIndex_;
  if (levelIndex >= kReaderFontSizeCount) {
    levelIndex = 0;
  }

  switch (levelIndex) {
    case 0:
      return uiText(UiText::Large);
    case 1:
      return uiText(UiText::Medium);
    case 2:
    default:
      return uiText(UiText::Small);
  }
}

String App::readerTypefaceLabel() const {
  switch (typographyConfig_.typeface) {
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible:
      return "Atkinson";
    case DisplayManager::ReaderTypeface::OpenDyslexic:
      return "OpenDyslexic";
    case DisplayManager::ReaderTypeface::Standard:
    default:
      return uiText(UiText::Standard);
  }
}

String App::typographyTuningLabel() const {
  switch (typographyTuningSelectedIndex_) {
    case TypographyTuningBack:
      return uiText(UiText::Back);
    case TypographyTuningFontSize:
      return uiText(UiText::FontSize);
    case TypographyTuningTypeface:
      return uiText(UiText::Typeface);
    case TypographyTuningPhantomWords:
      return uiText(UiText::PhantomWords);
    case TypographyTuningFocusHighlight:
      return uiText(UiText::RedHighlight);
    case TypographyTuningTracking:
      return uiText(UiText::Tracking);
    case TypographyTuningAnchor:
      return uiText(UiText::Anchor);
    case TypographyTuningGuideWidth:
      return uiText(UiText::GuideWidth);
    case TypographyTuningGuideGap:
      return uiText(UiText::GuideGap);
    case TypographyTuningReset:
      return uiText(UiText::Reset);
    default:
      return uiText(UiText::Typography);
  }
}

String App::typographyTuningValueLabel() const {
  switch (typographyTuningSelectedIndex_) {
    case TypographyTuningBack:
      return uiText(UiText::TapToExit);
    case TypographyTuningFontSize:
      return readerFontSizeLabel();
    case TypographyTuningTypeface:
      return readerTypefaceLabel();
    case TypographyTuningPhantomWords:
      return phantomWordsLabel();
    case TypographyTuningFocusHighlight:
      return focusHighlightLabel();
    case TypographyTuningTracking:
      return String(typographyConfig_.trackingPx >= 0 ? "+" : "") +
             String(static_cast<int>(typographyConfig_.trackingPx)) + " px";
    case TypographyTuningAnchor:
      return String(static_cast<unsigned int>(effectiveAnchorPercent())) + "%";
    case TypographyTuningGuideWidth:
      return String(static_cast<unsigned int>(typographyConfig_.guideHalfWidth)) + " px";
    case TypographyTuningGuideGap:
      return String(static_cast<unsigned int>(typographyConfig_.guideGap)) + " px";
    case TypographyTuningReset:
      return uiText(UiText::TapToReset);
    default:
      return "";
  }
}

void App::openBookPicker() {
  storage_.refreshBooks();
  bookMenuItems_.clear();
  bookPickerBookIndices_.clear();
  bookMenuItems_.push_back({uiText(UiText::Back), ""});

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
    bookMenuItems_.push_back(libraryItemForBook(bookIndex));
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
  chapterMenuItems_.push_back(uiText(UiText::Back));

  if (chapterMarkers_.empty()) {
    chapterMenuItems_.push_back(uiText(UiText::StartOfBook));
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

  chapterMenuItems_.push_back(uiText(UiText::RestartBook));

  menuScreen_ = MenuScreen::ChapterPicker;
  renderChapterPicker();
}

void App::selectChapterPickerItem(uint32_t nowMs) {
  if (chapterPickerSelectedIndex_ == kChapterPickerBackIndex || chapterMenuItems_.size() <= 1) {
    menuScreen_ = MenuScreen::Main;
    renderMainMenu();
    return;
  }

  const size_t restartIndex = chapterMenuItems_.size() - 1;
  if (chapterPickerSelectedIndex_ == restartIndex) {
    openRestartConfirm();
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

void App::openRestartConfirm() {
  restartConfirmReturnScreen_ = menuScreen_;
  restartConfirmSelectedIndex_ = RestartConfirmNo;
  menuScreen_ = MenuScreen::RestartConfirm;
  renderRestartConfirm();
}

void App::selectRestartConfirmItem(uint32_t nowMs) {
  if (restartConfirmSelectedIndex_ != RestartConfirmYes) {
    menuScreen_ = restartConfirmReturnScreen_;
    renderMenu();
    return;
  }

  reader_.begin(nowMs);
  restartConfirmReturnScreen_ = MenuScreen::Main;
  menuScreen_ = MenuScreen::Main;
  setState(AppState::Paused, nowMs);
  saveReadingPosition(true);
  Serial.println("[restart] book restarted from beginning");
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

void App::enterPowerOff(uint32_t nowMs) {
  if (powerOffStarted_) {
    return;
  }

  powerOffStarted_ = true;
  Serial.println("[app] powering off; hold PWR to start again");
  saveReadingPosition(true);
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  touchPlayHeld_ = false;
  contextViewVisible_ = false;
  wpmFeedbackVisible_ = false;
  menuScreen_ = MenuScreen::Main;
  state_ = AppState::Sleeping;

  display_.renderStatus("OFF", "Release PWR", "Hold PWR to start");
  delay(300);
  display_.prepareForSleep();

  storage_.end();
  touch_.end();
  touchInitialized_ = false;
  Serial.flush();

  BoardConfig::holdBacklightOffForDeepSleep();
  BoardConfig::releaseBatteryPowerHold();

  const uint32_t waitStartMs = millis();
  while (powerButton_.isHeld() && millis() - waitStartMs < kPowerOffReleaseWaitMs) {
    powerButton_.update(millis());
    delay(10);
  }

  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(BoardConfig::PIN_PWR_BUTTON), 0);
  esp_deep_sleep_start();
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
  powerButton_.begin();
  bootButtonReleasedSinceBoot_ = !button_.isHeld();
  bootButtonLongPressHandled_ = false;
  powerButtonReleasedSinceBoot_ = !powerButton_.isHeld();
  powerButtonLongPressHandled_ = false;
  powerOffStarted_ = false;
  updateBatteryStatus(nowMs, true);
  storage_.setStatusCallback(&App::handleStorageStatus, this);
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;
  menuScreen_ = MenuScreen::Main;
  lastStateLogMs_ = nowMs;
  state_ = AppState::Paused;

  const bool displayReady = display_.wakeFromSleep();
  if (displayReady) {
    renderActiveReader(nowMs);
  }

  touchInitialized_ = touch_.begin();
  const bool storageReady = storage_.begin();
  if (storageReady) {
    storage_.listBooks();
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
  paragraphStarts_ = std::move(book.paragraphStarts);
  reader_.setWords(std::move(book.words), nowMs);
  invalidateContextPreviewWindow();
  rebuildTimeEstimateCache();
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
  Serial.printf("[app] loaded SD book[%u/%u]: %s (%u chapters, %u paragraphs)\n",
                static_cast<unsigned int>(loadedIndex + 1),
                static_cast<unsigned int>(storage_.bookCount()), loadedPath.c_str(),
                static_cast<unsigned int>(chapterMarkers_.size()),
                static_cast<unsigned int>(paragraphStarts_.size()));
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
  if (!isFocusTimerMenuScreen(menuScreen_)) {
    applyReaderUiOrientation();
  }

  if (menuScreen_ == MenuScreen::SettingsHome || menuScreen_ == MenuScreen::SettingsDisplay ||
      menuScreen_ == MenuScreen::SettingsPacing || menuScreen_ == MenuScreen::WifiSettings) {
    renderSettings();
  } else if (menuScreen_ == MenuScreen::WifiNetworks) {
    renderWifiNetworks();
  } else if (menuScreen_ == MenuScreen::TextEntry) {
    renderTextEntry();
  } else if (menuScreen_ == MenuScreen::TypographyTuning) {
    renderTypographyTuning();
  } else if (menuScreen_ == MenuScreen::BookPicker) {
    renderBookPicker();
  } else if (menuScreen_ == MenuScreen::ChapterPicker) {
    renderChapterPicker();
  } else if (menuScreen_ == MenuScreen::RestartConfirm) {
    renderRestartConfirm();
  } else if (menuScreen_ == MenuScreen::FocusTimerGenres) {
    renderFocusTimerGenres();
  } else if (menuScreen_ == MenuScreen::FocusTimerSession) {
    renderFocusTimerSession();
  } else {
    renderMainMenu();
  }
}

void App::renderMainMenu() {
  std::vector<String> items;
  items.reserve(MenuItemCount);
  items.push_back(uiText(UiText::Resume));
  items.push_back(uiText(UiText::Chapters));
  items.push_back(uiText(UiText::Library));
  items.push_back("Focus Timer");
  items.push_back(uiText(UiText::Settings));
#if RSVP_USB_TRANSFER_ENABLED
  items.push_back(uiText(UiText::UsbTransfer));
#endif
  items.push_back(uiText(UiText::PowerOff));
  display_.renderMenu(items, menuSelectedIndex_);
}

void App::renderSettings() {
  if (settingsMenuItems_.empty()) {
    rebuildSettingsMenuItems();
  }
  display_.renderMenu(settingsMenuItems_, settingsSelectedIndex_);
}

void App::renderTypographyTuning() {
  if (kTypographyPreviewWordCount == 0) {
    display_.renderStatus(uiText(UiText::Typography), uiText(UiText::NoSamples), "");
    return;
  }

  if (typographyPreviewSampleIndex_ >= kTypographyPreviewWordCount) {
    typographyPreviewSampleIndex_ = 0;
  }
  if (typographyTuningSelectedIndex_ >= TypographyTuningItemCount) {
    typographyTuningSelectedIndex_ = TypographyTuningFontSize;
  }

  const size_t index = typographyPreviewSampleIndex_;
  const size_t beforeIndex =
      index == 0 ? kTypographyPreviewWordCount - 1 : index - 1;
  const size_t afterIndex =
      (index + 1 >= kTypographyPreviewWordCount) ? 0 : index + 1;
  const String beforeText = phantomWordsEnabled_ ? kTypographyPreviewWords[beforeIndex] : "";
  const String afterText = phantomWordsEnabled_ ? kTypographyPreviewWords[afterIndex] : "";
  const String line1 = typographyTuningLabel() + ": " + typographyTuningValueLabel();
  const String title =
      uiText(UiText::Typography) + " " + String(static_cast<unsigned int>(index + 1)) + "/" +
      String(static_cast<unsigned int>(kTypographyPreviewWordCount));
  String line2 = uiText(UiText::TapChangeSample);
  if (typographyTuningSelectedIndex_ == TypographyTuningBack) {
    line2 = uiText(UiText::TapExitSample);
  } else if (typographyTuningSelectedIndex_ == TypographyTuningPhantomWords ||
             typographyTuningSelectedIndex_ == TypographyTuningFocusHighlight) {
    line2 = uiText(UiText::TapToggleSample);
  } else if (typographyTuningSelectedIndex_ == TypographyTuningFontSize ||
             typographyTuningSelectedIndex_ == TypographyTuningTypeface) {
    line2 = uiText(UiText::TapCycleSample);
  } else if (typographyTuningSelectedIndex_ == TypographyTuningReset) {
    line2 = uiText(UiText::TapToReset);
  }

  display_.renderTypographyPreview(beforeText,
                                   kTypographyPreviewWords[index],
                                   afterText,
                                   readerFontSizeIndex_, title, line1, line2);
}

void App::renderBookPicker() {
  display_.renderLibrary(bookMenuItems_, bookPickerSelectedIndex_);
}

void App::renderChapterPicker() {
  display_.renderMenu(chapterMenuItems_, chapterPickerSelectedIndex_);
}

void App::renderRestartConfirm() {
  std::vector<String> items;
  items.reserve(RestartConfirmItemCount);
  items.push_back(uiText(UiText::AreYouSure));
  items.push_back(uiText(UiText::NoKeepPlace));
  items.push_back(uiText(UiText::YesRestart));

  display_.renderMenu(items, restartConfirmSelectedIndex_ + kRestartConfirmHeaderRows);
}

void App::renderFocusTimerGenres() {
  applyReaderUiOrientation();
  if (focusTimerGenreMenuItems_.empty()) {
    rebuildFocusTimerGenreMenuItems();
  }
  display_.renderMenu(focusTimerGenreMenuItems_, focusTimerGenreSelectedIndex_);
}

void App::renderFocusTimerSession() {
  applyUiOrientation(focusTimer_.uiOrientation());
  const String remainingLabel = formatFocusTimerRemaining(millis());

  switch (focusTimer_.state()) {
    case FocusTimer::State::Unavailable:
      display_.renderFocusTimerScreen("TIMER", "", "", "IMU unavailable");
      return;
    case FocusTimer::State::GenreSelect:
      renderFocusTimerGenres();
      return;
    case FocusTimer::State::WaitForTouchStart:
      display_.renderFocusTimerScreen("BEGIN", "", "", "Place on short side");
      return;
    case FocusTimer::State::TouchRunning:
      display_.renderFocusTimerScreen("BEGIN", "", remainingLabel, "",
                                      "", focusTimer_.progressPercent(millis()));
      return;
    case FocusTimer::State::WaitAfterTouch:
      display_.renderFocusTimerScreen("WORK", "", "", "Flip to continue");
      return;
    case FocusTimer::State::WorkRunning:
      display_.renderFocusTimerScreen("WORK", "", remainingLabel, "",
                                      "", focusTimer_.progressPercent(millis()));
      return;
    case FocusTimer::State::BreakRunning:
      display_.renderFocusTimerScreen("BREAK", "", remainingLabel, "",
                                      "", focusTimer_.progressPercent(millis()), true);
      return;
    case FocusTimer::State::WaitAfterWork:
      display_.renderFocusTimerScreen("BREAK", "", "", "Turn for break", "",
                                      -1, true);
      return;
    case FocusTimer::State::WaitAfterBreak:
      display_.renderFocusTimerScreen("WORK", "", "", "Flip to begin");
      return;
    case FocusTimer::State::Cancelled:
      display_.renderFocusTimerScreen("BEGIN", "", "", "Place to begin again");
      return;
    case FocusTimer::State::Complete:
      display_.renderFocusTimerScreen("DONE", "", "", "Session complete");
      return;
  }
}

DisplayManager::LibraryItem App::libraryItemForBook(size_t bookIndex) {
  DisplayManager::LibraryItem item;
  item.title = storage_.bookDisplayName(bookIndex);
  item.subtitle = storage_.bookAuthorName(bookIndex);

  uint8_t percent = 0;
  const bool hasProgress = bookProgressPercent(bookIndex, percent);
  if (hasProgress) {
    if (!item.subtitle.isEmpty()) {
      item.subtitle += " - ";
    }
    item.subtitle += String(percent) + "%";
  }

  if (item.subtitle.isEmpty() && usingStorageBook_ && bookIndex == currentBookIndex_) {
    item.subtitle = uiText(UiText::CurrentBook);
  }

  return item;
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

size_t App::currentChapterIndex() const {
  if (chapterMarkers_.empty()) {
    return static_cast<size_t>(-1);
  }

  size_t currentChapter = 0;
  const size_t currentIndex = reader_.currentIndex();
  for (size_t i = 0; i < chapterMarkers_.size(); ++i) {
    if (chapterMarkers_[i].wordIndex <= currentIndex) {
      currentChapter = i;
    }
  }

  return currentChapter;
}

String App::currentChapterLabel() const {
  const size_t chapterIndex = currentChapterIndex();
  if (chapterIndex >= chapterMarkers_.size()) {
    return currentBookTitle_.isEmpty() ? uiText(UiText::Start) : currentBookTitle_;
  }

  return chapterMarkers_[chapterIndex].title;
}

String App::currentFooterMetricLabel() const {
  if (footerMetricMode_ == FooterMetricMode::Percentage) {
    return String(readingProgressPercent()) + "%";
  }

  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    return "0%";
  }

  const size_t currentIndex = std::min(reader_.currentIndex(), wordCount - 1);
  size_t endIndex = wordCount;

  if (footerMetricMode_ == FooterMetricMode::ChapterTime) {
    const size_t chapterIndex = currentChapterIndex();
    if (chapterIndex < chapterMarkers_.size() && chapterIndex + 1 < chapterMarkers_.size()) {
      endIndex = chapterMarkers_[chapterIndex + 1].wordIndex;
    }
    return String("CH ") +
           formatReadingTimeRemaining(estimatedReadingTimeRemainingMs(currentIndex, endIndex));
  }

  return String("BOOK ") +
         formatReadingTimeRemaining(estimatedReadingTimeRemainingMs(currentIndex, endIndex));
}

uint32_t App::estimatedReadingTimeRemainingMs(size_t startIndex, size_t endIndex) const {
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0 || reader_.wpm() == 0) {
    return 0;
  }

  startIndex = std::min(startIndex, wordCount);
  endIndex = std::min(endIndex, wordCount);
  if (endIndex <= startIndex) {
    return 0;
  }

  const uint32_t baseMs = static_cast<uint32_t>(
      (static_cast<uint64_t>(endIndex - startIndex) * 60000ULL) /
      static_cast<uint64_t>(reader_.wpm()));

  if (!accurateTimeEstimateEnabled_ || !timeEstimateCacheValid_) {
    return baseMs;
  }

  return baseMs + wordBonusPrefixSumMs_[endIndex] - wordBonusPrefixSumMs_[startIndex];
}

void App::invalidateTimeEstimateCache() {
  timeEstimateCacheValid_ = false;
  std::vector<uint32_t>().swap(wordBonusPrefixSumMs_);
}

void App::rebuildTimeEstimateCache() {
  invalidateTimeEstimateCache();
  pacingCacheDirty_ = false;
  if (!accurateTimeEstimateEnabled_) {
    return;
  }

  const size_t n = reader_.wordCount();
  if (n == 0) {
    return;
  }

  wordBonusPrefixSumMs_.assign(n + 1, 0);
  uint32_t running = 0;
  const uint32_t startMs = millis();
  for (size_t i = 0; i < n; ++i) {
    wordBonusPrefixSumMs_[i] = running;
    running += reader_.wordPacingBonusMsAt(i);
  }
  wordBonusPrefixSumMs_[n] = running;
  timeEstimateCacheValid_ = true;

  Serial.printf("[time-est] cached %u words bonus=%lums took=%lums\n",
                static_cast<unsigned int>(n), static_cast<unsigned long>(running),
                static_cast<unsigned long>(millis() - startMs));
}

String App::timeEstimateModeLabel() const {
  return uiText(accurateTimeEstimateEnabled_ ? UiText::TimeEstimateAccurate
                                             : UiText::TimeEstimateFast);
}

String App::formatReadingTimeRemaining(uint32_t remainingMs) const {
  const uint32_t totalSeconds = remainingMs / 1000UL;
  if (totalSeconds < 60UL) {
    return "<1m";
  }

  const uint32_t totalMinutes = totalSeconds / 60UL;
  if (totalMinutes < 60UL) {
    return String(totalMinutes) + "m";
  }

  const uint32_t totalHours = totalMinutes / 60UL;
  const uint32_t minutes = totalMinutes % 60UL;
  if (totalHours < 24UL) {
    if (minutes == 0) {
      return String(totalHours) + "h";
    }
    return String(totalHours) + "h" + String(minutes) + "m";
  }

  const uint32_t days = totalHours / 24UL;
  const uint32_t hours = totalHours % 24UL;
  if (hours == 0) {
    return String(days) + "d";
  }
  return String(days) + "d" + String(hours) + "h";
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

bool App::isFocusTimerMenuScreen(MenuScreen screen) const {
  return screen == MenuScreen::FocusTimerGenres || screen == MenuScreen::FocusTimerSession;
}

void App::applyUiOrientation(BoardConfig::UiOrientation orientation) {
  touch_.setUiOrientation(orientation);
  display_.setUiOrientation(orientation);
}

void App::applyReaderUiOrientation() {
  applyUiOrientation(readerUiOrientation());
}

BoardConfig::UiOrientation App::readerUiOrientation() const {
  return uiRotated180() ? BoardConfig::UiOrientation::LandscapeFlipped
                        : BoardConfig::UiOrientation::Landscape;
}

String App::formatFocusTimerRemaining(uint32_t nowMs) const {
  const uint32_t remainingMs = focusTimer_.remainingMs(nowMs);
  const uint32_t totalSeconds = remainingMs / 1000UL;
  const uint32_t minutes = totalSeconds / 60UL;
  const uint32_t seconds = totalSeconds % 60UL;
  char buffer[8];
  std::snprintf(buffer, sizeof(buffer), "%02lu:%02lu",
                static_cast<unsigned long>(minutes),
                static_cast<unsigned long>(seconds));
  return String(buffer);
}

String App::focusTimerCountsLabel() const {
  return "T" + String(focusTimer_.completedTouchBlocks()) + " W" +
         String(focusTimer_.completedWorkBlocks()) + " B" +
         String(focusTimer_.completedBreakBlocks());
}

void App::playFocusTimerCompletionCue() {
  if (audio_.beep()) {
    return;
  }

  for (int i = 0; i < 3; ++i) {
    digitalWrite(BoardConfig::PIN_LCD_BACKLIGHT, HIGH);
    delay(55);
    digitalWrite(BoardConfig::PIN_LCD_BACKLIGHT, LOW);
    delay(45);
  }
}

bool App::scrollModeEnabled() const { return readerMode_ == ReaderMode::Scroll; }

bool App::uiRotated180() const {
  return handednessMode_ == HandednessMode::Right ? BoardConfig::UI_ROTATED_180
                                                  : !BoardConfig::UI_ROTATED_180;
}

uint8_t App::effectiveAnchorPercent() const {
  return handednessMode_ == HandednessMode::Left
             ? static_cast<uint8_t>(typographyConfig_.anchorPercent + kLeftHandAnchorOffset)
             : typographyConfig_.anchorPercent;
}

DisplayManager::TypographyConfig App::effectiveTypographyConfig() const {
  DisplayManager::TypographyConfig config = typographyConfig_;
  config.anchorPercent = effectiveAnchorPercent();
  return config;
}

uint32_t App::currentReaderContentToken() const {
  return hashBookPath(currentBookPath_.isEmpty() ? String("__demo__") : currentBookPath_);
}

size_t App::phantomBeforeCharTarget() const {
  uint8_t levelIndex = readerFontSizeIndex_;
  if (levelIndex >= kReaderFontSizeCount) {
    levelIndex = 0;
  }
  return kPhantomBeforeCharTargets[levelIndex];
}

size_t App::phantomAfterCharTarget() const {
  uint8_t levelIndex = readerFontSizeIndex_;
  if (levelIndex >= kReaderFontSizeCount) {
    levelIndex = 0;
  }
  return kPhantomAfterCharTargets[levelIndex];
}

String App::collectPhantomBeforeText(size_t currentIndex, size_t charTarget) const {
  if (currentIndex == 0 || charTarget == 0) {
    return "";
  }

  size_t startIndex = currentIndex;
  size_t totalChars = 0;
  while (startIndex > 0 && totalChars < charTarget) {
    --startIndex;
    const String word = reader_.wordAt(startIndex);
    totalChars += word.length();
    if (startIndex + 1 < currentIndex) {
      ++totalChars;
    }
  }

  String text;
  for (size_t index = startIndex; index < currentIndex; ++index) {
    if (!text.isEmpty()) {
      text += ' ';
    }
    text += reader_.wordAt(index);
  }
  return text;
}

String App::collectPhantomAfterText(size_t currentIndex, size_t charTarget) const {
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0 || currentIndex + 1 >= wordCount || charTarget == 0) {
    return "";
  }

  size_t endIndex = currentIndex + 1;
  size_t totalChars = 0;
  while (endIndex < wordCount && totalChars < charTarget) {
    const String word = reader_.wordAt(endIndex);
    totalChars += word.length();
    if (endIndex > currentIndex + 1) {
      ++totalChars;
    }
    ++endIndex;
  }

  String text;
  for (size_t index = currentIndex + 1; index < endIndex; ++index) {
    if (!text.isEmpty()) {
      text += ' ';
    }
    text += reader_.wordAt(index);
  }
  return text;
}

String App::phantomBeforeText() const {
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    return "";
  }

  const size_t currentIndex = std::min(reader_.currentIndex(), wordCount - 1);
  return collectPhantomBeforeText(currentIndex, phantomBeforeCharTarget());
}

String App::phantomAfterText() const {
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    return "";
  }

  const size_t currentIndex = std::min(reader_.currentIndex(), wordCount - 1);
  return collectPhantomAfterText(currentIndex, phantomAfterCharTarget());
}

void App::renderActiveReader(uint32_t nowMs) {
  applyReaderUiOrientation();
  if (scrollModeEnabled()) {
    if (wpmFeedbackVisible_) {
      renderScrollReader(nowMs, String(reader_.wpm()) + " WPM");
    } else {
      renderScrollReader(nowMs);
    }
    return;
  }

  if (contextViewVisible_) {
    renderContextPreview();
  } else if (wpmFeedbackVisible_) {
    renderWpmFeedback(nowMs);
  } else {
    renderReaderWord();
  }
}

void App::renderReaderWord() {
  applyReaderUiOrientation();
  contextViewVisible_ = false;
  const bool showFooter = state_ != AppState::Playing;
  const String beforeText = phantomWordsEnabled_ ? phantomBeforeText() : "";
  const String afterText = phantomWordsEnabled_ ? phantomAfterText() : "";
  const String footerMetricLabel = currentFooterMetricLabel();
  display_.renderPhantomRsvpWord(beforeText, reader_.currentWord(), afterText,
                                 readerFontSizeIndex_, currentChapterLabel(),
                                 readingProgressPercent(), showFooter, footerMetricLabel);
}

bool App::isParagraphStart(size_t wordIndex) const {
  if (wordIndex == 0) {
    return true;
  }

  return std::binary_search(paragraphStarts_.begin(), paragraphStarts_.end(), wordIndex);
}

size_t App::paragraphStartAtOrBefore(size_t wordIndex) const {
  if (wordIndex == 0 || paragraphStarts_.empty()) {
    return 0;
  }

  const auto it = std::upper_bound(paragraphStarts_.begin(), paragraphStarts_.end(), wordIndex);
  if (it == paragraphStarts_.begin()) {
    return 0;
  }

  return *std::prev(it);
}

size_t App::contextPreviewAnchorIndex(size_t currentIndex) const {
  if (currentIndex <= kContextPreviewAnchorLeadWords) {
    return 0;
  }

  const size_t anchorTarget = currentIndex - kContextPreviewAnchorLeadWords;
  const size_t paragraphStart = paragraphStartAtOrBefore(anchorTarget);
  if (anchorTarget - paragraphStart <= kContextPreviewMaxParagraphSnapWords) {
    return paragraphStart;
  }

  return anchorTarget;
}

void App::updateContextPreviewWindow(size_t currentIndex) {
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    contextPreviewWords_.clear();
    contextPreviewWindowValid_ = false;
    contextPreviewCurrentLocalIndex_ = static_cast<size_t>(-1);
    return;
  }

  size_t startIndex = contextPreviewStartIndex_;
  size_t endIndex = 0;
  bool rebuildWindow = !contextPreviewWindowValid_ || contextPreviewWords_.empty();
  if (!rebuildWindow) {
    endIndex = std::min(wordCount, startIndex + kContextPreviewWindowWords);
    rebuildWindow = currentIndex < startIndex || currentIndex >= endIndex ||
                    (currentIndex + 1 >= endIndex && endIndex < wordCount);
  }

  if (rebuildWindow) {
    startIndex = contextPreviewAnchorIndex(currentIndex);
    endIndex = std::min(wordCount, startIndex + kContextPreviewWindowWords);
    contextPreviewStartIndex_ = startIndex;
    contextPreviewWindowValid_ = true;
    contextPreviewWords_.clear();
    contextPreviewWords_.reserve(endIndex - startIndex);
    for (size_t index = startIndex; index < endIndex; ++index) {
      DisplayManager::ContextWord word;
      word.text = reader_.wordAt(index);
      word.paragraphStart = isParagraphStart(index);
      word.current = index == currentIndex;
      contextPreviewWords_.push_back(word);
    }
    contextPreviewCurrentLocalIndex_ =
        currentIndex >= startIndex ? currentIndex - startIndex : static_cast<size_t>(-1);
    return;
  }

  const size_t nextLocalIndex = currentIndex - startIndex;
  if (contextPreviewCurrentLocalIndex_ < contextPreviewWords_.size()) {
    contextPreviewWords_[contextPreviewCurrentLocalIndex_].current = false;
  }
  if (nextLocalIndex < contextPreviewWords_.size()) {
    contextPreviewWords_[nextLocalIndex].current = true;
    contextPreviewCurrentLocalIndex_ = nextLocalIndex;
  } else {
    contextPreviewCurrentLocalIndex_ = static_cast<size_t>(-1);
  }
}

void App::invalidateContextPreviewWindow() {
  contextPreviewWindowValid_ = false;
  contextPreviewWords_.clear();
  contextPreviewCurrentLocalIndex_ = static_cast<size_t>(-1);
}

void App::renderContextPreview() {
  applyReaderUiOrientation();
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    renderReaderWord();
    return;
  }

  const size_t currentIndex = std::min(reader_.currentIndex(), wordCount - 1);
  updateContextPreviewWindow(currentIndex);

  contextViewVisible_ = true;
  display_.renderScrollView(contextPreviewWords_, currentReaderContentToken(),
                            contextPreviewStartIndex_, currentIndex, 0,
                            currentChapterLabel(), readingProgressPercent(), "",
                            currentFooterMetricLabel());
}

void App::renderScrollReader(uint32_t nowMs, const String &overlayText) {
  applyReaderUiOrientation();
  contextViewVisible_ = false;
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    renderReaderWord();
    return;
  }

  const size_t currentIndex = std::min(reader_.currentIndex(), wordCount - 1);
  updateContextPreviewWindow(currentIndex);

  uint16_t scrollProgressPermille = 0;
  if (state_ == AppState::Playing && currentIndex + 1 < wordCount) {
    const uint32_t durationMs = reader_.currentWordDurationMs();
    if (durationMs > 0) {
      const uint32_t elapsedMs = reader_.elapsedInCurrentWordMs(nowMs);
      scrollProgressPermille = static_cast<uint16_t>(
          std::min<uint32_t>(1000UL, (elapsedMs * 1000UL) / durationMs));
    }
  }

  display_.renderScrollView(contextPreviewWords_, currentReaderContentToken(),
                            contextPreviewStartIndex_, currentIndex, scrollProgressPermille,
                            currentChapterLabel(), readingProgressPercent(), overlayText,
                            currentFooterMetricLabel());
}

void App::renderWpmFeedback(uint32_t nowMs) {
  applyReaderUiOrientation();
  wpmFeedbackVisible_ = true;
  wpmFeedbackUntilMs_ = nowMs + kWpmFeedbackMs;
  if (scrollModeEnabled()) {
    renderScrollReader(nowMs, String(reader_.wpm()) + " WPM");
    return;
  }

  contextViewVisible_ = false;
  const String beforeText = phantomWordsEnabled_ ? phantomBeforeText() : "";
  const String afterText = phantomWordsEnabled_ ? phantomAfterText() : "";
  const String footerMetricLabel = currentFooterMetricLabel();
  display_.renderPhantomRsvpWordWithWpm(beforeText, reader_.currentWord(), afterText,
                                        readerFontSizeIndex_, reader_.wpm(),
                                        currentChapterLabel(), readingProgressPercent(), true,
                                        footerMetricLabel);
}

void App::renderStorageStatus(const char *title, const char *line1, const char *line2,
                              int progressPercent) {
  applyReaderUiOrientation();
  display_.renderProgress(title == nullptr ? "SD" : title, line1 == nullptr ? "" : line1,
                          line2 == nullptr ? "" : line2, progressPercent);
}

void App::handleStorageStatus(void *context, const char *title, const char *line1,
                              const char *line2, int progressPercent) {
  if (context == nullptr) {
    return;
  }

  static_cast<App *>(context)->renderStorageStatus(title, line1, line2, progressPercent);
  delay(0);
}
