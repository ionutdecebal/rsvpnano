#include "app/App.h"

#include <WiFi.h>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <esp_log.h>
#include <iterator>
#include <utility>
#include <vector>

#include "app/MenuRepeat.h"
#include "board/BoardConfig.h"
#include "settings/PreferenceKeys.h"

#ifndef RSVP_USB_TRANSFER_ENABLED
#define RSVP_USB_TRANSFER_ENABLED 0
#endif

#ifndef RSVP_USB_TRANSFER_AUTO_START
#define RSVP_USB_TRANSFER_AUTO_START 0
#endif

static const char *kAppTag = "app";
constexpr uint32_t kOtaCheckTaskStackBytes = 10240;
constexpr uint32_t kBootSplashMs = 750;
constexpr uint32_t kWpmFeedbackMs = 900;
constexpr uint32_t kBrightnessToastMs = 1500;
constexpr uint32_t kPowerOffHoldMs = 1600;
constexpr uint32_t kPowerOffReleaseWaitMs = 4000;
constexpr uint32_t kSoftOffReleaseWaitMs = 1200;
constexpr uint32_t kSoftOffWakePollMs = 15;
constexpr uint32_t kSoftOffArmQuietMs = 250;
constexpr uint32_t kSoftOffWakeConfirmMs = Board::Config::SOFT_OFF_WAKE_CONFIRM_MS;
constexpr uint32_t kBatterySampleIntervalMs = 180000;
constexpr uint32_t kPreviewBrowseHoldMs = 240;
constexpr uint32_t kThemeToggleHoldMs = 900;
constexpr uint32_t kScrollAnimationFrameMs = 16;
constexpr uint16_t kSwipeThresholdPx = 40;
constexpr uint16_t kAxisBiasPx = 12;
constexpr uint16_t kTapSlopPx = 26;
constexpr uint16_t kPreviousSentenceTapWidthPx = 96;
constexpr uint16_t kPreviousSentenceTapHeightPx = 60;
constexpr uint16_t kFooterMetricTapWidthPx = 220;
constexpr uint16_t kFooterMetricTapHeightPx = 32;
constexpr uint16_t kBatteryBadgeTapWidthPx = 160;
constexpr uint16_t kBatteryBadgeTapHeightPx = 40;
constexpr uint16_t kReaderChromeMarginXPx = Board::Config::READER_CHROME_MARGIN_X;
constexpr uint16_t kReaderChromeTopMarginPx = Board::Config::READER_CHROME_MARGIN_TOP;
constexpr uint16_t kReaderChromeBottomMarginPx = Board::Config::READER_CHROME_MARGIN_BOTTOM;
constexpr uint16_t kReaderBatteryMarginXPx = Board::Config::READER_BATTERY_MARGIN_X;
constexpr uint16_t kReaderBatteryTopMarginPx = Board::Config::READER_BATTERY_MARGIN_TOP;
constexpr uint16_t kMenuSwipeTopZonePx =
    kReaderChromeTopMarginPx + (Board::Config::ENABLE_TOP_EDGE_MENU_SWIPE ? 32 : 0);
constexpr uint16_t kMenuSwipeCenterHalfWidthPx = Board::Config::DISPLAY_WIDTH / 5;
constexpr uint16_t kQuickSettingsSwipeBottomZonePx =
    kReaderChromeBottomMarginPx + (Board::Config::ENABLE_BOTTOM_EDGE_QUICK_SETTINGS_SWIPE ? 32 : 0);
constexpr uint16_t kMenuSwipeTriggerPx = 72;
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
constexpr size_t kTimeEstimateBlockWords = 256;
constexpr size_t kTimeEstimateBlocksPerUpdate = 1;
constexpr uint32_t kTimeEstimateProgressLogMs = 5000;
constexpr uint32_t kNominalBatteryRuntimeMinutes = 450;
constexpr uint8_t kBatteryDisplayHysteresisPercent = 2;
constexpr uint8_t kBatteryRuntimeMinDropPercent = 3;
constexpr uint32_t kBatteryRuntimeMinElapsedMs = 10UL * 60UL * 1000UL;
constexpr uint32_t kBatteryLabelRefreshIntervalMs = 60UL * 1000UL;
constexpr uint32_t kBatteryPlayingSampleIntervalMs = 10UL * 60UL * 1000UL;
constexpr uint32_t kBatteryLowSampleIntervalMs = 60UL * 1000UL;
constexpr uint32_t kBatteryLowWarningRepeatMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kBatteryWarningVisibleMs = 2500;
constexpr uint32_t kBatteryShutdownNoticeMs = 1500;
constexpr float kBatteryLowWarningVoltage = 3.50f;
constexpr float kBatteryCriticalVoltage = 3.30f;
constexpr uint8_t kBatteryLowWarningPercent = 5;
constexpr uint8_t kBatteryCriticalPercent = 1;
constexpr uint8_t kBatteryCriticalConsecutiveSamples = 2;
constexpr uint32_t kStandbyWakeGraceMs = 900;
constexpr uint32_t kStandbyFrameMs = 160;
constexpr uint16_t kStandbyLifeCellPixels = 2;
constexpr uint16_t kStandbyLifeColumns = Board::Config::DISPLAY_WIDTH / kStandbyLifeCellPixels;
constexpr uint16_t kStandbyLifeRows = Board::Config::DISPLAY_HEIGHT / kStandbyLifeCellPixels;
constexpr uint32_t kChapterTransitionMs = 1400;
constexpr uint8_t kBrightnessLevels[] = {40, 55, 70, 85, 100};
constexpr uint8_t kNightBrightnessLevels[] = {35, 40, 45, 50, 55};
constexpr size_t kBrightnessLevelCount = sizeof(kBrightnessLevels) / sizeof(kBrightnessLevels[0]);

namespace {

enum MenuItem : size_t {
  MenuResume,
  MenuChapters,
  MenuBooks,
  MenuArticles,
  MenuFocusTimer,
  MenuSettings,
  MenuSdCardCheck,
  MenuRssFeeds,
  MenuCompanionSync,
#if RSVP_USB_TRANSFER_ENABLED
  MenuUsbTransfer,
#endif
  MenuPowerOff,
  MenuItemCount,
};

enum RestructuredMenuItem : size_t {
  RestructuredMenuResume,
  RestructuredMenuChapters,
  RestructuredMenuBooks,
  RestructuredMenuArticles,
  RestructuredMenuSettings,
  RestructuredMenuPowerOff,
  RestructuredMenuItemCount,
};

enum ArticlesItem : size_t {
  ArticlesBack,
  ArticlesBrowse,
  ArticlesUpdateRss,
  ArticlesItemCount,
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

enum SdCardRepairConfirmItem : size_t {
  SdCardRepairConfirmNo,
  SdCardRepairConfirmYes,
  SdCardRepairConfirmItemCount,
};

enum UpdateConfirmItem : size_t {
  UpdateConfirmSkip,
  UpdateConfirmUpdate,
  UpdateConfirmItemCount,
};

enum PowerOffConfirmItem : size_t {
  PowerOffConfirmNo,
  PowerOffConfirmYes,
  PowerOffConfirmItemCount,
};

enum QuickSettingsItem : size_t {
  QuickSettingsBrightness,
  QuickSettingsTheme,
  QuickSettingsFocusTimer,
  QuickSettingsSync,
  QuickSettingsItemCount,
};

enum QuickSyncItem : size_t {
  QuickSyncWifi,
  QuickSyncUsb,
  QuickSyncItemCount,
};

constexpr size_t kRestartConfirmHeaderRows = 1;
constexpr size_t kSdCardRepairConfirmHeaderRows = 1;
constexpr size_t kUpdateConfirmHeaderRows = 2;
constexpr size_t kPowerOffConfirmHeaderRows = 1;
constexpr size_t kSettingsBackIndex = 0;
constexpr size_t kSettingsHomePacingIndex = 1;
constexpr size_t kSettingsHomeDisplayIndex = 2;
constexpr size_t kSettingsHomeTypographyIndex = 3;
constexpr size_t kSettingsHomeWifiIndex = 4;
constexpr size_t kSettingsHomeBatteryIndex = 5;
constexpr size_t kSettingsHomeUpdateIndex = 6;
constexpr size_t kSettingsHomeFirmwareVersionIndex = 7;
constexpr size_t kSettingsHomeRestructuredDisplayIndex = 1;
constexpr size_t kSettingsHomeRestructuredPacingIndex = 2;
constexpr size_t kSettingsHomeRestructuredTypographyIndex = 3;
constexpr size_t kSettingsHomeRestructuredWifiIndex = 4;
constexpr size_t kSettingsHomeRestructuredBatteryIndex = 5;
constexpr size_t kSettingsHomeRestructuredUpdateIndex = 6;
constexpr size_t kSettingsHomeRestructuredFirmwareVersionIndex = 7;
constexpr size_t kSettingsHomeRestructuredSdCardIndex = 8;
constexpr size_t kSettingsDisplayThemeIndex = 1;
constexpr size_t kSettingsDisplayBrightnessIndex = 2;
constexpr size_t kSettingsDisplayHandednessIndex = 3;
constexpr size_t kSettingsDisplayReaderControlsIndex = 4;
constexpr size_t kSettingsDisplayChapterLabelIndex = 5;
constexpr size_t kSettingsDisplayFooterIndex = 6;
constexpr size_t kSettingsDisplayBatteryIndex = 7;
constexpr size_t kSettingsDisplayScreensaverIndex = 8;
constexpr size_t kSettingsDisplayStandbyTimerIndex = 9;
constexpr size_t kSettingsDisplayReaderBatteryIndex = 10;
constexpr size_t kSettingsDisplayReaderChapterIndex = 11;
constexpr size_t kSettingsDisplayReaderProgressIndex = 12;
constexpr size_t kSettingsDisplayLanguageIndex = 13;
constexpr size_t kSettingsDisplayMenuRepeatIndex = 14;
constexpr size_t kSettingsDisplayRestructuredThemeIndex = 1;
constexpr size_t kSettingsDisplayRestructuredBrightnessIndex = 2;
constexpr size_t kSettingsDisplayRestructuredHandednessIndex = 3;
constexpr size_t kSettingsDisplayRestructuredReaderControlsIndex = 4;
constexpr size_t kSettingsDisplayRestructuredLanguageIndex = 5;
constexpr size_t kSettingsDisplayRestructuredScreensaverIndex = 6;
constexpr size_t kSettingsDisplayRestructuredStandbyTimerIndex = 7;
constexpr size_t kSettingsDisplayRestructuredChapterLabelIndex = 8;
constexpr size_t kSettingsDisplayRestructuredFooterIndex = 9;
constexpr size_t kSettingsDisplayRestructuredBatteryIndex = 10;
constexpr size_t kSettingsDisplayRestructuredReaderBatteryIndex = 11;
constexpr size_t kSettingsDisplayRestructuredReaderChapterIndex = 12;
constexpr size_t kSettingsDisplayRestructuredReaderProgressIndex = 13;
constexpr size_t kSettingsDisplayRestructuredMenuRepeatIndex = 14;
constexpr size_t kSettingsPacingReadingModeIndex = 1;
constexpr size_t kSettingsPacingPauseModeIndex = 2;
constexpr size_t kSettingsPacingWpmIndex = 3;
constexpr size_t kSettingsPacingLongWordsIndex = 4;
constexpr size_t kSettingsPacingComplexityIndex = 5;
constexpr size_t kSettingsPacingPunctuationIndex = 6;
constexpr size_t kSettingsPacingResetIndex = 7;
constexpr size_t kSettingsPacingRestructuredLongWordsIndex = 3;
constexpr size_t kSettingsPacingRestructuredComplexityIndex = 4;
constexpr size_t kSettingsPacingRestructuredPunctuationIndex = 5;
constexpr size_t kSettingsPacingRestructuredResetIndex = 6;
constexpr size_t kWifiSettingsNetworkIndex = 1;
constexpr size_t kWifiSettingsChooseIndex = 2;
constexpr size_t kWifiSettingsAutoUpdateIndex = 3;
constexpr size_t kWifiSettingsForgetIndex = 4;
constexpr size_t kWifiSettingsOtaOwnerIndex = 5;
constexpr size_t kWifiSettingsRestructuredNetworkIndex = 1;
constexpr size_t kWifiSettingsRestructuredAutoUpdateIndex = 2;
constexpr size_t kWifiSettingsRestructuredOtaOwnerIndex = 3;
constexpr size_t kWifiNetworkSettingsChooseIndex = 1;
constexpr size_t kWifiNetworkSettingsForgetIndex = 2;
constexpr size_t kSettingsBatteryCpuPlayIndex = 1;
constexpr size_t kSettingsBatteryCpuScrollIndex = 2;
constexpr size_t kSettingsBatteryCpuPausedIndex = 3;
constexpr size_t kSettingsBatteryCpuMenuIndex = 4;
constexpr size_t kSettingsBatteryCpuStandbyIndex = 5;
constexpr size_t kSettingsBatteryAutoDimDelayIndex = 6;
constexpr size_t kSettingsBatteryAutoDimLevelIndex = 7;

constexpr size_t kBookPickerBackIndex = 0;
constexpr size_t kChapterPickerBackIndex = 0;
constexpr size_t kChapterPickerFallbackIndex = 1;
constexpr size_t kWifiNetworksBackIndex = 0;
constexpr size_t kWifiNetworksFirstItemIndex = 1;
constexpr size_t kFocusTimerGenreBackIndex = 0;
constexpr size_t kFocusTimerGenreFirstIndex = 1;
// Preference keys are defined once in settings/PreferenceKeys.h and shared with
// the web companion; pull them into scope so existing call sites are unchanged.
using namespace settings;
constexpr size_t kReaderFontSizeCount = 3;
constexpr size_t kPhantomBeforeCharTargets[] = {64, 96, 144};
constexpr size_t kPhantomAfterCharTargets[] = {96, 144, 208};
constexpr uint32_t kNoSavedWordIndex = 0xFFFFFFFFUL;
constexpr uint16_t kPacingDelayMinMs = 0;
constexpr uint16_t kPacingDelayMaxMs = 600;
constexpr uint16_t kPacingDelayStepMs = 50;
constexpr uint16_t kDefaultPacingDelayMs = 200;
constexpr uint16_t kSettingsWpmMin = 10;
constexpr uint16_t kSettingsWpmLowMax = 100;
constexpr uint16_t kSettingsWpmLowStep = 10;
constexpr uint16_t kSettingsWpmMax = 1000;
constexpr uint16_t kSettingsWpmHighStep = 25;
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

bool isValidCpuMhz(uint32_t mhz, bool allow40Mhz) {
  return mhz == 80 || mhz == 160 || mhz == 240 || (allow40Mhz && mhz == 40);
}

uint32_t sanitizeCpuMhz(uint32_t mhz, uint32_t fallback, bool allow40Mhz = false) {
  return isValidCpuMhz(mhz, allow40Mhz) ? mhz : fallback;
}

uint8_t sanitizeAutoDimBrightness(uint8_t percent, uint8_t fallback) {
  return percent == 0 || percent == 10 || percent == 20 || percent == 30 ? percent : fallback;
}

uint32_t sanitizeAutoDimDelayMs(uint32_t delayMs, uint32_t fallback) {
  return delayMs == 0 || delayMs == 30000 || delayMs == 60000 || delayMs == 120000 ? delayMs
                                                                                     : fallback;
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

String menuRepeatDelayLabel(uint16_t delayMs) {
  delayMs = MenuRepeat::sanitizeDelayMs(delayMs);
  return delayMs == 0 ? String("Off") : String(delayMs) + " ms";
}

uint16_t nextReaderWpmSetting(uint16_t value) {
  int normalized = clampIntSetting(value, kSettingsWpmMin, kSettingsWpmMax);
  if (normalized < static_cast<int>(kSettingsWpmLowMax)) {
    normalized += kSettingsWpmLowStep;
    if (normalized > static_cast<int>(kSettingsWpmLowMax)) {
      normalized = kSettingsWpmLowMax;
    }
    return static_cast<uint16_t>(normalized);
  }

  int next = normalized + kSettingsWpmHighStep;
  if (next > static_cast<int>(kSettingsWpmMax)) {
    next = kSettingsWpmMin;
  }
  return static_cast<uint16_t>(next);
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

void copyOtaLabel(char *destination, size_t destinationSize, const String &source) {
  if (destination == nullptr || destinationSize == 0) {
    return;
  }

  const size_t copyLength = std::min(destinationSize - 1, source.length());
  for (size_t i = 0; i < copyLength; ++i) {
    destination[i] = source[i];
  }
  destination[copyLength] = '\0';
}

bool sdCardFolderRepairNeeded(const StorageManager::DiagnosticResult &result) {
  return result.mounted &&
         (!result.booksDirectory || !result.bookFilesDirectory ||
          !result.articleFilesDirectory || !result.configDirectory);
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

bool readActiveLowButton(int pin) {
  return pin >= 0 ? !digitalRead(pin) : false;
}

bool readLogicalBootButtonHeld() {
  if (Board::Config::SWAP_APP_BOOT_AND_POWER_BUTTONS) {
    return Board::Buttons::readVirtualPowerHeld();
  }

  if (Board::Config::PIN_BOOT_BUTTON >= 0) {
    return readActiveLowButton(Board::Config::PIN_BOOT_BUTTON);
  }

  return Board::Buttons::readVirtualBootHeld();
}

bool readLogicalPowerButtonHeld() {
  if (Board::Config::SWAP_APP_BOOT_AND_POWER_BUTTONS) {
    return readActiveLowButton(Board::Config::PIN_BOOT_BUTTON);
  }

  if (Board::Config::PIN_PWR_BUTTON >= 0) {
    return readActiveLowButton(Board::Config::PIN_PWR_BUTTON);
  }

  return Board::Buttons::readVirtualPowerHeld();
}

bool readFirmwarePowerButtonHeld() {
  return Board::Config::FIRMWARE_POWER_BUTTON_ENABLED ? readLogicalPowerButtonHeld() : false;
}

bool logicalPowerButtonUsesVirtualState() {
  return !Board::Config::SWAP_APP_BOOT_AND_POWER_BUTTONS && Board::Config::PIN_PWR_BUTTON < 0;
}

enum class SoftOffWakeSource : uint8_t {
  None = 0,
  Power,
  Boot,
  PowerAndBoot,
};

const char *softOffWakeSourceName(SoftOffWakeSource source) {
  switch (source) {
    case SoftOffWakeSource::Power:
      return "power";
    case SoftOffWakeSource::Boot:
      return "boot";
    case SoftOffWakeSource::PowerAndBoot:
      return "power+boot";
    case SoftOffWakeSource::None:
    default:
      return "none";
  }
}

SoftOffWakeSource waitForRecoverableSoftOffWake(bool allowPowerWake, bool allowBootWake) {
  Serial.println("[app] soft-off settling wake buttons");
  uint32_t allReleasedSinceMs = 0;
  while (true) {
    const bool powerHeld = allowPowerWake && readLogicalPowerButtonHeld();
    const bool bootHeld = allowBootWake && readLogicalBootButtonHeld();
    if ((!allowPowerWake || !powerHeld) && (!allowBootWake || !bootHeld)) {
      if (allReleasedSinceMs == 0) {
        allReleasedSinceMs = millis();
      }
      if (millis() - allReleasedSinceMs >= kSoftOffArmQuietMs) {
        break;
      }
    } else {
      allReleasedSinceMs = 0;
    }
    delay(kSoftOffWakePollMs);
  }

  if (allowPowerWake && allowBootWake) {
    Serial.println("[app] soft-off armed; press PWR or BOOT to wake");
  } else if (allowPowerWake) {
    Serial.println("[app] soft-off armed; press PWR to wake");
  } else {
    Serial.println("[app] soft-off armed; press BOOT to wake");
  }
  bool candidatePowerHeld = false;
  bool candidateBootHeld = false;
  uint32_t candidateStartedMs = 0;
  SoftOffWakeSource wakeSource = SoftOffWakeSource::None;

  while (true) {
    const bool powerHeld = allowPowerWake && readLogicalPowerButtonHeld();
    const bool bootHeld = allowBootWake && readLogicalBootButtonHeld();
    if (powerHeld || bootHeld) {
      if (powerHeld != candidatePowerHeld || bootHeld != candidateBootHeld) {
        candidatePowerHeld = powerHeld;
        candidateBootHeld = bootHeld;
        candidateStartedMs = millis();
      } else if (millis() - candidateStartedMs >= kSoftOffWakeConfirmMs) {
        if (candidatePowerHeld && candidateBootHeld) {
          wakeSource = SoftOffWakeSource::PowerAndBoot;
        } else if (candidatePowerHeld) {
          wakeSource = SoftOffWakeSource::Power;
        } else {
          wakeSource = SoftOffWakeSource::Boot;
        }
        break;
      }
    } else {
      candidatePowerHeld = false;
      candidateBootHeld = false;
      candidateStartedMs = 0;
    }
    delay(kSoftOffWakePollMs);
  }

  const uint32_t waitStartMs = millis();
  while (((allowPowerWake && readLogicalPowerButtonHeld()) ||
          (allowBootWake && readLogicalBootButtonHeld())) &&
         millis() - waitStartMs < kSoftOffReleaseWaitMs) {
    delay(10);
  }

  return wakeSource;
}

}  // namespace

App::App()
    : button_(Board::Config::PIN_BOOT_BUTTON),
      powerButton_(Board::Config::PIN_PWR_BUTTON),
      keyButton_(Board::Config::PIN_KEY_BUTTON) {}

void App::begin() {
  Board::System::begin();
  button_.beginWithState(readLogicalBootButtonHeld());
  powerButton_.beginWithState(readFirmwarePowerButtonHeld());
  if (Board::Config::FIRMWARE_POWER_BUTTON_ENABLED &&
      (Board::Config::PIN_PWR_BUTTON < 0 || Board::Config::SWAP_APP_BOOT_AND_POWER_BUTTONS)) {
    Board::Buttons::consumeVirtualPowerShortPress();
    Board::Buttons::consumeVirtualPowerLongPress();
  }
  keyButton_.begin();
  bootButtonReleasedSinceBoot_ = !button_.isHeld();
  bootButtonLongPressHandled_ = false;
  powerButtonReleasedSinceBoot_ =
      !Board::Config::FIRMWARE_POWER_BUTTON_ENABLED
          ? true
          : (logicalPowerButtonUsesVirtualState() ? false : !powerButton_.isHeld());
  powerButtonLongPressHandled_ = false;
  powerButtonEventArmMs_ = Board::Buttons::usesPowerEvents()
                               ? millis() + Board::Buttons::powerEventIgnoreMs()
                               : 0;
  keyButtonReleasedSinceBoot_ = !keyButton_.isHeld();
  keyButtonLongPressHandled_ = false;
  keyButtonTapArmed_ = false;
  storage_.setStatusCallback(&App::handleStorageStatus, this);
  preferences_.begin(kPrefsNamespace, false);
  brightnessLevelIndex_ = preferences_.getUChar(kPrefBrightness, brightnessLevelIndex_);
  if (brightnessLevelIndex_ >= kBrightnessLevelCount) {
    brightnessLevelIndex_ = kBrightnessLevelCount - 1;
  }
  for (uint8_t i = 0; i < FocusTimer::kGenreCount; ++i) {
    focusTimer_.setTouchDurationIndexForGenre(
        static_cast<FocusTimer::Genre>(i), preferences_.getUChar(kPrefTimerDurationByGenre[i], 0));
  }
  phantomWordsEnabled_ = preferences_.getBool(kPrefPhantomWords, phantomWordsEnabled_);
  readerBatteryVisibleWhilePlaying_ =
      preferences_.getBool(kPrefReaderBatteryVisible, readerBatteryVisibleWhilePlaying_);
  readerChapterVisibleWhilePlaying_ =
      preferences_.getBool(kPrefReaderChapterVisible, readerChapterVisibleWhilePlaying_);
  readerProgressVisibleWhilePlaying_ =
      preferences_.getBool(kPrefReaderProgressVisible, readerProgressVisibleWhilePlaying_);
  uiLanguage_ =
      Localization::sanitizeLanguage(preferences_.getUChar(
          kPrefUiLanguage, static_cast<uint8_t>(uiLanguage_)));
  readerMode_ = readerModeFromSetting(
      preferences_.getUChar(kPrefReaderMode, static_cast<uint8_t>(readerMode_)));
  chapterLabelEnabled_ =
      preferences_.getBool(chapterLabelPrefKey(), chapterLabelDefaultForMode(readerMode_));
  handednessMode_ = handednessModeFromSetting(
      preferences_.getUChar(kPrefHandedness, static_cast<uint8_t>(handednessMode_)));
  readerControlsSwapped_ =
      preferences_.getBool(kPrefReaderControlsSwapped, readerControlsSwapped_);
  readerFontSizeIndex_ = preferences_.getUChar(kPrefReaderFontSize, readerFontSizeIndex_);
  if (readerFontSizeIndex_ >= kReaderFontSizeCount) {
    readerFontSizeIndex_ = 0;
  }
  menuRepeatDelayMs_ = MenuRepeat::sanitizeDelayMs(
      preferences_.getUShort(kPrefMenuRepeatMs, MenuRepeat::kDefaultDelayMs));
  standbyTimerIndex_ = preferences_.getUChar(kPrefStandbyTimer, standbyTimerIndex_);
  if (standbyTimerIndex_ > 4) {
    standbyTimerIndex_ = 0;
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
  switch (preferences_.getUChar(kPrefBatteryLabelMode,
                                static_cast<uint8_t>(batteryLabelMode_))) {
    case static_cast<uint8_t>(BatteryLabelMode::TimeRemaining):
      batteryLabelMode_ = BatteryLabelMode::TimeRemaining;
      break;
    case static_cast<uint8_t>(BatteryLabelMode::Voltage):
      batteryLabelMode_ = BatteryLabelMode::Voltage;
      break;
    case static_cast<uint8_t>(BatteryLabelMode::Percent):
    default:
      batteryLabelMode_ = BatteryLabelMode::Percent;
      break;
  }
  switch (preferences_.getUChar(kPrefScreensaverMode, static_cast<uint8_t>(screensaverMode_))) {
    case static_cast<uint8_t>(ScreensaverMode::Maze):
      screensaverMode_ = ScreensaverMode::Maze;
      break;
    case static_cast<uint8_t>(ScreensaverMode::Voronoi):
      screensaverMode_ = ScreensaverMode::Voronoi;
      break;
    case static_cast<uint8_t>(ScreensaverMode::ScreenOff):
      screensaverMode_ = ScreensaverMode::ScreenOff;
      break;
    case static_cast<uint8_t>(ScreensaverMode::Life):
    default:
      screensaverMode_ = ScreensaverMode::Life;
      break;
  }
  switch (preferences_.getUChar(kPrefPauseMode, static_cast<uint8_t>(pauseMode_))) {
    case static_cast<uint8_t>(PauseMode::Instant):
      pauseMode_ = PauseMode::Instant;
      break;
    case static_cast<uint8_t>(PauseMode::SentenceEnd):
    default:
      pauseMode_ = PauseMode::SentenceEnd;
      break;
  }
  cpuMhzPlay_ = sanitizeCpuMhz(preferences_.getUInt(kPrefCpuPlay, cpuMhzPlay_), cpuMhzPlay_);
  cpuMhzScroll_ =
      sanitizeCpuMhz(preferences_.getUInt(kPrefCpuScroll, cpuMhzScroll_), cpuMhzScroll_);
  cpuMhzPaused_ =
      sanitizeCpuMhz(preferences_.getUInt(kPrefCpuPaused, cpuMhzPaused_), cpuMhzPaused_);
  cpuMhzMenu_ = sanitizeCpuMhz(preferences_.getUInt(kPrefCpuMenu, cpuMhzMenu_), cpuMhzMenu_);
  cpuMhzStandby_ = sanitizeCpuMhz(preferences_.getUInt(kPrefCpuStandby, cpuMhzStandby_),
                                  cpuMhzStandby_, true);
  autoDimBrightnessPercent_ = sanitizeAutoDimBrightness(
      preferences_.getUChar(kPrefAutoDimLevel, autoDimBrightnessPercent_),
      autoDimBrightnessPercent_);
  autoDimDelayMs_ =
      sanitizeAutoDimDelayMs(preferences_.getUInt(kPrefAutoDimDelay, autoDimDelayMs_),
                             autoDimDelayMs_);
  pacingLongWordDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingLongMs, kPrefLegacyPacingLong);
  pacingComplexWordDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingComplexMs, kPrefLegacyPacingComplex);
  pacingPunctuationDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingPunctuationMs, kPrefLegacyPacingPunctuation);
  accurateTimeEstimateEnabled_ = true;
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
  yellowModeEnabled_ = preferences_.getBool(kPrefYellowMode, yellowModeEnabled_);
  applyHandednessSettings(0, false);
  applyDisplayPreferences(0, false);
  applyTypographySettings(0, false);
  applyPacingSettings();
  bootStartedMs_ = millis();
  lastActivityMs_ = bootStartedMs_;
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

  touchInitialized_ = Input::Touch::begin();
  Board::Audio::begin();
  focusTimer_.begin();

#if RSVP_USB_TRANSFER_ENABLED && RSVP_USB_TRANSFER_AUTO_START
  state_ = AppState::Booting;
  Serial.println("[app] USB transfer auto-start active");
  enterUsbTransfer(millis());
  return;
#endif

  display_.renderProgress("SD", "Loading books", "Use SD converter for EPUB", 0);
  storageReady_ = storage_.begin();
  const uint16_t savedWpm = preferences_.getUShort(kPrefWpm, reader_.wpm());
  reader_.setWpm(savedWpm);

  pendingBootBookLoad_ = prepareBootBookLoad();
  if (!pendingBootBookLoad_) {
    usingStorageBook_ = false;
    chapterMarkers_.clear();
    paragraphStarts_.clear();
    currentBookPath_ = "";
    currentBookTitle_ = "Demo";
    reader_.begin(bootStartedMs_);
    invalidateContextPreviewWindow();
    rebuildTimeEstimateCache();
    Serial.println("[app] using built-in demo text");
  } else {
    currentBookTitle_ = storage_.bookDisplayName(pendingBootBookIndex_);
    if (currentBookTitle_.isEmpty()) {
      currentBookTitle_ = "Loading book";
    }
  }

  maybeAutoCheckForUpdates(bootStartedMs_);
  Serial.printf("[app] WPM=%u interval=%lu ms\n", reader_.wpm(),
                static_cast<unsigned long>(reader_.wordIntervalMs()));

  state_ = AppState::Booting;
  Serial.println("[app] READY splash active");
}

void App::update(uint32_t nowMs) {
  button_.updateFromState(readLogicalBootButtonHeld(), nowMs);
  powerButton_.updateFromState(readFirmwarePowerButtonHeld(), nowMs);
  keyButton_.update(nowMs);
  if (button_.isHeld() || powerButton_.isHeld() || keyButton_.isHeld()) {
    lastActivityMs_ = nowMs;
    restoreFromAutoDim(nowMs);
  }
  handleBootButton(nowMs);
  handlePowerButton(nowMs);
  handleKeyButton(nowMs);
  if (powerOffStarted_) {
    return;
  }

  const bool batteryChanged = updateBatteryStatus(nowMs);
  if (powerOffStarted_) {
    return;
  }

  if (batteryWarningOverlayVisible_) {
    updateBatteryWarningOverlay(nowMs);
    if (batteryWarningOverlayVisible_) {
      if (nowMs - lastStateLogMs_ > 1500) {
        lastStateLogMs_ = nowMs;
        ESP_LOGI(kAppTag, "state=%s", stateName(state_));
        Serial.printf("[app] state=%s ms=%lu\n", stateName(state_),
                      static_cast<unsigned long>(nowMs));
      }
      return;
    }
  }

  if (state_ == AppState::Standby) {
    handleTouch(nowMs);
    updateStandbyScreensaver(nowMs);
    if (nowMs - lastStateLogMs_ > 1500) {
      lastStateLogMs_ = nowMs;
      ESP_LOGI(kAppTag, "state=%s", stateName(state_));
      Serial.printf("[app] state=%s ms=%lu\n", stateName(state_),
                    static_cast<unsigned long>(nowMs));
    }
    return;
  }

  pollOtaCheckResult(nowMs);
  updateState(nowMs);
  loadPendingBootBook(nowMs);
  maybeOpenUpdateConfirm(nowMs);
  updateFocusTimer(nowMs);
  updateReader(nowMs);
  handleTouch(nowMs);
  updateWpmFeedback(nowMs);
  updateBrightnessToast(nowMs);
  updateAutoDim(nowMs);
  updateBatteryRuntimeLabel(nowMs);
  maybeSaveReadingPosition(nowMs);
  updateTimeEstimateBuild(nowMs);
  updateIdleStandby(nowMs);
  if (state_ == AppState::Standby) {
    return;
  }

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

void App::updateIdleStandby(uint32_t nowMs) {
  if (state_ == AppState::Standby || state_ == AppState::Sleeping || powerOffStarted_) {
    return;
  }

  bool idle = state_ == AppState::Paused || state_ == AppState::Menu;
  if (state_ == AppState::Menu && menuScreen_ == MenuScreen::FocusTimerSession) {
    idle = false;
  }
  if (otaCheckInProgress_) {
    idle = false;
  }

  if (!idle) {
    lastActivityMs_ = nowMs;
    return;
  }

  const uint32_t timeoutMs = standbyTimerMs();
  if (timeoutMs == 0 || nowMs - lastActivityMs_ < timeoutMs) {
    return;
  }

  Serial.println("[app] standby idle timeout reached");
  enterStandby(nowMs);
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
    case AppState::CompanionSync:
      return "CompanionSync";
    case AppState::UsbTransfer:
      return "UsbTransfer";
    case AppState::Standby:
      return "Standby";
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

bool App::isSettingsMenuScreen(MenuScreen screen) const {
  return screen == MenuScreen::SettingsHome || screen == MenuScreen::SettingsDisplay ||
         screen == MenuScreen::SettingsPacing || screen == MenuScreen::SettingsBattery ||
         screen == MenuScreen::WifiSettings || screen == MenuScreen::WifiNetworkSettings;
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
    playLocked_ = false;
    pauseAtSentenceEndRequested_ = false;
    chapterTransitionVisible_ = false;
  }
  state_ = nextState;
  lastActivityMs_ = nowMs;
  if (autoDimActive_) {
    autoDimActive_ = false;
    display_.setBrightnessPercent(currentBrightnessPercent());
  }

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
    case AppState::CompanionSync:
      display_.renderStatus("Sync", companionSync_.statusLine1(), companionSync_.statusLine2());
      break;
    case AppState::UsbTransfer:
      display_.renderStatus("USB", "Preparing SD", "Eject when done");
      break;
    case AppState::Standby:
      seedStandbyScreensaver(nowMs);
      updateStandbyScreensaver(nowMs, true);
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

  applyStateCpuFrequency();

  ESP_LOGI(kAppTag, "state -> %s", stateName(state_));
  Serial.printf("[app] state -> %s at %lu ms\n", stateName(state_),
                static_cast<unsigned long>(nowMs));
}

void App::applyStateCpuFrequency() {
  if (otaCheckInProgress_) {
    if (getCpuFrequencyMhz() != 240) {
      setCpuFrequencyMhz(240);
      Serial.println("[power] CPU -> 240 MHz (OTA active)");
    }
    return;
  }

  uint32_t mhz = 240;
  switch (state_) {
    case AppState::Playing:
      mhz = scrollModeEnabled() ? cpuMhzScroll_ : cpuMhzPlay_;
      break;
    case AppState::Paused:
      mhz = scrollModeEnabled() ? cpuMhzScroll_ : cpuMhzPaused_;
      break;
    case AppState::Menu:
      mhz = cpuMhzMenu_;
      break;
    case AppState::Standby:
      mhz = cpuMhzStandby_;
      break;
    case AppState::Booting:
    case AppState::CompanionSync:
    case AppState::UsbTransfer:
    case AppState::Sleeping:
    default:
      mhz = 240;
      break;
  }

  if (getCpuFrequencyMhz() != mhz) {
    setCpuFrequencyMhz(mhz);
    Serial.printf("[power] CPU -> %u MHz (state=%s)\n", static_cast<unsigned int>(mhz),
                  stateName(state_));
  }
}

void App::updateState(uint32_t nowMs) {
  if (state_ == AppState::Booting) {
    if (nowMs - bootStartedMs_ < kBootSplashMs) {
      return;
    }

    setState((playLocked_ || pauseAtSentenceEndRequested_) ? AppState::Playing : AppState::Paused,
             nowMs);
    return;
  }

  if (state_ == AppState::UsbTransfer) {
    updateUsbTransfer(nowMs);
    return;
  }

  if (state_ == AppState::CompanionSync) {
    updateCompanionSync(nowMs);
    return;
  }

  if (state_ == AppState::Menu || state_ == AppState::Standby || state_ == AppState::Sleeping) {
    // Menu, standby, and sleeping state changes are driven by direct input and power events.
    return;
  }

  if (playLocked_ || pauseAtSentenceEndRequested_) {
    setState(AppState::Playing, nowMs);
    return;
  }

  setState(AppState::Paused, nowMs);
}

void App::updateReader(uint32_t nowMs) {
  if (state_ != AppState::Playing) {
    return;
  }

  if (updateChapterTransition(nowMs)) {
    return;
  }

  if (!ensureCurrentBookWordAvailable(nowMs)) {
    return;
  }

  if (shouldFinalizeReaderPause(nowMs)) {
    finalizeReaderPause(nowMs);
    return;
  }

  const size_t previousIndex = reader_.currentIndex();
  const bool changed = reader_.update(nowMs, !pauseAtSentenceEndRequested_);
  if (!ensureCurrentBookWordAvailable(nowMs)) {
    return;
  }
  if (changed && maybeStartChapterTransition(previousIndex, reader_.currentIndex(), nowMs)) {
    return;
  }
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

void App::executeBootButtonSingleTap(uint32_t nowMs) {
  if (state_ == AppState::Menu && Board::Config::BOOT_BUTTON_BACKS_OUT_OF_MENU) {
    navigateBackInMenu(nowMs);
    return;
  }

  if (Board::Config::BOOT_BUTTON_TOGGLES_READER &&
      (state_ == AppState::Paused || state_ == AppState::Playing)) {
    toggleReaderPlaybackFromShortcut(nowMs);
    return;
  }

  cycleBrightness(nowMs);
}

void App::handleBootButton(uint32_t nowMs) {
  if (state_ == AppState::Standby) {
    if (!Board::Config::BOOT_BUTTON_WAKES_STANDBY) {
      return;
    }

    if (!standbyButtonsReleased_ && !button_.isHeld() && !powerButton_.isHeld() &&
        nowMs - standbyEnteredMs_ >= kStandbyWakeGraceMs) {
      standbyButtonsReleased_ = true;
    }
    if (standbyButtonsReleased_ && button_.wasPressedEvent()) {
      bootButtonLongPressHandled_ = true;
      exitStandby(nowMs);
    }
    return;
  }

  if (state_ == AppState::Booting || state_ == AppState::UsbTransfer ||
      state_ == AppState::Sleeping || powerOffStarted_) {
    return;
  }

  if (!bootButtonReleasedSinceBoot_) {
    if (!button_.isHeld()) {
      bootButtonReleasedSinceBoot_ = true;
    }
    return;
  }

  if (state_ == AppState::CompanionSync) {
    if (!Board::Config::BOOT_BUTTON_BACKS_OUT_OF_MENU || !button_.wasReleasedEvent()) {
      return;
    }
    if (button_.lastHoldDurationMs() < kThemeToggleHoldMs) {
      bootButtonLongPressHandled_ = false;
      exitCompanionSync(nowMs);
    }
    return;
  }

  if (button_.isHeld() && !bootButtonLongPressHandled_ &&
      button_.heldDurationMs(nowMs) >= kThemeToggleHoldMs) {
    bootButtonLongPressHandled_ = true;
    if (Board::Config::BOOT_BUTTON_HOLD_STARTS_STANDBY) {
      Serial.println("[button] BOOT hold -> standby");
      enterStandby(nowMs);
    } else {
      cycleThemeMode(nowMs);
    }
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
    if (Board::Config::BOOT_BUTTON_TOGGLES_READER ||
        Board::Config::BOOT_BUTTON_BACKS_OUT_OF_MENU) {
      executeBootButtonSingleTap(nowMs);
    } else {
      cycleBrightness(nowMs);
    }
  }
}

void App::handlePowerButton(uint32_t nowMs) {
  if (!Board::Config::FIRMWARE_POWER_BUTTON_ENABLED) {
    return;
  }

  if (Board::Buttons::usesPowerEvents() && nowMs < powerButtonEventArmMs_) {
    const bool ignoredShort = Board::Buttons::consumeVirtualPowerShortPress();
    const bool ignoredLong = Board::Buttons::consumeVirtualPowerLongPress();
    if (ignoredShort || ignoredLong || powerButton_.isHeld()) {
      powerButtonEventArmMs_ = nowMs + Board::Buttons::powerEventIgnoreMs();
    }
    return;
  }

  if (!powerButtonReleasedSinceBoot_) {
    if (logicalPowerButtonUsesVirtualState()) {
      Board::Buttons::consumeVirtualPowerShortPress();
      Board::Buttons::consumeVirtualPowerLongPress();
    }
    if (!powerButton_.isHeld()) {
      powerButtonReleasedSinceBoot_ = true;
    }
    return;
  }

  if (Board::Buttons::usesPowerEvents()) {
    if (state_ == AppState::UsbTransfer || state_ == AppState::CompanionSync || powerOffStarted_) {
      return;
    }

    if (powerButtonLongPressHandled_ && powerButton_.isHeld()) {
      return;
    }

    if (powerButton_.isHeld() && powerButton_.heldDurationMs(nowMs) >= kPowerOffHoldMs) {
      powerButtonLongPressHandled_ = true;
      if (Board::Config::SUPPORTS_SOFTWARE_POWEROFF) {
        openPowerOffConfirm(nowMs);
      } else {
        enterStandby(nowMs);
      }
      return;
    }

    if (!powerButton_.wasReleasedEvent()) {
      return;
    }

    if (powerButtonLongPressHandled_) {
      powerButtonLongPressHandled_ = false;
      return;
    }

    if (powerButton_.lastHoldDurationMs() < kPowerOffHoldMs) {
      if (state_ == AppState::Standby) {
        exitStandby(nowMs);
      } else if (state_ != AppState::Booting && state_ != AppState::Sleeping) {
        enterStandby(nowMs);
      }
    }

    return;
  }

  if (state_ == AppState::Standby) {
    if (!standbyButtonsReleased_ && !button_.isHeld() && !powerButton_.isHeld() &&
        nowMs - standbyEnteredMs_ >= kStandbyWakeGraceMs) {
      standbyButtonsReleased_ = true;
    }
    if (standbyButtonsReleased_ && powerButton_.wasPressedEvent()) {
      powerButtonReleasedSinceBoot_ = false;
      powerButtonLongPressHandled_ = false;
      exitStandby(nowMs);
    }
    return;
  }

  if (state_ == AppState::UsbTransfer || state_ == AppState::CompanionSync || powerOffStarted_) {
    return;
  }

  if (logicalPowerButtonUsesVirtualState() && Board::Buttons::consumeVirtualPowerLongPress()) {
    powerButtonLongPressHandled_ = true;
    if (Board::Config::SUPPORTS_SOFTWARE_POWEROFF) {
      openPowerOffConfirm(nowMs);
    } else {
      enterStandby(nowMs);
    }
    return;
  }

  if (powerButtonLongPressHandled_ && powerButton_.isHeld()) {
    return;
  }

  if (state_ == AppState::Menu && isFocusTimerMenuScreen(menuScreen_) &&
      powerButton_.isHeld() && nowMs - powerButton_.lastEdgeMs() >= kUsbTransferExitHoldMs) {
    powerButtonLongPressHandled_ = true;
    resetFocusTimer();
    menuScreen_ = MenuScreen::Main;
    renderMainMenu();
    return;
  }

  if (powerButton_.isHeld() && nowMs - powerButton_.lastEdgeMs() >= kPowerOffHoldMs) {
    powerButtonLongPressHandled_ = true;
    if (Board::Config::SUPPORTS_SOFTWARE_POWEROFF) {
      openPowerOffConfirm(nowMs);
    } else {
      enterStandby(nowMs);
    }
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

void App::handleKeyButton(uint32_t nowMs) {
  if (!keyButtonReleasedSinceBoot_) {
    if (!keyButton_.isHeld()) {
      keyButtonReleasedSinceBoot_ = true;
    }
    return;
  }

  if (state_ == AppState::Standby) {
    if (!standbyButtonsReleased_ && !button_.isHeld() && !powerButton_.isHeld() &&
        !keyButton_.isHeld() && nowMs - standbyEnteredMs_ >= kStandbyWakeGraceMs) {
      standbyButtonsReleased_ = true;
    }
    if (keyButtonLongPressHandled_ && !keyButton_.isHeld()) {
      keyButtonLongPressHandled_ = false;
    }
    if (standbyButtonsReleased_ && keyButton_.wasPressedEvent()) {
      keyButtonLongPressHandled_ = true;
      exitStandby(nowMs);
    }
    return;
  }

  if (state_ == AppState::Booting || state_ == AppState::UsbTransfer ||
      state_ == AppState::CompanionSync ||
      state_ == AppState::Sleeping || powerOffStarted_) {
    return;
  }

  if (keyButtonLongPressHandled_ && keyButton_.isHeld()) {
    return;
  }

  if (keyButton_.isHeld() && keyButton_.heldDurationMs(nowMs) >= kThemeToggleHoldMs) {
    keyButtonLongPressHandled_ = true;
    Serial.println("[button] KEY hold -> standby");
    enterStandby(nowMs);
    return;
  }

  if (!keyButton_.wasReleasedEvent()) {
    return;
  }

  if (keyButtonLongPressHandled_) {
    keyButtonLongPressHandled_ = false;
    return;
  }

  if (state_ == AppState::Playing) {
    pauseAtSentenceEndRequested_ = false;
    playLocked_ = false;
    setState(AppState::Paused, nowMs);
    return;
  }

  if (state_ == AppState::Paused) {
    toggleReaderPlaybackFromShortcut(nowMs);
  }
}

void App::toggleMenuFromPowerButton(uint32_t nowMs) {
  if (state_ == AppState::Booting || state_ == AppState::UsbTransfer ||
      state_ == AppState::CompanionSync || state_ == AppState::Standby ||
      state_ == AppState::Sleeping) {
    return;
  }

  if (state_ == AppState::Menu) {
    if (menuScreen_ == MenuScreen::Main) {
      setState(AppState::Paused, nowMs);
    } else if (menuScreen_ == MenuScreen::PowerOffConfirm) {
      cancelPowerOffConfirm(nowMs);
    } else if (menuScreen_ == MenuScreen::QuickSync) {
      menuScreen_ = MenuScreen::QuickSettings;
      renderQuickSettings();
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
  menuScreen_ = MenuScreen::Main;
  menuSelectedIndex_ = MenuResume;
  wpmFeedbackVisible_ = false;
  contextViewVisible_ = false;
  if (state_ == AppState::Playing) {
    saveReadingPosition(true);
  }
  setState(AppState::Menu, nowMs);
}

void App::openQuickSettings(uint32_t nowMs) {
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;
  contextViewVisible_ = false;
  quickSettingsSelectedIndex_ = QuickSettingsBrightness;
  if (state_ == AppState::Playing) {
    saveReadingPosition(true);
  }
  menuScreen_ = MenuScreen::QuickSettings;
  setState(AppState::Menu, nowMs);
}

uint8_t App::currentBrightnessPercent() const {
  return nightMode_ ? kNightBrightnessLevels[brightnessLevelIndex_]
                    : kBrightnessLevels[brightnessLevelIndex_];
}

void App::applyDisplayPreferences(uint32_t nowMs, bool rerender) {
  display_.setDarkMode(darkMode_);
  display_.setNightMode(nightMode_);
  display_.setYellowMode(yellowModeEnabled_);
  display_.setBrightnessPercent(currentBrightnessPercent());

  if (!rerender) {
    return;
  }

  if (state_ == AppState::Menu) {
    if (isSettingsMenuScreen(menuScreen_)) {
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

  if (state_ == AppState::Menu && isSettingsMenuScreen(menuScreen_)) {
    rebuildSettingsMenuItems();
  }

  applyTypographySettings(nowMs);
}

void App::reloadRuntimePreferences(uint32_t nowMs, bool rerender) {
  brightnessLevelIndex_ = preferences_.getUChar(kPrefBrightness, brightnessLevelIndex_);
  if (brightnessLevelIndex_ >= kBrightnessLevelCount) {
    brightnessLevelIndex_ = kBrightnessLevelCount - 1;
  }
  for (uint8_t i = 0; i < FocusTimer::kGenreCount; ++i) {
    focusTimer_.setTouchDurationIndexForGenre(
        static_cast<FocusTimer::Genre>(i), preferences_.getUChar(kPrefTimerDurationByGenre[i], 0));
  }
  phantomWordsEnabled_ = preferences_.getBool(kPrefPhantomWords, phantomWordsEnabled_);
  readerBatteryVisibleWhilePlaying_ =
      preferences_.getBool(kPrefReaderBatteryVisible, readerBatteryVisibleWhilePlaying_);
  readerChapterVisibleWhilePlaying_ =
      preferences_.getBool(kPrefReaderChapterVisible, readerChapterVisibleWhilePlaying_);
  readerProgressVisibleWhilePlaying_ =
      preferences_.getBool(kPrefReaderProgressVisible, readerProgressVisibleWhilePlaying_);
  uiLanguage_ =
      Localization::sanitizeLanguage(preferences_.getUChar(
          kPrefUiLanguage, static_cast<uint8_t>(uiLanguage_)));
  readerMode_ = readerModeFromSetting(
      preferences_.getUChar(kPrefReaderMode, static_cast<uint8_t>(readerMode_)));
  chapterLabelEnabled_ =
      preferences_.getBool(chapterLabelPrefKey(), chapterLabelDefaultForMode(readerMode_));
  handednessMode_ = handednessModeFromSetting(
      preferences_.getUChar(kPrefHandedness, static_cast<uint8_t>(handednessMode_)));
  readerControlsSwapped_ =
      preferences_.getBool(kPrefReaderControlsSwapped, readerControlsSwapped_);
  readerFontSizeIndex_ = preferences_.getUChar(kPrefReaderFontSize, readerFontSizeIndex_);
  if (readerFontSizeIndex_ >= kReaderFontSizeCount) {
    readerFontSizeIndex_ = 0;
  }
  menuRepeatDelayMs_ = MenuRepeat::sanitizeDelayMs(
      preferences_.getUShort(kPrefMenuRepeatMs, MenuRepeat::kDefaultDelayMs));
  standbyTimerIndex_ = preferences_.getUChar(kPrefStandbyTimer, standbyTimerIndex_);
  if (standbyTimerIndex_ > 4) {
    standbyTimerIndex_ = 0;
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

  switch (preferences_.getUChar(kPrefBatteryLabelMode,
                                static_cast<uint8_t>(batteryLabelMode_))) {
    case static_cast<uint8_t>(BatteryLabelMode::TimeRemaining):
      batteryLabelMode_ = BatteryLabelMode::TimeRemaining;
      break;
    case static_cast<uint8_t>(BatteryLabelMode::Voltage):
      batteryLabelMode_ = BatteryLabelMode::Voltage;
      break;
    case static_cast<uint8_t>(BatteryLabelMode::Percent):
    default:
      batteryLabelMode_ = BatteryLabelMode::Percent;
      break;
  }

  switch (preferences_.getUChar(kPrefScreensaverMode, static_cast<uint8_t>(screensaverMode_))) {
    case static_cast<uint8_t>(ScreensaverMode::Maze):
      screensaverMode_ = ScreensaverMode::Maze;
      break;
    case static_cast<uint8_t>(ScreensaverMode::Voronoi):
      screensaverMode_ = ScreensaverMode::Voronoi;
      break;
    case static_cast<uint8_t>(ScreensaverMode::ScreenOff):
      screensaverMode_ = ScreensaverMode::ScreenOff;
      break;
    case static_cast<uint8_t>(ScreensaverMode::Life):
    default:
      screensaverMode_ = ScreensaverMode::Life;
      break;
  }

  switch (preferences_.getUChar(kPrefPauseMode, static_cast<uint8_t>(pauseMode_))) {
    case static_cast<uint8_t>(PauseMode::Instant):
      pauseMode_ = PauseMode::Instant;
      break;
    case static_cast<uint8_t>(PauseMode::SentenceEnd):
    default:
      pauseMode_ = PauseMode::SentenceEnd;
      break;
  }

  cpuMhzPlay_ = sanitizeCpuMhz(preferences_.getUInt(kPrefCpuPlay, cpuMhzPlay_), cpuMhzPlay_);
  cpuMhzScroll_ =
      sanitizeCpuMhz(preferences_.getUInt(kPrefCpuScroll, cpuMhzScroll_), cpuMhzScroll_);
  cpuMhzPaused_ =
      sanitizeCpuMhz(preferences_.getUInt(kPrefCpuPaused, cpuMhzPaused_), cpuMhzPaused_);
  cpuMhzMenu_ = sanitizeCpuMhz(preferences_.getUInt(kPrefCpuMenu, cpuMhzMenu_), cpuMhzMenu_);
  cpuMhzStandby_ = sanitizeCpuMhz(preferences_.getUInt(kPrefCpuStandby, cpuMhzStandby_),
                                  cpuMhzStandby_, true);
  autoDimBrightnessPercent_ = sanitizeAutoDimBrightness(
      preferences_.getUChar(kPrefAutoDimLevel, autoDimBrightnessPercent_),
      autoDimBrightnessPercent_);
  autoDimDelayMs_ =
      sanitizeAutoDimDelayMs(preferences_.getUInt(kPrefAutoDimDelay, autoDimDelayMs_),
                             autoDimDelayMs_);

  pacingLongWordDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingLongMs, kPrefLegacyPacingLong);
  pacingComplexWordDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingComplexMs, kPrefLegacyPacingComplex);
  pacingPunctuationDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingPunctuationMs, kPrefLegacyPacingPunctuation);
  accurateTimeEstimateEnabled_ = true;

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
  yellowModeEnabled_ = preferences_.getBool(kPrefYellowMode, yellowModeEnabled_);

  reader_.setWpm(preferences_.getUShort(kPrefWpm, reader_.wpm()));
  applyReaderUiOrientation();
  applyDisplayPreferences(nowMs, false);
  applyTypographySettings(nowMs, false);
  applyPacingSettings();
  if (rerender) {
    renderActiveReader(nowMs);
  }
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

void App::cycleBrightness(uint32_t nowMs) {
  brightnessLevelIndex_ = static_cast<uint8_t>((brightnessLevelIndex_ + 1) % kBrightnessLevelCount);
  preferences_.putUChar(kPrefBrightness, brightnessLevelIndex_);
  const uint8_t percent = currentBrightnessPercent();
  Serial.printf("[display] brightness level %u/%u (%u%%)\n",
                static_cast<unsigned int>(brightnessLevelIndex_ + 1),
                static_cast<unsigned int>(kBrightnessLevelCount),
                static_cast<unsigned int>(percent));
  brightnessToastVisible_ = true;
  brightnessToastUntilMs_ = nowMs + kBrightnessToastMs;
  display_.setBrightnessOverlay(String(static_cast<unsigned int>(percent)) + "%");
  applyDisplayPreferences(nowMs);
}

void App::cycleThemeMode(uint32_t nowMs) {
  // Cycle display modes as standalone options:
  // Dark -> Light -> Night -> Yellow -> Dark
  if (yellowModeEnabled_) {
    yellowModeEnabled_ = false;
    darkMode_ = true;
    nightMode_ = false;
  } else if (nightMode_) {
    yellowModeEnabled_ = true;
    darkMode_ = false;
    nightMode_ = false;
  } else if (darkMode_) {
    darkMode_ = false;
    nightMode_ = false;
  } else {
    darkMode_ = true;
    nightMode_ = true;
  }

  preferences_.putBool(kPrefYellowMode, yellowModeEnabled_);
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
    if (isSettingsMenuScreen(menuScreen_)) {
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
  chapterLabelEnabled_ =
      preferences_.getBool(chapterLabelPrefKey(), chapterLabelDefaultForMode(readerMode_));
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

void App::toggleReaderControlsLayout(uint32_t nowMs) {
  readerControlsSwapped_ = !readerControlsSwapped_;
  preferences_.putBool(kPrefReaderControlsSwapped, readerControlsSwapped_);
  Serial.printf("[display] reader controls=%s\n", readerControlsLayoutLabel().c_str());

  if (state_ == AppState::Menu && isSettingsMenuScreen(menuScreen_)) {
    rebuildSettingsMenuItems();
    renderSettings();
    return;
  }

  if (state_ == AppState::Paused || state_ == AppState::Playing) {
    renderActiveReader(nowMs);
  }
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
  if (!force) {
    const bool lowBatteryKnown =
        batteryPresent_ && batterySampleInitialized_ &&
        (batteryFilteredVoltage_ <= kBatteryLowWarningVoltage ||
         batteryDisplayedPercent_ <= kBatteryLowWarningPercent);
    uint32_t sampleIntervalMs =
        lowBatteryKnown ? kBatteryLowSampleIntervalMs : kBatterySampleIntervalMs;
    if (state_ == AppState::Playing && !lowBatteryKnown) {
      sampleIntervalMs = kBatteryPlayingSampleIntervalMs;
    }
    if (nowMs - lastBatterySampleMs_ < sampleIntervalMs) {
      return false;
    }
  }

  lastBatterySampleMs_ = nowMs;

  Board::Config::BatteryStatus status;
  if (Board::Power::readBatteryStatus(status)) {
    batteryPresent_ = true;
    if (!batterySampleInitialized_) {
      batteryFilteredVoltage_ = status.voltage;
      batteryFilteredPercent_ = status.percent;
      batteryDisplayedPercent_ = status.percent;
      batteryRuntimeAnchorPercent_ = status.percent;
      batteryRuntimeAnchorMs_ = nowMs;
      batterySampleInitialized_ = true;
    } else {
      batteryFilteredVoltage_ = (batteryFilteredVoltage_ * 0.72f) + (status.voltage * 0.28f);
      batteryFilteredPercent_ = (batteryFilteredPercent_ * 0.72f) + (status.percent * 0.28f);

      const int filteredPercent =
          std::max(0, std::min(100, static_cast<int>(batteryFilteredPercent_ + 0.5f)));
      const int delta = filteredPercent - static_cast<int>(batteryDisplayedPercent_);
      if (force || abs(delta) >= kBatteryDisplayHysteresisPercent ||
          filteredPercent <= 10 || filteredPercent >= 99) {
        batteryDisplayedPercent_ = static_cast<uint8_t>(filteredPercent);
      }

      if (batteryDisplayedPercent_ > batteryRuntimeAnchorPercent_) {
        batteryRuntimeAnchorPercent_ = batteryDisplayedPercent_;
        batteryRuntimeAnchorMs_ = nowMs;
        batteryRuntimeEstimateReady_ = false;
      } else {
        const uint8_t percentDrop = batteryRuntimeAnchorPercent_ - batteryDisplayedPercent_;
        const uint32_t elapsedMs = nowMs - batteryRuntimeAnchorMs_;
        if (percentDrop >= kBatteryRuntimeMinDropPercent &&
            elapsedMs >= kBatteryRuntimeMinElapsedMs) {
          const float minutesPerPercent =
              (static_cast<float>(elapsedMs) / 60000.0f) / static_cast<float>(percentDrop);
          batteryRuntimeMinutesRemaining_ =
              static_cast<uint32_t>(batteryDisplayedPercent_ * minutesPerPercent + 0.5f);
          batteryRuntimeEstimateReady_ = true;
        }
      }
    }
  } else {
    batteryPresent_ = false;
    batteryCriticalSampleCount_ = 0;
  }

  handleBatteryProtection(nowMs);
  if (powerOffStarted_) {
    return false;
  }

  const String nextLabel = currentBatteryLabel();
  if (nextLabel == batteryLabel_) {
    return false;
  }

  batteryLabel_ = nextLabel;
  display_.setBatteryLabel(batteryLabel_);
  if (!batteryLabel_.isEmpty()) {
    Serial.printf("[power] battery %.2f V raw=%u%% shown=%u%% label=%s\n", status.voltage,
                  static_cast<unsigned int>(status.percent),
                  static_cast<unsigned int>(batteryDisplayedPercent_), batteryLabel_.c_str());
  } else {
    Serial.println("[power] battery not detected");
  }
  return true;
}

void App::handleBatteryProtection(uint32_t nowMs) {
  if (!batteryPresent_ || !batterySampleInitialized_) {
    batteryCriticalSampleCount_ = 0;
    return;
  }

  const bool critical = batteryFilteredVoltage_ <= kBatteryCriticalVoltage ||
                        batteryDisplayedPercent_ <= kBatteryCriticalPercent;
  if (critical) {
    if (batteryCriticalSampleCount_ < 255) {
      ++batteryCriticalSampleCount_;
    }
  } else {
    batteryCriticalSampleCount_ = 0;
  }

  if (batteryCriticalSampleCount_ >= kBatteryCriticalConsecutiveSamples) {
    const String line2 =
        batteryVoltageLabel() + " " + String(static_cast<unsigned int>(batteryDisplayedPercent_)) +
        "%";
    Serial.printf("[power] critical battery %.2f V %u%%; powering off\n",
                  static_cast<double>(batteryFilteredVoltage_),
                  static_cast<unsigned int>(batteryDisplayedPercent_));
    display_.renderStatus("LOW BATTERY", "Powering off", line2);
    delay(kBatteryShutdownNoticeMs);
    enterPowerOff(millis());
    return;
  }

  const bool low = batteryFilteredVoltage_ <= kBatteryLowWarningVoltage ||
                   batteryDisplayedPercent_ <= kBatteryLowWarningPercent;
  if (!low) {
    return;
  }

  if (lastLowBatteryWarningMs_ == 0 ||
      nowMs - lastLowBatteryWarningMs_ >= kBatteryLowWarningRepeatMs) {
    showLowBatteryWarning(nowMs);
  }
}

void App::showLowBatteryWarning(uint32_t nowMs) {
  lastLowBatteryWarningMs_ = nowMs;
  batteryWarningOverlayVisible_ = true;
  batteryWarningRestoreAtMs_ = nowMs + kBatteryWarningVisibleMs;
  playLocked_ = false;
  pauseAtSentenceEndRequested_ = false;
  wpmFeedbackVisible_ = false;
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;

  if (state_ == AppState::Playing) {
    setState(AppState::Paused, nowMs);
  }

  const String line1 =
      String(static_cast<unsigned int>(batteryDisplayedPercent_)) + "% remaining";
  display_.renderStatus("LOW BATTERY", line1, batteryVoltageLabel() + " charge soon");
  Serial.printf("[power] low battery warning %.2f V %u%%\n",
                static_cast<double>(batteryFilteredVoltage_),
                static_cast<unsigned int>(batteryDisplayedPercent_));
}

void App::updateBatteryWarningOverlay(uint32_t nowMs) {
  if (!batteryWarningOverlayVisible_ || nowMs < batteryWarningRestoreAtMs_) {
    return;
  }

  batteryWarningOverlayVisible_ = false;
  if (state_ == AppState::Paused || state_ == AppState::Playing) {
    renderActiveReader(nowMs);
  } else if (state_ == AppState::Menu) {
    renderMenu();
  } else if (state_ == AppState::Standby) {
    updateStandbyScreensaver(nowMs, true);
  }
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

void App::updateBrightnessToast(uint32_t nowMs) {
  if (!brightnessToastVisible_ || nowMs < brightnessToastUntilMs_) {
    return;
  }

  brightnessToastVisible_ = false;
  display_.setBrightnessOverlay("");
  if (state_ == AppState::Menu || state_ == AppState::Paused || state_ == AppState::Playing ||
      state_ == AppState::Booting) {
    applyDisplayPreferences(nowMs);
  }
}

void App::updateAutoDim(uint32_t nowMs) {
  const bool dimEligible = state_ == AppState::Paused || state_ == AppState::Menu;
  if (!dimEligible) {
    restoreFromAutoDim(nowMs);
    return;
  }

  if (autoDimDelayMs_ == 0) {
    restoreFromAutoDim(nowMs);
    return;
  }

  if (!autoDimActive_ && lastActivityMs_ > 0 && nowMs - lastActivityMs_ >= autoDimDelayMs_) {
    autoDimActive_ = true;
    display_.setBrightnessPercent(autoDimBrightnessPercent_);
    Serial.printf("[power] auto-dim active -> %u%%\n",
                  static_cast<unsigned int>(autoDimBrightnessPercent_));
  }
}

void App::restoreFromAutoDim(uint32_t nowMs) {
  if (!autoDimActive_) {
    return;
  }

  autoDimActive_ = false;
  lastActivityMs_ = nowMs;
  display_.setBrightnessPercent(currentBrightnessPercent());
  Serial.println("[power] auto-dim restored");
}

void App::updateBatteryRuntimeLabel(uint32_t nowMs) {
  if (!batteryPresent_ || !batterySampleInitialized_ ||
      batteryLabelMode_ != BatteryLabelMode::TimeRemaining || !batteryRuntimeEstimateReady_) {
    return;
  }

  if (nowMs - lastBatteryLabelRefreshMs_ < kBatteryLabelRefreshIntervalMs) {
    return;
  }
  lastBatteryLabelRefreshMs_ = nowMs;

  const uint32_t elapsedSinceSampleMinutes = (nowMs - lastBatterySampleMs_) / 60000UL;
  const uint32_t projectedMinutes =
      batteryRuntimeMinutesRemaining_ > elapsedSinceSampleMinutes
          ? batteryRuntimeMinutesRemaining_ - elapsedSinceSampleMinutes
          : 0;
  const String nextLabel = formatBatteryTimeRemaining(projectedMinutes);
  if (nextLabel == batteryLabel_) {
    return;
  }

  batteryLabel_ = nextLabel;
  display_.setBatteryLabel(batteryLabel_);
  if (state_ == AppState::Paused || state_ == AppState::Playing) {
    renderActiveReader(nowMs);
  } else if (state_ == AppState::Menu) {
    renderMenu();
  }
}

bool App::isFooterMetricTap(uint16_t x, uint16_t y) const {
  return x >= Board::Config::DISPLAY_WIDTH - kReaderChromeMarginXPx - kFooterMetricTapWidthPx &&
         y >= Board::Config::DISPLAY_HEIGHT - kReaderChromeBottomMarginPx - kFooterMetricTapHeightPx;
}

bool App::isBatteryBadgeTap(uint16_t x, uint16_t y) const {
  if (readerControlsSwapped_) {
    return x <= kReaderBatteryMarginXPx + kBatteryBadgeTapWidthPx &&
           y <= kReaderBatteryTopMarginPx + kBatteryBadgeTapHeightPx;
  }

  return x >= Board::Config::DISPLAY_WIDTH - kReaderBatteryMarginXPx - kBatteryBadgeTapWidthPx &&
         y <= kReaderBatteryTopMarginPx + kBatteryBadgeTapHeightPx;
}

bool App::isPreviousSentenceTap(uint16_t x, uint16_t y) const {
  if (readerControlsSwapped_) {
    return x >= Board::Config::DISPLAY_WIDTH - kReaderChromeMarginXPx -
                    kPreviousSentenceTapWidthPx &&
           y <= kReaderChromeTopMarginPx + kPreviousSentenceTapHeightPx;
  }

  return x <= kReaderChromeMarginXPx + kPreviousSentenceTapWidthPx &&
         y <= kReaderChromeTopMarginPx + kPreviousSentenceTapHeightPx;
}

bool App::isActivelyReading() const { return state_ == AppState::Playing; }

DisplayManager::ReaderChrome App::readerChrome() const {
  DisplayManager::ReaderChrome chrome;
  const bool reading = isActivelyReading();
  chrome.showBattery = !reading || readerBatteryVisibleWhilePlaying_;
  chrome.showChapter = chapterLabelEnabled_ && (!reading || readerChapterVisibleWhilePlaying_);
  chrome.showProgress = !reading || readerProgressVisibleWhilePlaying_;
  chrome.showPreviousSentenceHint = !contextViewVisible_ || scrollModeEnabled();
  chrome.showEdgeMenuHints = !reading;
  chrome.swapPreviousSentenceAndBattery = readerControlsSwapped_;
  return chrome;
}

bool App::readerFooterVisible() const {
  const DisplayManager::ReaderChrome chrome = readerChrome();
  return chrome.showChapter || chrome.showProgress;
}

String App::readerFooterStatusLabel() const {
  if (isActivelyReading()) {
    return String(readingProgressPercent()) + "%";
  }

  return currentFooterMetricLabel();
}

String App::onOffLabel(bool enabled) const { return enabled ? uiText(UiText::On) : uiText(UiText::Off); }

bool App::handlePreviousSentenceTap(uint16_t x, uint16_t y, uint32_t nowMs) {
  const bool previewBrowseMode = contextViewVisible_ && !scrollModeEnabled();
  if (previewBrowseMode || !isPreviousSentenceTap(x, y)) {
    return false;
  }

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
  return true;
}

bool App::handleFooterMetricTap(uint16_t x, uint16_t y, uint32_t nowMs) {
  if (isActivelyReading() || !readerFooterVisible() || !isFooterMetricTap(x, y)) {
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

bool App::handleBatteryBadgeTap(uint16_t x, uint16_t y, uint32_t nowMs) {
  if (batteryLabel_.isEmpty() || !readerChrome().showBattery || !isBatteryBadgeTap(x, y)) {
    return false;
  }

  switch (batteryLabelMode_) {
    case BatteryLabelMode::Percent:
      batteryLabelMode_ = BatteryLabelMode::TimeRemaining;
      break;
    case BatteryLabelMode::TimeRemaining:
      batteryLabelMode_ = BatteryLabelMode::Voltage;
      break;
    case BatteryLabelMode::Voltage:
    default:
      batteryLabelMode_ = BatteryLabelMode::Percent;
      break;
  }
  preferences_.putUChar(kPrefBatteryLabelMode, static_cast<uint8_t>(batteryLabelMode_));
  batteryLabel_ = currentBatteryLabel();
  display_.setBatteryLabel(batteryLabel_);
  renderActiveReader(nowMs);
  const char *modeName = "percent";
  if (batteryLabelMode_ == BatteryLabelMode::TimeRemaining) {
    modeName = "time";
  } else if (batteryLabelMode_ == BatteryLabelMode::Voltage) {
    modeName = "voltage";
  }
  Serial.printf("[power] battery label mode=%s label=%s\n", modeName, batteryLabel_.c_str());
  return true;
}

void App::requestReaderPauseAtSentenceEnd(uint32_t nowMs) {
  if (state_ != AppState::Playing) {
    return;
  }

  playLocked_ = false;
  if (pauseMode_ == PauseMode::Instant) {
    pauseAtSentenceEndRequested_ = false;
    setState(AppState::Paused, nowMs);
    return;
  }

  if (!pauseAtSentenceEndRequested_) {
    pauseAtSentenceEndRequested_ = true;
    Serial.println("[app] pause requested at sentence end");
  }

  if (shouldFinalizeReaderPause(nowMs)) {
    finalizeReaderPause(nowMs);
  }
}

void App::toggleReaderPlaybackFromShortcut(uint32_t nowMs) {
  if (state_ == AppState::Playing) {
    requestReaderPauseAtSentenceEnd(nowMs);
    return;
  }

  if (state_ == AppState::Paused) {
    playLocked_ = true;
    pauseAtSentenceEndRequested_ = false;
    wpmFeedbackVisible_ = false;
    setState(AppState::Playing, nowMs);
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
  setState(AppState::Paused, nowMs);
}

void App::handleTouch(uint32_t nowMs) {
  if (!touchInitialized_) {
    return;
  }

  if (state_ == AppState::Booting || state_ == AppState::UsbTransfer ||
      state_ == AppState::Standby ||
      state_ == AppState::Sleeping) {
    Input::Touch::cancel();
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
    return;
  }

  TouchEvent ev;
  if (!Input::Touch::readEvent(ev)) {
    return;
  }
  lastActivityMs_ = nowMs;
  restoreFromAutoDim(nowMs);

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

  if (handleTopEdgeMenuSwipe(event, nowMs, deltaX, deltaY, ended)) {
    return;
  }
  if (handleBottomEdgeQuickSettingsSwipe(event, nowMs, deltaX, deltaY, ended)) {
    return;
  }

  if (state_ == AppState::Playing) {
    if (ended) {
      pausedTouch_.active = false;
      pausedTouchIntent_ = TouchIntent::None;
      if (tapLike) {
        if (handleBatteryBadgeTap(event.x, event.y, nowMs)) {
          return;
        }
        if (handleFooterMetricTap(event.x, event.y, nowMs)) {
          return;
        }
        if (handlePreviousSentenceTap(event.x, event.y, nowMs)) {
          return;
        }
      }
    }
    return;
  }

  if (pausedTouchIntent_ == TouchIntent::None) {
    if (absDeltaX >= static_cast<int>(kSwipeThresholdPx) &&
        absDeltaX > absDeltaY + static_cast<int>(kAxisBiasPx)) {
      pausedTouchIntent_ = TouchIntent::Scrub;
    } else if (previewBrowseMode && !ended && pressDurationMs >= kPreviewBrowseHoldMs &&
               absDeltaY > absDeltaX + static_cast<int>(kAxisBiasPx)) {
      pausedTouchIntent_ = TouchIntent::BrowseScroll;
    } else if (!previewBrowseMode && absDeltaY >= static_cast<int>(kSwipeThresholdPx) &&
               absDeltaY > absDeltaX + static_cast<int>(kAxisBiasPx)) {
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
    if (tapLike && handleBatteryBadgeTap(event.x, event.y, nowMs)) {
      return;
    }
    if (tapLike && handleFooterMetricTap(event.x, event.y, nowMs)) {
      return;
    }
    if (tapLike && handlePreviousSentenceTap(event.x, event.y, nowMs)) {
      return;
    }
    if (tapLike && previewBrowseMode) {
      contextViewVisible_ = false;
      renderActiveReader(nowMs);
    }
  }
}

bool App::handleTopEdgeMenuSwipe(const TouchEvent &event, uint32_t nowMs, int deltaX, int deltaY,
                                 bool ended) {
  if (!Board::Config::ENABLE_TOP_EDGE_MENU_SWIPE || !ended || state_ == AppState::Menu ||
      state_ == AppState::Standby || state_ == AppState::Sleeping) {
    return false;
  }

  const int absDeltaX = abs(deltaX);
  const int absDeltaY = abs(deltaY);
  const int centerX = Board::Config::DISPLAY_WIDTH / 2;
  const int minMenuStartX = centerX - static_cast<int>(kMenuSwipeCenterHalfWidthPx);
  const int maxMenuStartX = centerX + static_cast<int>(kMenuSwipeCenterHalfWidthPx);
  const bool startsInTopCenter = pausedTouch_.startY <= kMenuSwipeTopZonePx &&
                                 static_cast<int>(pausedTouch_.startX) >= minMenuStartX &&
                                 static_cast<int>(pausedTouch_.startX) <= maxMenuStartX;
  const bool verticalDownSwipe =
      deltaY >= static_cast<int>(kMenuSwipeTriggerPx) &&
      absDeltaY > absDeltaX + static_cast<int>(kAxisBiasPx);
  if (!startsInTopCenter || !verticalDownSwipe) {
    return false;
  }

  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  openMainMenu(nowMs);
  Serial.printf("[touch] top-edge menu swipe x=%u y=%u dy=%d\n", event.x, event.y, deltaY);
  return true;
}

bool App::handleBottomEdgeQuickSettingsSwipe(const TouchEvent &event, uint32_t nowMs, int deltaX,
                                             int deltaY, bool ended) {
  if (!Board::Config::ENABLE_BOTTOM_EDGE_QUICK_SETTINGS_SWIPE || !ended ||
      state_ == AppState::Menu || state_ == AppState::Standby || state_ == AppState::Sleeping) {
    return false;
  }

  const int absDeltaX = abs(deltaX);
  const int absDeltaY = abs(deltaY);
  const int centerX = Board::Config::DISPLAY_WIDTH / 2;
  const int minMenuStartX = centerX - static_cast<int>(kMenuSwipeCenterHalfWidthPx);
  const int maxMenuStartX = centerX + static_cast<int>(kMenuSwipeCenterHalfWidthPx);
  const uint16_t bottomStartY =
      static_cast<uint16_t>(Board::Config::DISPLAY_HEIGHT - kQuickSettingsSwipeBottomZonePx);
  const bool startsInBottomCenter = pausedTouch_.startY >= bottomStartY &&
                                    static_cast<int>(pausedTouch_.startX) >= minMenuStartX &&
                                    static_cast<int>(pausedTouch_.startX) <= maxMenuStartX;
  const bool verticalUpSwipe =
      deltaY <= -static_cast<int>(kMenuSwipeTriggerPx) &&
      absDeltaY > absDeltaX + static_cast<int>(kAxisBiasPx);
  if (!startsInBottomCenter || !verticalUpSwipe) {
    return false;
  }

  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  openQuickSettings(nowMs);
  Serial.printf("[touch] bottom-edge quick settings swipe x=%u y=%u dy=%d\n", event.x,
                event.y, deltaY);
  return true;
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
  const int centerY = Board::Config::DISPLAY_HEIGHT / 2;
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
  const DisplayManager::ReaderChrome chrome = readerChrome();
  display_.renderScrollView(contextPreviewWords_, currentReaderContentToken(),
                            contextPreviewStartIndex_, currentIndex, scrollProgressPermille,
                            currentChapterLabel(), readingProgressPercent(), "",
                            readerFooterStatusLabel(), chrome);
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
  if (!ensureCurrentBookWordAvailable(nowMs)) {
    return;
  }
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
    menuRepeatGestureConsumed_ = false;
    menuRepeatMoved_ = false;
    menuRepeatDirection_ = 0;
    menuRepeatNextMs_ = 0;
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

  if (event.phase != TouchPhase::End) {
    if (menuScreen_ == MenuScreen::TextEntry || menuRepeatDelayMs_ == 0) {
      return;
    }

    const int repeatDirection =
        MenuRepeat::directionForDrag(deltaX, deltaY, kSwipeThresholdPx, kAxisBiasPx);
    if (repeatDirection == 0) {
      menuRepeatDirection_ = 0;
      return;
    }

    if (!menuRepeatGestureConsumed_ || repeatDirection != menuRepeatDirection_ ||
        nowMs >= menuRepeatNextMs_) {
      menuRepeatGestureConsumed_ = true;
      menuRepeatMoved_ = moveMenuSelection(repeatDirection, false) || menuRepeatMoved_;
      menuRepeatDirection_ = repeatDirection;
      menuRepeatNextMs_ = nowMs + menuRepeatDelayMs_;
    }
    return;
  }

  pausedTouch_.active = false;
  menuRepeatDirection_ = 0;

  if (menuScreen_ == MenuScreen::TextEntry) {
    if (MenuRepeat::isRightSwipe(deltaX, deltaY, kSwipeThresholdPx, kAxisBiasPx)) {
      navigateBackInMenu(nowMs);
      return;
    }
    if (absDeltaX <= static_cast<int>(kTapSlopPx) && absDeltaY <= static_cast<int>(kTapSlopPx)) {
      handleTextEntryTap(event.x, event.y, nowMs);
    }
    return;
  }

  if (menuRepeatGestureConsumed_) {
    const bool quickEdgeSwipe =
        !menuRepeatMoved_ && (nowMs - pausedTouch_.startMs) < menuRepeatDelayMs_;
    const int releaseDirection =
        MenuRepeat::directionForDrag(deltaX, deltaY, kSwipeThresholdPx, kAxisBiasPx);
    if (quickEdgeSwipe && releaseDirection != 0) {
      moveMenuSelection(releaseDirection, true);
    }
    menuRepeatGestureConsumed_ = false;
    menuRepeatMoved_ = false;
    menuRepeatNextMs_ = 0;
    return;
  }

  if (MenuRepeat::isRightSwipe(deltaX, deltaY, kSwipeThresholdPx, kAxisBiasPx)) {
    navigateBackInMenu(nowMs);
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
    moveMenuSelection(deltaY < 0 ? -1 : 1, true);
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

  if (focusTimer_.state() == FocusTimer::State::WaitForTouchStart &&
      absDeltaX >= static_cast<int>(kSwipeThresholdPx) &&
      absDeltaX > absDeltaY + static_cast<int>(kAxisBiasPx)) {
    focusTimer_.stepTouchDuration(deltaX > 0 ? 1 : -1);
    const uint8_t genreIndex = static_cast<uint8_t>(focusTimer_.genre());
    if (genreIndex < FocusTimer::kGenreCount) {
      preferences_.putUChar(kPrefTimerDurationByGenre[genreIndex],
                            focusTimer_.touchDurationIndex());
    }
    renderFocusTimerSession();
  }
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
  applyReaderUiOrientation();
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

bool App::navigateBackInMenu(uint32_t nowMs) {
  if (state_ != AppState::Menu) {
    return false;
  }

  pausedTouch_.active = false;
  menuRepeatGestureConsumed_ = false;
  menuRepeatMoved_ = false;
  menuRepeatDirection_ = 0;
  menuRepeatNextMs_ = 0;

  switch (menuScreen_) {
    case MenuScreen::Main:
    case MenuScreen::QuickSettings:
      setState(AppState::Paused, nowMs);
      return true;

    case MenuScreen::Articles:
      menuScreen_ = MenuScreen::Main;
      renderMainMenu();
      return true;

    case MenuScreen::SettingsHome:
      menuScreen_ = MenuScreen::Main;
      renderMainMenu();
      return true;

    case MenuScreen::SettingsDisplay:
      settingsSelectedIndex_ = Board::Config::ENABLE_RESTRUCTURED_MENU
                                   ? kSettingsHomeRestructuredDisplayIndex
                                   : kSettingsHomeDisplayIndex;
      menuScreen_ = MenuScreen::SettingsHome;
      rebuildSettingsMenuItems();
      renderSettings();
      return true;

    case MenuScreen::SettingsPacing:
      flushPendingTimeEstimateRebuild();
      settingsSelectedIndex_ = Board::Config::ENABLE_RESTRUCTURED_MENU
                                   ? kSettingsHomeRestructuredPacingIndex
                                   : kSettingsHomePacingIndex;
      menuScreen_ = MenuScreen::SettingsHome;
      rebuildSettingsMenuItems();
      renderSettings();
      return true;

    case MenuScreen::SettingsBattery:
      settingsSelectedIndex_ = Board::Config::ENABLE_RESTRUCTURED_MENU
                                   ? kSettingsHomeRestructuredBatteryIndex
                                   : kSettingsHomeBatteryIndex;
      menuScreen_ = MenuScreen::SettingsHome;
      rebuildSettingsMenuItems();
      renderSettings();
      return true;

    case MenuScreen::WifiSettings:
      settingsSelectedIndex_ = Board::Config::ENABLE_RESTRUCTURED_MENU
                                   ? kSettingsHomeRestructuredWifiIndex
                                   : kSettingsHomeWifiIndex;
      menuScreen_ = MenuScreen::SettingsHome;
      rebuildSettingsMenuItems();
      renderSettings();
      return true;

    case MenuScreen::WifiNetworkSettings:
      openWifiSettings();
      return true;

    case MenuScreen::WifiNetworks:
      openWifiSettings();
      return true;

    case MenuScreen::TextEntry:
      menuScreen_ = textEntrySession_.returnScreen;
      textEntrySession_ = TextEntrySession();
      textEntryButtons_.clear();
      renderMenu();
      return true;

    case MenuScreen::TypographyTuning:
      settingsSelectedIndex_ = Board::Config::ENABLE_RESTRUCTURED_MENU
                                   ? kSettingsHomeRestructuredTypographyIndex
                                   : kSettingsHomeTypographyIndex;
      menuScreen_ = MenuScreen::SettingsHome;
      rebuildSettingsMenuItems();
      renderSettings();
      return true;

    case MenuScreen::BookPicker:
      if (Board::Config::ENABLE_RESTRUCTURED_MENU && bookPickerArticlesOnly_) {
        menuScreen_ = MenuScreen::Articles;
        renderArticlesMenu();
      } else {
        menuScreen_ = MenuScreen::Main;
        renderMainMenu();
      }
      return true;

    case MenuScreen::ChapterPicker:
      menuScreen_ = MenuScreen::Main;
      renderMainMenu();
      return true;

    case MenuScreen::RestartConfirm:
      menuScreen_ = restartConfirmReturnScreen_;
      renderMenu();
      return true;

    case MenuScreen::SdCardRepairConfirm:
      menuScreen_ = MenuScreen::Main;
      renderMenu();
      return true;

    case MenuScreen::UpdateConfirm:
      updateConfirmSelectedIndex_ = UpdateConfirmSkip;
      selectUpdateConfirmItem(nowMs);
      return true;

    case MenuScreen::PowerOffConfirm:
      cancelPowerOffConfirm(nowMs);
      return true;

    case MenuScreen::QuickSync:
      menuScreen_ = MenuScreen::QuickSettings;
      renderQuickSettings();
      return true;

    case MenuScreen::FocusTimerGenres:
    case MenuScreen::FocusTimerSession:
      resetFocusTimer();
      menuScreen_ = MenuScreen::Main;
      renderMainMenu();
      return true;
  }

  return false;
}

bool App::moveMenuSelection(int direction, bool wrap) {
  if (direction == 0 || menuScreen_ == MenuScreen::TextEntry) {
    return false;
  }

  size_t *selectedIndex = &menuSelectedIndex_;
  size_t itemCount = Board::Config::ENABLE_RESTRUCTURED_MENU
                         ? static_cast<size_t>(RestructuredMenuItemCount)
                         : static_cast<size_t>(MenuItemCount);
  if (isSettingsMenuScreen(menuScreen_)) {
    selectedIndex = &settingsSelectedIndex_;
    itemCount = settingsMenuItems_.size();
  } else if (menuScreen_ == MenuScreen::Articles) {
    selectedIndex = &articlesSelectedIndex_;
    itemCount = ArticlesItemCount;
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
  } else if (menuScreen_ == MenuScreen::SdCardRepairConfirm) {
    selectedIndex = &sdCardRepairConfirmSelectedIndex_;
    itemCount = SdCardRepairConfirmItemCount;
  } else if (menuScreen_ == MenuScreen::UpdateConfirm) {
    selectedIndex = &updateConfirmSelectedIndex_;
    itemCount = UpdateConfirmItemCount;
  } else if (menuScreen_ == MenuScreen::PowerOffConfirm) {
    selectedIndex = &powerOffConfirmSelectedIndex_;
    itemCount = PowerOffConfirmItemCount;
  } else if (menuScreen_ == MenuScreen::QuickSettings) {
    selectedIndex = &quickSettingsSelectedIndex_;
    itemCount = QuickSettingsItemCount;
  } else if (menuScreen_ == MenuScreen::QuickSync) {
    selectedIndex = &quickSyncSelectedIndex_;
    itemCount = QuickSyncItemCount;
  } else if (menuScreen_ == MenuScreen::FocusTimerGenres) {
    selectedIndex = &focusTimerGenreSelectedIndex_;
    itemCount = focusTimerGenreMenuItems_.size();
  }

  if (itemCount == 0) {
    return false;
  }

  const MenuRepeat::MoveResult move =
      MenuRepeat::movedIndex(*selectedIndex, itemCount, direction, wrap);
  *selectedIndex = move.index;
  if (!move.changed) {
    return false;
  }

  renderMenu();
  if (isSettingsMenuScreen(menuScreen_)) {
    Serial.printf("[settings] selected=%s\n", settingsMenuItems_[settingsSelectedIndex_].c_str());
  } else if (menuScreen_ == MenuScreen::Articles) {
    String selectedLabel = "Back";
    switch (articlesSelectedIndex_) {
      case ArticlesBrowse:
        selectedLabel = "Browse articles";
        break;
      case ArticlesUpdateRss:
        selectedLabel = "Update RSS";
        break;
      default:
        break;
    }
    Serial.printf("[articles] selected=%s\n", selectedLabel.c_str());
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
  } else if (menuScreen_ == MenuScreen::SdCardRepairConfirm) {
    const String selectedLabel =
        sdCardRepairConfirmSelectedIndex_ == SdCardRepairConfirmYes ? "Create folders" : "Not now";
    Serial.printf("[sd-check] selected=%s\n", selectedLabel.c_str());
  } else if (menuScreen_ == MenuScreen::UpdateConfirm) {
    const String selectedLabel =
        updateConfirmSelectedIndex_ == UpdateConfirmUpdate ? "Update" : "Skip for now";
    Serial.printf("[ota] selected=%s\n", selectedLabel.c_str());
  } else if (menuScreen_ == MenuScreen::PowerOffConfirm) {
    const String selectedLabel =
        powerOffConfirmSelectedIndex_ == PowerOffConfirmYes ? "Yes" : "Cancel";
    Serial.printf("[power-off] selected=%s\n", selectedLabel.c_str());
  } else if (menuScreen_ == MenuScreen::QuickSettings) {
    String selectedLabel = "Brightness";
    switch (quickSettingsSelectedIndex_) {
      case QuickSettingsBrightness:
        selectedLabel = "Brightness";
        break;
      case QuickSettingsTheme:
        selectedLabel = "Theme";
        break;
      case QuickSettingsFocusTimer:
        selectedLabel = "Focus Timer";
        break;
      case QuickSettingsSync:
      default:
        selectedLabel = "Sync";
        break;
    }
    Serial.printf("[quick] selected=%s\n", selectedLabel.c_str());
  } else if (menuScreen_ == MenuScreen::QuickSync) {
    const String selectedLabel =
        quickSyncSelectedIndex_ == QuickSyncWifi ? "Wi-Fi Sync" : "USB Sync";
    Serial.printf("[quick] sync selected=%s\n", selectedLabel.c_str());
  } else if (menuScreen_ == MenuScreen::FocusTimerGenres) {
    Serial.printf("[timer] selected genre=%s\n",
                  focusTimerGenreMenuItems_[focusTimerGenreSelectedIndex_].c_str());
  } else {
    String selectedLabel = uiText(UiText::Resume);
    if (Board::Config::ENABLE_RESTRUCTURED_MENU) {
      switch (menuSelectedIndex_) {
        case RestructuredMenuResume:
          selectedLabel = uiText(UiText::Resume);
          break;
        case RestructuredMenuChapters:
          selectedLabel = uiText(UiText::Chapters);
          break;
        case RestructuredMenuBooks:
          selectedLabel = "Books";
          break;
        case RestructuredMenuArticles:
          selectedLabel = "Articles";
          break;
        case RestructuredMenuSettings:
          selectedLabel = uiText(UiText::Settings);
          break;
        case RestructuredMenuPowerOff:
          selectedLabel = uiText(UiText::PowerOff);
          break;
        default:
          break;
      }
    } else {
      switch (menuSelectedIndex_) {
        case MenuResume:
          selectedLabel = uiText(UiText::Resume);
          break;
        case MenuChapters:
          selectedLabel = uiText(UiText::Chapters);
          break;
        case MenuBooks:
          selectedLabel = "Books";
          break;
        case MenuArticles:
          selectedLabel = "Articles";
          break;
        case MenuFocusTimer:
          selectedLabel = "Focus Timer";
          break;
        case MenuSettings:
          selectedLabel = uiText(UiText::Settings);
          break;
        case MenuSdCardCheck:
          selectedLabel = "SD card check";
          break;
        case MenuRssFeeds:
          selectedLabel = "RSS feeds";
          break;
        case MenuCompanionSync:
          selectedLabel = "Companion sync";
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
    }
    Serial.printf("[menu] selected=%s\n", selectedLabel.c_str());
  }

  return true;
}

void App::selectMenuItem(uint32_t nowMs) {
  if (isSettingsMenuScreen(menuScreen_)) {
    selectSettingsItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::Articles) {
    selectArticlesItem(nowMs);
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
  if (menuScreen_ == MenuScreen::SdCardRepairConfirm) {
    selectSdCardRepairConfirmItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::UpdateConfirm) {
    selectUpdateConfirmItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::PowerOffConfirm) {
    selectPowerOffConfirmItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::QuickSettings) {
    selectQuickSettingsItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::QuickSync) {
    selectQuickSyncItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::FocusTimerGenres) {
    selectFocusTimerGenre(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::FocusTimerSession) {
    return;
  }

  if (Board::Config::ENABLE_RESTRUCTURED_MENU) {
    switch (menuSelectedIndex_) {
      case RestructuredMenuResume:
        setState(AppState::Paused, nowMs);
        return;
      case RestructuredMenuChapters:
        openChapterPicker();
        return;
      case RestructuredMenuBooks:
        openBookPicker(false);
        return;
      case RestructuredMenuArticles:
        openArticlesMenu();
        return;
      case RestructuredMenuSettings:
        openSettings();
        return;
      case RestructuredMenuPowerOff:
        enterPowerOff(nowMs);
        return;
      default:
        return;
    }
  }

  switch (menuSelectedIndex_) {
    case MenuResume:
      setState(AppState::Paused, nowMs);
      return;
    case MenuPowerOff:
      enterPowerOff(nowMs);
      return;
    case MenuCompanionSync:
      enterCompanionSync(nowMs);
      return;
    case MenuSdCardCheck:
      runSdCardCheck(nowMs);
      return;
    case MenuRssFeeds:
      runRssFeedCheck(nowMs);
      return;
#if RSVP_USB_TRANSFER_ENABLED
    case MenuUsbTransfer:
      enterUsbTransfer(nowMs);
      return;
#endif
    case MenuChapters:
      openChapterPicker();
      return;
    case MenuBooks:
      openBookPicker(false);
      return;
    case MenuArticles:
      openBookPicker(true);
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

void App::openArticlesMenu() {
  articlesSelectedIndex_ = ArticlesBrowse;
  menuScreen_ = MenuScreen::Articles;
  renderArticlesMenu();
}

void App::selectArticlesItem(uint32_t nowMs) {
  switch (articlesSelectedIndex_) {
    case ArticlesBack:
      menuScreen_ = MenuScreen::Main;
      renderMainMenu();
      return;
    case ArticlesBrowse:
      openBookPicker(true);
      return;
    case ArticlesUpdateRss:
      runRssFeedCheck(nowMs);
      return;
    default:
      return;
  }
}

void App::selectQuickSettingsItem(uint32_t nowMs) {
  switch (quickSettingsSelectedIndex_) {
    case QuickSettingsBrightness:
      cycleBrightness(nowMs);
      return;
    case QuickSettingsTheme:
      cycleThemeMode(nowMs);
      return;
    case QuickSettingsFocusTimer:
      openFocusTimer();
      return;
    case QuickSettingsSync:
      openQuickSync();
      return;
    default:
      return;
  }
}

void App::openQuickSync() {
  quickSyncSelectedIndex_ = QuickSyncWifi;
  menuScreen_ = MenuScreen::QuickSync;
  renderQuickSync();
}

void App::selectQuickSyncItem(uint32_t nowMs) {
  switch (quickSyncSelectedIndex_) {
    case QuickSyncWifi:
      enterCompanionSync(nowMs);
      return;
    case QuickSyncUsb:
      enterUsbTransfer(nowMs);
      return;
    default:
      return;
  }
}

void App::openSettings() {
  settingsSelectedIndex_ = Board::Config::ENABLE_RESTRUCTURED_MENU
                               ? kSettingsHomeRestructuredDisplayIndex
                               : kSettingsHomeDisplayIndex;
  menuScreen_ = MenuScreen::SettingsHome;
  rebuildSettingsMenuItems();
  renderSettings();
}

void App::selectSettingsItem(uint32_t nowMs) {
  if (settingsMenuItems_.empty()) {
    if (menuScreen_ == MenuScreen::WifiSettings) {
      openWifiSettings();
    } else if (menuScreen_ == MenuScreen::WifiNetworkSettings) {
      openWifiNetworkSettings();
    } else {
      openSettings();
    }
    return;
  }

  if (Board::Config::ENABLE_RESTRUCTURED_MENU) {
    selectRestructuredSettingsItem(nowMs);
    return;
  }

  if (menuScreen_ == MenuScreen::SettingsHome) {
    switch (settingsSelectedIndex_) {
      case kSettingsBackIndex:
        menuScreen_ = MenuScreen::Main;
        renderMainMenu();
        return;
      case kSettingsHomeDisplayIndex:
        settingsSelectedIndex_ = kSettingsDisplayThemeIndex;
        menuScreen_ = MenuScreen::SettingsDisplay;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsHomeTypographyIndex:
        openTypographyTuning();
        return;
      case kSettingsHomePacingIndex:
        settingsSelectedIndex_ = kSettingsPacingReadingModeIndex;
        menuScreen_ = MenuScreen::SettingsPacing;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsHomeWifiIndex:
        openWifiSettings();
        return;
      case kSettingsHomeBatteryIndex:
        openBatterySettings();
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

  if (menuScreen_ == MenuScreen::SettingsBattery) {
    selectBatterySettingsItem(nowMs);
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
      case kSettingsDisplayThemeIndex:
        cycleThemeMode(nowMs);
        return;
      case kSettingsDisplayBrightnessIndex:
        cycleBrightness(nowMs);
        return;
      case kSettingsDisplayHandednessIndex:
        cycleHandednessMode(nowMs);
        return;
      case kSettingsDisplayReaderControlsIndex:
        toggleReaderControlsLayout(nowMs);
        return;
      case kSettingsDisplayChapterLabelIndex:
        chapterLabelEnabled_ = !chapterLabelEnabled_;
        preferences_.putBool(chapterLabelPrefKey(), chapterLabelEnabled_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayFooterIndex:
        switch (footerMetricMode_) {
          case FooterMetricMode::Percentage:
            footerMetricMode_ = FooterMetricMode::ChapterTime;
            break;
          case FooterMetricMode::ChapterTime:
            footerMetricMode_ = FooterMetricMode::BookTime;
            break;
          case FooterMetricMode::BookTime:
            footerMetricMode_ = FooterMetricMode::Percentage;
            break;
        }
        preferences_.putUChar(kPrefFooterMetricMode, static_cast<uint8_t>(footerMetricMode_));
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayBatteryIndex:
        switch (batteryLabelMode_) {
          case BatteryLabelMode::Percent:
            batteryLabelMode_ = BatteryLabelMode::TimeRemaining;
            break;
          case BatteryLabelMode::TimeRemaining:
            batteryLabelMode_ = BatteryLabelMode::Voltage;
            break;
          case BatteryLabelMode::Voltage:
          default:
            batteryLabelMode_ = BatteryLabelMode::Percent;
            break;
        }
        preferences_.putUChar(kPrefBatteryLabelMode, static_cast<uint8_t>(batteryLabelMode_));
        batteryLabel_ = currentBatteryLabel();
        display_.setBatteryLabel(batteryLabel_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayScreensaverIndex:
        switch (screensaverMode_) {
          case ScreensaverMode::Life:
            screensaverMode_ = ScreensaverMode::Maze;
            break;
          case ScreensaverMode::Maze:
            screensaverMode_ = ScreensaverMode::Voronoi;
            break;
          case ScreensaverMode::Voronoi:
            screensaverMode_ = ScreensaverMode::ScreenOff;
            break;
          case ScreensaverMode::ScreenOff:
          default:
            screensaverMode_ = ScreensaverMode::Life;
            break;
        }
        preferences_.putUChar(kPrefScreensaverMode, static_cast<uint8_t>(screensaverMode_));
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayStandbyTimerIndex:
        standbyTimerIndex_ = static_cast<uint8_t>((standbyTimerIndex_ + 1) % 5);
        preferences_.putUChar(kPrefStandbyTimer, standbyTimerIndex_);
        lastActivityMs_ = nowMs;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayReaderBatteryIndex:
        readerBatteryVisibleWhilePlaying_ = !readerBatteryVisibleWhilePlaying_;
        preferences_.putBool(kPrefReaderBatteryVisible, readerBatteryVisibleWhilePlaying_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayReaderChapterIndex:
        readerChapterVisibleWhilePlaying_ = !readerChapterVisibleWhilePlaying_;
        preferences_.putBool(kPrefReaderChapterVisible, readerChapterVisibleWhilePlaying_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayReaderProgressIndex:
        readerProgressVisibleWhilePlaying_ = !readerProgressVisibleWhilePlaying_;
        preferences_.putBool(kPrefReaderProgressVisible, readerProgressVisibleWhilePlaying_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayLanguageIndex:
        cycleUiLanguage(nowMs);
        return;
      case kSettingsDisplayMenuRepeatIndex:
        menuRepeatDelayMs_ = MenuRepeat::nextDelayMs(menuRepeatDelayMs_);
        preferences_.putUShort(kPrefMenuRepeatMs, menuRepeatDelayMs_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      default:
        return;
    }
  }

  if (menuScreen_ != MenuScreen::SettingsPacing) {
    return;
  }

  bool pacingConfigChanged = false;
  switch (settingsSelectedIndex_) {
    case kSettingsBackIndex:
      flushPendingTimeEstimateRebuild();
      settingsSelectedIndex_ = kSettingsHomePacingIndex;
      menuScreen_ = MenuScreen::SettingsHome;
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kSettingsPacingReadingModeIndex:
      cycleReaderMode(nowMs);
      return;
    case kSettingsPacingPauseModeIndex:
      pauseMode_ =
          pauseMode_ == PauseMode::SentenceEnd ? PauseMode::Instant : PauseMode::SentenceEnd;
      preferences_.putUChar(kPrefPauseMode, static_cast<uint8_t>(pauseMode_));
      Serial.printf("[settings] pause mode=%s\n", pauseModeLabel().c_str());
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kSettingsPacingWpmIndex:
      reader_.setWpm(nextReaderWpmSetting(reader_.wpm()));
      preferences_.putUShort(kPrefWpm, reader_.wpm());
      Serial.printf("[settings] WPM=%u interval=%lu ms\n", reader_.wpm(),
                    static_cast<unsigned long>(reader_.wordIntervalMs()));
      break;
    case kSettingsPacingLongWordsIndex:
      pacingLongWordDelayMs_ = static_cast<uint16_t>(nextCyclicSetting(
          pacingLongWordDelayMs_, kPacingDelayMinMs, kPacingDelayMaxMs, kPacingDelayStepMs));
      preferences_.putUShort(kPrefPacingLongMs, pacingLongWordDelayMs_);
      pacingConfigChanged = true;
      break;
    case kSettingsPacingComplexityIndex:
      pacingComplexWordDelayMs_ = static_cast<uint16_t>(nextCyclicSetting(
          pacingComplexWordDelayMs_, kPacingDelayMinMs, kPacingDelayMaxMs, kPacingDelayStepMs));
      preferences_.putUShort(kPrefPacingComplexMs, pacingComplexWordDelayMs_);
      pacingConfigChanged = true;
      break;
    case kSettingsPacingPunctuationIndex:
      pacingPunctuationDelayMs_ = static_cast<uint16_t>(nextCyclicSetting(
          pacingPunctuationDelayMs_, kPacingDelayMinMs, kPacingDelayMaxMs, kPacingDelayStepMs));
      preferences_.putUShort(kPrefPacingPunctuationMs, pacingPunctuationDelayMs_);
      pacingConfigChanged = true;
      break;
    case kSettingsPacingResetIndex:
      pacingLongWordDelayMs_ = kDefaultPacingDelayMs;
      pacingComplexWordDelayMs_ = kDefaultPacingDelayMs;
      pacingPunctuationDelayMs_ = kDefaultPacingDelayMs;
      preferences_.putUShort(kPrefPacingLongMs, pacingLongWordDelayMs_);
      preferences_.putUShort(kPrefPacingComplexMs, pacingComplexWordDelayMs_);
      preferences_.putUShort(kPrefPacingPunctuationMs, pacingPunctuationDelayMs_);
      pacingConfigChanged = true;
      break;
    default:
      return;
  }

  if (pacingConfigChanged) {
    applyPacingSettings();
  }
  rebuildSettingsMenuItems();
  renderSettings();
}

void App::selectRestructuredSettingsItem(uint32_t nowMs) {
  if (menuScreen_ == MenuScreen::SettingsHome) {
    switch (settingsSelectedIndex_) {
      case kSettingsBackIndex:
        menuScreen_ = MenuScreen::Main;
        renderMainMenu();
        return;
      case kSettingsHomeRestructuredDisplayIndex:
        settingsSelectedIndex_ = kSettingsDisplayRestructuredThemeIndex;
        menuScreen_ = MenuScreen::SettingsDisplay;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsHomeRestructuredPacingIndex:
        settingsSelectedIndex_ = kSettingsPacingReadingModeIndex;
        menuScreen_ = MenuScreen::SettingsPacing;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsHomeRestructuredTypographyIndex:
        openTypographyTuning();
        return;
      case kSettingsHomeRestructuredWifiIndex:
        openWifiSettings();
        return;
      case kSettingsHomeRestructuredBatteryIndex:
        openBatterySettings();
        return;
      case kSettingsHomeRestructuredUpdateIndex:
        runFirmwareUpdate(preferredOtaConfig(), false, nowMs);
        return;
      case kSettingsHomeRestructuredFirmwareVersionIndex:
        return;
      case kSettingsHomeRestructuredSdCardIndex:
        runSdCardCheck(nowMs);
        return;
      default:
        return;
    }
  }

  if (menuScreen_ == MenuScreen::SettingsDisplay) {
    switch (settingsSelectedIndex_) {
      case kSettingsBackIndex:
        settingsSelectedIndex_ = kSettingsHomeRestructuredDisplayIndex;
        menuScreen_ = MenuScreen::SettingsHome;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayRestructuredThemeIndex:
        cycleThemeMode(nowMs);
        return;
      case kSettingsDisplayRestructuredBrightnessIndex:
        cycleBrightness(nowMs);
        return;
      case kSettingsDisplayRestructuredHandednessIndex:
        cycleHandednessMode(nowMs);
        return;
      case kSettingsDisplayRestructuredReaderControlsIndex:
        toggleReaderControlsLayout(nowMs);
        return;
      case kSettingsDisplayRestructuredLanguageIndex:
        cycleUiLanguage(nowMs);
        return;
      case kSettingsDisplayRestructuredScreensaverIndex:
        switch (screensaverMode_) {
          case ScreensaverMode::Life:
            screensaverMode_ = ScreensaverMode::Maze;
            break;
          case ScreensaverMode::Maze:
            screensaverMode_ = ScreensaverMode::Voronoi;
            break;
          case ScreensaverMode::Voronoi:
            screensaverMode_ = ScreensaverMode::ScreenOff;
            break;
          case ScreensaverMode::ScreenOff:
          default:
            screensaverMode_ = ScreensaverMode::Life;
            break;
        }
        preferences_.putUChar(kPrefScreensaverMode, static_cast<uint8_t>(screensaverMode_));
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayRestructuredStandbyTimerIndex:
        standbyTimerIndex_ = static_cast<uint8_t>((standbyTimerIndex_ + 1) % 5);
        preferences_.putUChar(kPrefStandbyTimer, standbyTimerIndex_);
        lastActivityMs_ = nowMs;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayRestructuredChapterLabelIndex:
        chapterLabelEnabled_ = !chapterLabelEnabled_;
        preferences_.putBool(chapterLabelPrefKey(), chapterLabelEnabled_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayRestructuredFooterIndex:
        switch (footerMetricMode_) {
          case FooterMetricMode::Percentage:
            footerMetricMode_ = FooterMetricMode::ChapterTime;
            break;
          case FooterMetricMode::ChapterTime:
            footerMetricMode_ = FooterMetricMode::BookTime;
            break;
          case FooterMetricMode::BookTime:
            footerMetricMode_ = FooterMetricMode::Percentage;
            break;
        }
        preferences_.putUChar(kPrefFooterMetricMode, static_cast<uint8_t>(footerMetricMode_));
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayRestructuredBatteryIndex:
        switch (batteryLabelMode_) {
          case BatteryLabelMode::Percent:
            batteryLabelMode_ = BatteryLabelMode::TimeRemaining;
            break;
          case BatteryLabelMode::TimeRemaining:
            batteryLabelMode_ = BatteryLabelMode::Voltage;
            break;
          case BatteryLabelMode::Voltage:
          default:
            batteryLabelMode_ = BatteryLabelMode::Percent;
            break;
        }
        preferences_.putUChar(kPrefBatteryLabelMode, static_cast<uint8_t>(batteryLabelMode_));
        batteryLabel_ = currentBatteryLabel();
        display_.setBatteryLabel(batteryLabel_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayRestructuredReaderBatteryIndex:
        readerBatteryVisibleWhilePlaying_ = !readerBatteryVisibleWhilePlaying_;
        preferences_.putBool(kPrefReaderBatteryVisible, readerBatteryVisibleWhilePlaying_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayRestructuredReaderChapterIndex:
        readerChapterVisibleWhilePlaying_ = !readerChapterVisibleWhilePlaying_;
        preferences_.putBool(kPrefReaderChapterVisible, readerChapterVisibleWhilePlaying_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayRestructuredReaderProgressIndex:
        readerProgressVisibleWhilePlaying_ = !readerProgressVisibleWhilePlaying_;
        preferences_.putBool(kPrefReaderProgressVisible, readerProgressVisibleWhilePlaying_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayRestructuredMenuRepeatIndex:
        menuRepeatDelayMs_ = MenuRepeat::nextDelayMs(menuRepeatDelayMs_);
        preferences_.putUShort(kPrefMenuRepeatMs, menuRepeatDelayMs_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      default:
        return;
    }
  }

  if (menuScreen_ == MenuScreen::SettingsPacing) {
    bool pacingConfigChanged = false;
    switch (settingsSelectedIndex_) {
      case kSettingsBackIndex:
        flushPendingTimeEstimateRebuild();
        settingsSelectedIndex_ = kSettingsHomeRestructuredPacingIndex;
        menuScreen_ = MenuScreen::SettingsHome;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsPacingReadingModeIndex:
        cycleReaderMode(nowMs);
        return;
      case kSettingsPacingPauseModeIndex:
        pauseMode_ =
            pauseMode_ == PauseMode::SentenceEnd ? PauseMode::Instant : PauseMode::SentenceEnd;
        preferences_.putUChar(kPrefPauseMode, static_cast<uint8_t>(pauseMode_));
        Serial.printf("[settings] pause mode=%s\n", pauseModeLabel().c_str());
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsPacingRestructuredLongWordsIndex:
        pacingLongWordDelayMs_ = static_cast<uint16_t>(nextCyclicSetting(
            pacingLongWordDelayMs_, kPacingDelayMinMs, kPacingDelayMaxMs, kPacingDelayStepMs));
        preferences_.putUShort(kPrefPacingLongMs, pacingLongWordDelayMs_);
        pacingConfigChanged = true;
        break;
      case kSettingsPacingRestructuredComplexityIndex:
        pacingComplexWordDelayMs_ = static_cast<uint16_t>(nextCyclicSetting(
            pacingComplexWordDelayMs_, kPacingDelayMinMs, kPacingDelayMaxMs, kPacingDelayStepMs));
        preferences_.putUShort(kPrefPacingComplexMs, pacingComplexWordDelayMs_);
        pacingConfigChanged = true;
        break;
      case kSettingsPacingRestructuredPunctuationIndex:
        pacingPunctuationDelayMs_ = static_cast<uint16_t>(nextCyclicSetting(
            pacingPunctuationDelayMs_, kPacingDelayMinMs, kPacingDelayMaxMs, kPacingDelayStepMs));
        preferences_.putUShort(kPrefPacingPunctuationMs, pacingPunctuationDelayMs_);
        pacingConfigChanged = true;
        break;
      case kSettingsPacingRestructuredResetIndex:
        pacingLongWordDelayMs_ = kDefaultPacingDelayMs;
        pacingComplexWordDelayMs_ = kDefaultPacingDelayMs;
        pacingPunctuationDelayMs_ = kDefaultPacingDelayMs;
        preferences_.putUShort(kPrefPacingLongMs, pacingLongWordDelayMs_);
        preferences_.putUShort(kPrefPacingComplexMs, pacingComplexWordDelayMs_);
        preferences_.putUShort(kPrefPacingPunctuationMs, pacingPunctuationDelayMs_);
        pacingConfigChanged = true;
        break;
      default:
        return;
    }

    if (pacingConfigChanged) {
      applyPacingSettings();
    }
    rebuildSettingsMenuItems();
    renderSettings();
    return;
  }

  if (menuScreen_ == MenuScreen::SettingsBattery) {
    selectBatterySettingsItem(nowMs);
    return;
  }

  if (menuScreen_ == MenuScreen::WifiSettings) {
    switch (settingsSelectedIndex_) {
      case kSettingsBackIndex:
        settingsSelectedIndex_ = kSettingsHomeRestructuredWifiIndex;
        menuScreen_ = MenuScreen::SettingsHome;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kWifiSettingsRestructuredNetworkIndex:
        openWifiNetworkSettings();
        return;
      case kWifiSettingsRestructuredAutoUpdateIndex:
        preferences_.putBool(kPrefOtaAuto, !otaAutoCheckEnabled());
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kWifiSettingsRestructuredOtaOwnerIndex:
        openTextEntry(TextEntryPurpose::OtaOwner, "OTA Source", "GitHub owner", "",
                      preferences_.getString(kPrefOtaOwner, ""), "", false, 39,
                      MenuScreen::WifiSettings);
        return;
      default:
        return;
    }
  }

  if (menuScreen_ == MenuScreen::WifiNetworkSettings) {
    switch (settingsSelectedIndex_) {
      case kSettingsBackIndex:
        openWifiSettings();
        return;
      case kWifiNetworkSettingsChooseIndex:
        scanWifiNetworks();
        return;
      case kWifiNetworkSettingsForgetIndex:
        preferences_.remove(kPrefWifiSsid);
        preferences_.remove(kPrefWifiPass);
        display_.renderStatus("Wi-Fi", "Credentials cleared", "");
        delay(900);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      default:
        return;
    }
  }
}

void App::openBatterySettings() {
  settingsSelectedIndex_ = kSettingsBatteryCpuPlayIndex;
  menuScreen_ = MenuScreen::SettingsBattery;
  rebuildSettingsMenuItems();
  renderSettings();
}

void App::selectBatterySettingsItem(uint32_t nowMs) {
  auto cycleCpuMhz = [](uint32_t current) -> uint32_t {
    if (current <= 80) {
      return 160;
    }
    if (current <= 160) {
      return 240;
    }
    return 80;
  };
  auto cycleCpuMhzStandby = [](uint32_t current) -> uint32_t {
    if (current <= 40) {
      return 80;
    }
    if (current <= 80) {
      return 160;
    }
    if (current <= 160) {
      return 240;
    }
    return 40;
  };
  auto refreshBatteryRuntimeLabel = [&]() {
    batteryRuntimeEstimateReady_ = false;
    lastBatteryLabelRefreshMs_ = 0;
    batteryLabel_ = currentBatteryLabel();
    display_.setBatteryLabel(batteryLabel_);
  };

  switch (settingsSelectedIndex_) {
    case kSettingsBackIndex:
      settingsSelectedIndex_ = Board::Config::ENABLE_RESTRUCTURED_MENU
                                   ? kSettingsHomeRestructuredBatteryIndex
                                   : kSettingsHomeBatteryIndex;
      menuScreen_ = MenuScreen::SettingsHome;
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kSettingsBatteryCpuPlayIndex:
      cpuMhzPlay_ = cycleCpuMhz(cpuMhzPlay_);
      preferences_.putUInt(kPrefCpuPlay, cpuMhzPlay_);
      refreshBatteryRuntimeLabel();
      applyStateCpuFrequency();
      Serial.printf("[battery] CPU RSVP -> %u MHz\n", static_cast<unsigned int>(cpuMhzPlay_));
      break;
    case kSettingsBatteryCpuScrollIndex:
      cpuMhzScroll_ = cycleCpuMhz(cpuMhzScroll_);
      preferences_.putUInt(kPrefCpuScroll, cpuMhzScroll_);
      refreshBatteryRuntimeLabel();
      applyStateCpuFrequency();
      Serial.printf("[battery] CPU scroll -> %u MHz\n", static_cast<unsigned int>(cpuMhzScroll_));
      break;
    case kSettingsBatteryCpuPausedIndex:
      cpuMhzPaused_ = cycleCpuMhz(cpuMhzPaused_);
      preferences_.putUInt(kPrefCpuPaused, cpuMhzPaused_);
      refreshBatteryRuntimeLabel();
      applyStateCpuFrequency();
      Serial.printf("[battery] CPU paused -> %u MHz\n", static_cast<unsigned int>(cpuMhzPaused_));
      break;
    case kSettingsBatteryCpuMenuIndex:
      cpuMhzMenu_ = cycleCpuMhz(cpuMhzMenu_);
      preferences_.putUInt(kPrefCpuMenu, cpuMhzMenu_);
      refreshBatteryRuntimeLabel();
      applyStateCpuFrequency();
      Serial.printf("[battery] CPU menu -> %u MHz\n", static_cast<unsigned int>(cpuMhzMenu_));
      break;
    case kSettingsBatteryCpuStandbyIndex:
      cpuMhzStandby_ = cycleCpuMhzStandby(cpuMhzStandby_);
      preferences_.putUInt(kPrefCpuStandby, cpuMhzStandby_);
      refreshBatteryRuntimeLabel();
      applyStateCpuFrequency();
      Serial.printf("[battery] CPU standby -> %u MHz\n",
                    static_cast<unsigned int>(cpuMhzStandby_));
      break;
    case kSettingsBatteryAutoDimDelayIndex:
      if (autoDimDelayMs_ == 0) {
        autoDimDelayMs_ = 30000;
      } else if (autoDimDelayMs_ <= 30000) {
        autoDimDelayMs_ = 60000;
      } else if (autoDimDelayMs_ <= 60000) {
        autoDimDelayMs_ = 120000;
      } else {
        autoDimDelayMs_ = 0;
      }
      preferences_.putUInt(kPrefAutoDimDelay, autoDimDelayMs_);
      if (autoDimDelayMs_ == 0) {
        restoreFromAutoDim(nowMs);
      }
      lastActivityMs_ = nowMs;
      Serial.printf("[battery] auto-dim delay -> %s\n", autoDimDelayLabel().c_str());
      break;
    case kSettingsBatteryAutoDimLevelIndex:
      if (autoDimBrightnessPercent_ >= 30) {
        autoDimBrightnessPercent_ = 0;
      } else if (autoDimBrightnessPercent_ == 0) {
        autoDimBrightnessPercent_ = 10;
      } else {
        autoDimBrightnessPercent_ = static_cast<uint8_t>(autoDimBrightnessPercent_ + 10);
      }
      preferences_.putUChar(kPrefAutoDimLevel, autoDimBrightnessPercent_);
      if (autoDimActive_) {
        display_.setBrightnessPercent(autoDimBrightnessPercent_);
      }
      Serial.printf("[battery] auto-dim level -> %u%%\n",
                    static_cast<unsigned int>(autoDimBrightnessPercent_));
      break;
    default:
      return;
  }

  rebuildSettingsMenuItems();
  renderSettings();
}

void App::openWifiSettings() {
  if (Board::Config::ENABLE_RESTRUCTURED_MENU) {
    settingsSelectedIndex_ = configuredWifiSsid().isEmpty()
                                 ? kWifiSettingsRestructuredNetworkIndex
                                 : kWifiSettingsRestructuredAutoUpdateIndex;
  } else {
    settingsSelectedIndex_ = configuredWifiSsid().isEmpty() ? kWifiSettingsChooseIndex
                                                            : kWifiSettingsAutoUpdateIndex;
  }
  menuScreen_ = MenuScreen::WifiSettings;
  rebuildSettingsMenuItems();
  renderSettings();
}

void App::openWifiNetworkSettings() {
  settingsSelectedIndex_ = kWifiNetworkSettingsChooseIndex;
  menuScreen_ = MenuScreen::WifiNetworkSettings;
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
  if (blockNetworkActionForOtaCheck("Wi-Fi", millis())) {
    return;
  }

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
        Board::Config::DISPLAY_WIDTH - (2 * kKeyboardMarginX) -
        static_cast<int>((keyCount - 1) * kKeyboardRowGap);
    const int keyWidth = std::max(28, availableWidth / static_cast<int>(keyCount));
    const int totalWidth =
        keyWidth * static_cast<int>(keyCount) + static_cast<int>((keyCount - 1) * kKeyboardRowGap);
    int x = std::max(0, (Board::Config::DISPLAY_WIDTH - totalWidth) / 2);
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
  const int availableWidth = Board::Config::DISPLAY_WIDTH - (2 * kKeyboardMarginX) - totalGapWidth;
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
  if (Board::Config::ENABLE_RESTRUCTURED_MENU) {
    if (menuScreen_ == MenuScreen::SettingsHome) {
      settingsMenuItems_.push_back(uiText(UiText::Back));
      settingsMenuItems_.push_back(uiText(UiText::Display));
      settingsMenuItems_.push_back(uiText(UiText::WordPacing));
      settingsMenuItems_.push_back(uiText(UiText::TypographyTune));
      settingsMenuItems_.push_back("Wi-Fi");
      settingsMenuItems_.push_back("Battery");
      settingsMenuItems_.push_back(firmwareUpdateMenuLabel());
      settingsMenuItems_.push_back("Installed: " + firmwareVersionLabel());
      settingsMenuItems_.push_back("SD card check");
    } else if (menuScreen_ == MenuScreen::SettingsDisplay) {
      settingsMenuItems_.push_back(uiText(UiText::Back));
      settingsMenuItems_.push_back("Theme: " + themeModeLabel());
      settingsMenuItems_.push_back(uiText(UiText::Brightness) + ": " +
                                   String(currentBrightnessPercent()) + "%");
      settingsMenuItems_.push_back("L/R hand: " + handednessLabel());
      settingsMenuItems_.push_back("Reader controls: " + readerControlsLayoutLabel());
      settingsMenuItems_.push_back(uiText(UiText::Language) + ": " + uiLanguageLabel());
      settingsMenuItems_.push_back("Screen saver: " + screensaverModeLabel());
      settingsMenuItems_.push_back("Standby timer: " + standbyTimerLabel());
      settingsMenuItems_.push_back("Chapter label: " + onOffLabel(chapterLabelEnabled_));
      settingsMenuItems_.push_back("Progress label: " + footerMetricModeLabel());
      settingsMenuItems_.push_back("Battery label: " + batteryLabelModeLabel());
      settingsMenuItems_.push_back("Reading battery: " +
                                   onOffLabel(readerBatteryVisibleWhilePlaying_));
      settingsMenuItems_.push_back("Reading chapter: " +
                                   onOffLabel(readerChapterVisibleWhilePlaying_));
      settingsMenuItems_.push_back("Reading progress: " +
                                   onOffLabel(readerProgressVisibleWhilePlaying_));
      settingsMenuItems_.push_back("Menu repeat: " + menuRepeatDelayLabel(menuRepeatDelayMs_));
    } else if (menuScreen_ == MenuScreen::SettingsPacing) {
      settingsMenuItems_.push_back(uiText(UiText::Back));
      settingsMenuItems_.push_back("Reading mode: " + readerModeLabel());
      settingsMenuItems_.push_back("Pause mode: " + pauseModeLabel());
      settingsMenuItems_.push_back(uiText(UiText::LongWords) + ": " +
                                   pacingDelayLabel(pacingLongWordDelayMs_));
      settingsMenuItems_.push_back(uiText(UiText::Complexity) + ": " +
                                   pacingDelayLabel(pacingComplexWordDelayMs_));
      settingsMenuItems_.push_back(uiText(UiText::Punctuation) + ": " +
                                   pacingDelayLabel(pacingPunctuationDelayMs_));
      settingsMenuItems_.push_back(uiText(UiText::ResetPacing));
    } else if (menuScreen_ == MenuScreen::WifiSettings) {
      settingsMenuItems_.push_back(uiText(UiText::Back));
      settingsMenuItems_.push_back("Network: " +
                                   storedOrFallbackLabel(configuredWifiSsid(), "Not set"));
      settingsMenuItems_.push_back("Auto OTA: " + String(otaAutoCheckEnabled() ? "On" : "Off"));
      settingsMenuItems_.push_back("OTA Owner: " + otaOwnerLabel());
    } else if (menuScreen_ == MenuScreen::SettingsBattery) {
      settingsMenuItems_.push_back(uiText(UiText::Back));
      settingsMenuItems_.push_back("CPU RSVP: " + cpuMhzLabel(cpuMhzPlay_));
      settingsMenuItems_.push_back("CPU scroll: " + cpuMhzLabel(cpuMhzScroll_));
      settingsMenuItems_.push_back("CPU paused: " + cpuMhzLabel(cpuMhzPaused_));
      settingsMenuItems_.push_back("CPU menu: " + cpuMhzLabel(cpuMhzMenu_));
      String standbyLabel = "CPU standby: " + cpuMhzLabel(cpuMhzStandby_);
      if (cpuMhzStandby_ <= 40) {
        standbyLabel += " (slow anim)";
      }
      settingsMenuItems_.push_back(standbyLabel);
      settingsMenuItems_.push_back("Auto-dim delay: " + autoDimDelayLabel());
      settingsMenuItems_.push_back("Auto-dim level: " + autoDimBrightnessLabel());
    } else if (menuScreen_ == MenuScreen::WifiNetworkSettings) {
      settingsMenuItems_.push_back(uiText(UiText::Back));
      settingsMenuItems_.push_back("Choose network: " +
                                   storedOrFallbackLabel(configuredWifiSsid(), "Not set"));
      settingsMenuItems_.push_back("Forget network");
    }

    if (settingsSelectedIndex_ >= settingsMenuItems_.size()) {
      settingsSelectedIndex_ = kSettingsBackIndex;
    }
    return;
  }

  if (menuScreen_ == MenuScreen::SettingsHome) {
    settingsMenuItems_.push_back(uiText(UiText::Back));
    settingsMenuItems_.push_back(uiText(UiText::WordPacing));
    settingsMenuItems_.push_back(uiText(UiText::Display));
    settingsMenuItems_.push_back(uiText(UiText::TypographyTune));
    settingsMenuItems_.push_back("Wi-Fi");
    settingsMenuItems_.push_back("Battery");
    settingsMenuItems_.push_back(firmwareUpdateMenuLabel());
    settingsMenuItems_.push_back("Installed: " + firmwareVersionLabel());
  } else if (menuScreen_ == MenuScreen::SettingsDisplay) {
    settingsMenuItems_.push_back(uiText(UiText::Back));
    settingsMenuItems_.push_back("Display mode: " + themeModeLabel());
    settingsMenuItems_.push_back(uiText(UiText::Brightness) + ": " +
                                 String(currentBrightnessPercent()) + "%");
    settingsMenuItems_.push_back("Reader hand: " + handednessLabel());
    settingsMenuItems_.push_back("Reader controls: " + readerControlsLayoutLabel());
    settingsMenuItems_.push_back("Chapter label: " + onOffLabel(chapterLabelEnabled_));
    settingsMenuItems_.push_back("Footer label: " + footerMetricModeLabel());
    settingsMenuItems_.push_back("Battery label: " + batteryLabelModeLabel());
    settingsMenuItems_.push_back("Screensaver: " + screensaverModeLabel());
    settingsMenuItems_.push_back("Standby timer: " + standbyTimerLabel());
    settingsMenuItems_.push_back("Reading battery: " +
                                 onOffLabel(readerBatteryVisibleWhilePlaying_));
    settingsMenuItems_.push_back("Reading chapter: " +
                                 onOffLabel(readerChapterVisibleWhilePlaying_));
    settingsMenuItems_.push_back("Reading percent: " +
                                 onOffLabel(readerProgressVisibleWhilePlaying_));
    settingsMenuItems_.push_back(uiText(UiText::Language) + ": " + uiLanguageLabel());
    settingsMenuItems_.push_back("Menu repeat: " + menuRepeatDelayLabel(menuRepeatDelayMs_));
  } else if (menuScreen_ == MenuScreen::SettingsPacing) {
    settingsMenuItems_.push_back(uiText(UiText::Back));
    settingsMenuItems_.push_back("Reading mode: " + readerModeLabel());
    settingsMenuItems_.push_back("Pause behaviour: " + pauseModeLabel());
    settingsMenuItems_.push_back("Base speed: " + String(reader_.wpm()) + " WPM");
    settingsMenuItems_.push_back(uiText(UiText::LongWords) + ": " +
                                 pacingDelayLabel(pacingLongWordDelayMs_));
    settingsMenuItems_.push_back(uiText(UiText::Complexity) + ": " +
                                 pacingDelayLabel(pacingComplexWordDelayMs_));
    settingsMenuItems_.push_back(uiText(UiText::Punctuation) + ": " +
                                 pacingDelayLabel(pacingPunctuationDelayMs_));
    settingsMenuItems_.push_back(uiText(UiText::ResetPacing));
  } else if (menuScreen_ == MenuScreen::WifiSettings) {
    settingsMenuItems_.push_back(uiText(UiText::Back));
    settingsMenuItems_.push_back("Network: " + storedOrFallbackLabel(configuredWifiSsid(), "Not set"));
    settingsMenuItems_.push_back("Choose network");
    settingsMenuItems_.push_back("Auto OTA: " + String(otaAutoCheckEnabled() ? "On" : "Off"));
    settingsMenuItems_.push_back("Forget network");
    settingsMenuItems_.push_back("OTA Owner: " + otaOwnerLabel());
  } else if (menuScreen_ == MenuScreen::SettingsBattery) {
    settingsMenuItems_.push_back(uiText(UiText::Back));
    settingsMenuItems_.push_back("CPU RSVP: " + cpuMhzLabel(cpuMhzPlay_));
    settingsMenuItems_.push_back("CPU scroll: " + cpuMhzLabel(cpuMhzScroll_));
    settingsMenuItems_.push_back("CPU paused: " + cpuMhzLabel(cpuMhzPaused_));
    settingsMenuItems_.push_back("CPU menu: " + cpuMhzLabel(cpuMhzMenu_));
    String standbyLabel = "CPU standby: " + cpuMhzLabel(cpuMhzStandby_);
    if (cpuMhzStandby_ <= 40) {
      standbyLabel += " (slow anim)";
    }
    settingsMenuItems_.push_back(standbyLabel);
    settingsMenuItems_.push_back("Auto-dim delay: " + autoDimDelayLabel());
    settingsMenuItems_.push_back("Auto-dim level: " + autoDimBrightnessLabel());
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
  (void)nowMs;
  OtaUpdater::Config otaConfig = preferredOtaConfig();
  if (!otaConfig.autoCheck || !otaUpdater_.isConfigured(otaConfig)) {
    return;
  }

  Serial.println("[ota] auto-check enabled");
  startBackgroundOtaCheck(otaConfig);
}

bool App::startBackgroundOtaCheck(const OtaUpdater::Config &config) {
  if (otaCheckInProgress_) {
    Serial.println("[ota] background check already running");
    return false;
  }

  if (otaCheckQueue_ == nullptr) {
    otaCheckQueue_ = xQueueCreate(1, sizeof(OtaCheckResult));
    if (otaCheckQueue_ == nullptr) {
      Serial.println("[ota] could not create result queue");
      return false;
    }
  }
  xQueueReset(otaCheckQueue_);

  OtaCheckTaskParams *params = new OtaCheckTaskParams();
  if (params == nullptr) {
    Serial.println("[ota] could not allocate task params");
    return false;
  }
  params->config = config;
  params->resultQueue = otaCheckQueue_;

  otaCheckInProgress_ = true;
  applyStateCpuFrequency();
  BaseType_t created = xTaskCreatePinnedToCore(otaCheckTask, "ota_check",
                                               kOtaCheckTaskStackBytes, params, 1, nullptr, 0);
  if (created != pdPASS) {
    Serial.printf("[ota] background task create failed: %ld\n", static_cast<long>(created));
    otaCheckInProgress_ = false;
    applyStateCpuFrequency();
    delete params;
    return false;
  }

  Serial.println("[ota] background check started");
  return true;
}

void App::otaCheckTask(void *params) {
  OtaCheckTaskParams *taskParams = static_cast<OtaCheckTaskParams *>(params);
  if (taskParams == nullptr) {
    vTaskDelete(nullptr);
    return;
  }

  OtaCheckResult queuedResult;

  const OtaUpdater::Result result =
      OtaUpdater().checkOnly(taskParams->config, nullptr, nullptr);
  queuedResult.code = result.code;
  copyOtaLabel(queuedResult.currentVersion, sizeof(queuedResult.currentVersion),
               result.currentVersion);
  copyOtaLabel(queuedResult.latestVersion, sizeof(queuedResult.latestVersion),
               result.latestVersion);
  copyOtaLabel(queuedResult.summary, sizeof(queuedResult.summary), result.summary);
  copyOtaLabel(queuedResult.detail, sizeof(queuedResult.detail), result.detail);

  if (taskParams->resultQueue != nullptr) {
    xQueueOverwrite(taskParams->resultQueue, &queuedResult);
  }

  delete taskParams;
  vTaskDelete(nullptr);
}

void App::pollOtaCheckResult(uint32_t nowMs) {
  (void)nowMs;
  if (otaCheckQueue_ == nullptr) {
    return;
  }

  OtaCheckResult result;
  while (xQueueReceive(otaCheckQueue_, &result, 0) == pdTRUE) {
    otaCheckInProgress_ = false;
    applyStateCpuFrequency();
    Serial.printf("[ota] background result code=%u current=%s latest=%s summary=%s detail=%s\n",
                  static_cast<unsigned int>(result.code), result.currentVersion,
                  result.latestVersion, result.summary, result.detail);

    if (result.code == OtaUpdater::ResultCode::UpdateAvailable) {
      pendingUpdateCurrentVersion_ = String(result.currentVersion);
      pendingUpdateNewVersion_ = String(result.latestVersion);
      otaUpdatePromptPending_ = true;
    }
  }
}

bool App::updateConfirmCanOpen() const {
  return otaUpdatePromptPending_ && !pendingBootBookLoad_ && state_ == AppState::Paused;
}

void App::maybeOpenUpdateConfirm(uint32_t nowMs) {
  if (!updateConfirmCanOpen()) {
    return;
  }

  otaUpdatePromptPending_ = false;
  setState(AppState::Menu, nowMs);
  openUpdateConfirm();
}

bool App::blockNetworkActionForOtaCheck(const String &title, uint32_t nowMs) {
  pollOtaCheckResult(nowMs);
  if (!otaCheckInProgress_) {
    return false;
  }

  display_.renderStatus(title, "OTA check running", "Try again soon");
  delay(1200);
  renderMenu();
  return true;
}

void App::runFirmwareUpdate(const OtaUpdater::Config &config, bool automatic, uint32_t nowMs) {
  if (blockNetworkActionForOtaCheck("OTA", nowMs)) {
    return;
  }

  if (!automatic) {
    otaUpdatePromptPending_ = false;
  }

  if (!otaUpdater_.isConfigured(config)) {
    if (!automatic) {
      display_.renderStatus("OTA", "Wi-Fi not set", "Settings -> Wi-Fi");
      delay(1600);
      if (state_ == AppState::Menu && isSettingsMenuScreen(menuScreen_)) {
        rebuildSettingsMenuItems();
        renderSettings();
      } else {
        menuScreen_ = MenuScreen::Main;
        setState(AppState::Paused, nowMs);
      }
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
  if (state_ == AppState::Menu && isSettingsMenuScreen(menuScreen_)) {
    rebuildSettingsMenuItems();
    renderSettings();
  } else {
    menuScreen_ = MenuScreen::Main;
    setState(AppState::Paused, nowMs);
  }
}

void App::runRssFeedCheck(uint32_t nowMs) {
  (void)nowMs;
  if (blockNetworkActionForOtaCheck("RSS", nowMs)) {
    return;
  }

  saveReadingPosition(true);

  display_.renderStatus("RSS", "Checking feeds", "Please wait");
  const RssFeedManager::Result result =
      rssFeedManager_.checkFeeds(preferredOtaConfig(), preferences_, &App::handleStorageStatus, this);

  Serial.printf("[rss] feeds=%u saved=%u skipped=%u summary=%s detail=%s\n",
                static_cast<unsigned int>(result.feedsChecked),
                static_cast<unsigned int>(result.articlesSaved),
                static_cast<unsigned int>(result.articlesSkipped), result.summary.c_str(),
                result.detail.c_str());

  storage_.refreshBooks(false);
  display_.renderStatus("RSS", result.summary, result.detail);
  delay(1800);
  if (state_ == AppState::Menu && menuScreen_ == MenuScreen::Articles) {
    renderArticlesMenu();
    return;
  }
  renderMainMenu();
}

String App::pacingDelayLabel(uint16_t delayMs) const { return String(delayMs) + " ms"; }

String App::firmwareUpdateMenuLabel() const { return "Firmware update"; }

String App::firmwareVersionLabel() const {
#ifdef RSVP_FIRMWARE_VERSION
  return String(RSVP_FIRMWARE_VERSION);
#else
  return "dev";
#endif
}

String App::uiText(UiText key) const { return Localization::text(uiLanguage_, key); }

String App::themeModeLabel() const {
  if (yellowModeEnabled_) {
    return "Yellow";
  }
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

String App::pauseModeLabel() const {
  return pauseMode_ == PauseMode::Instant ? "Instant" : "Sentence";
}

String App::handednessLabel() const {
  return handednessMode_ == HandednessMode::Left ? "Left" : "Right";
}

String App::readerControlsLayoutLabel() const {
  return readerControlsSwapped_ ? "Rewind top-right" : "Standard";
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

void App::openBookPicker(bool articlesOnly) {
  storage_.refreshBooks();
  bookPickerArticlesOnly_ = articlesOnly;
  bookMenuItems_.clear();
  bookPickerBookIndices_.clear();
  bookMenuItems_.push_back({uiText(UiText::Back), ""});

  const size_t count = storage_.bookCount();
  std::vector<size_t> sortedBookIndices;
  sortedBookIndices.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    if (storage_.bookIsArticle(i) != articlesOnly) {
      continue;
    }
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

  if (sortedBookIndices.empty()) {
    Serial.printf("[book-picker] No SD %s available\n", articlesOnly ? "articles" : "books");
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
    if (Board::Config::ENABLE_RESTRUCTURED_MENU && bookPickerArticlesOnly_) {
      menuScreen_ = MenuScreen::Articles;
      renderArticlesMenu();
      return;
    }
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
    display_.renderStatus("Book open failed", storage_.bookDisplayName(bookIndex),
                          "Check serial log");
    delay(1800);
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

void App::openSdCardRepairConfirm() {
  sdCardRepairConfirmSelectedIndex_ = SdCardRepairConfirmNo;
  menuScreen_ = MenuScreen::SdCardRepairConfirm;
  renderSdCardRepairConfirm();
}

void App::selectSdCardRepairConfirmItem(uint32_t nowMs) {
  if (sdCardRepairConfirmSelectedIndex_ != SdCardRepairConfirmYes) {
    Serial.println("[sd-check] folder repair declined");
    menuScreen_ = MenuScreen::Main;
    renderMenu();
    return;
  }

  runSdCardRepair(nowMs);
}

void App::openUpdateConfirm() {
  updateConfirmSelectedIndex_ = UpdateConfirmSkip;
  menuScreen_ = MenuScreen::UpdateConfirm;
  renderUpdateConfirm();
}

void App::selectUpdateConfirmItem(uint32_t nowMs) {
  if (updateConfirmSelectedIndex_ != UpdateConfirmUpdate) {
    Serial.println("[ota] update skipped by user");
    menuScreen_ = MenuScreen::Main;
    setState(AppState::Paused, nowMs);
    return;
  }

  Serial.println("[ota] update confirmed by user");
  runFirmwareUpdate(preferredOtaConfig(), false, nowMs);
}

void App::openPowerOffConfirm(uint32_t nowMs) {
  powerOffConfirmReturnState_ = state_;
  powerOffConfirmReturnScreen_ = state_ == AppState::Menu ? menuScreen_ : MenuScreen::Main;
  powerOffConfirmSelectedIndex_ = PowerOffConfirmNo;
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;
  contextViewVisible_ = false;

  if (state_ != AppState::Menu) {
    saveReadingPosition(true);
  }

  menuScreen_ = MenuScreen::PowerOffConfirm;
  if (state_ == AppState::Menu) {
    renderPowerOffConfirm();
  } else {
    setState(AppState::Menu, nowMs);
  }
}

void App::cancelPowerOffConfirm(uint32_t nowMs) {
  Serial.println("[power-off] cancelled by user");
  const AppState returnState = powerOffConfirmReturnState_;
  const MenuScreen returnScreen = powerOffConfirmReturnScreen_;
  menuScreen_ = returnScreen;

  if (returnState == AppState::Menu) {
    renderMenu();
    return;
  }

  if (returnState == AppState::Playing) {
    setState(AppState::Paused, nowMs);
    return;
  }

  setState(returnState, nowMs);
}

void App::selectPowerOffConfirmItem(uint32_t nowMs) {
  if (powerOffConfirmSelectedIndex_ != PowerOffConfirmYes) {
    cancelPowerOffConfirm(nowMs);
    return;
  }

  Serial.println("[power-off] confirmed by user");
  enterPowerOff(nowMs);
}

void App::enterCompanionSync(uint32_t nowMs) {
  if (blockNetworkActionForOtaCheck("Sync", nowMs)) {
    return;
  }

  Serial.println("[app] entering companion sync mode");
  saveReadingPosition(true);
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;
  display_.renderStatus("Sync", "Starting Wi-Fi", "");

  OtaUpdater::Config wifiConfig = preferredOtaConfig();
  CompanionSyncManager::Config syncConfig;
  syncConfig.wifiSsid = wifiConfig.wifiSsid;
  syncConfig.wifiPassword = wifiConfig.wifiPassword;

  if (!companionSync_.begin(syncConfig)) {
    Serial.println("[app] companion sync failed");
    display_.renderStatus("Sync", "Could not start", "Returning");
    delay(1400);
    menuScreen_ = MenuScreen::Main;
    setState(AppState::Menu, nowMs);
    return;
  }

  lastCompanionSyncRenderMs_ = 0;
  setState(AppState::CompanionSync, nowMs);
}

void App::updateCompanionSync(uint32_t nowMs) {
  companionSync_.update();

  if (powerButton_.isHeld() && nowMs - powerButton_.lastEdgeMs() >= kUsbTransferExitHoldMs) {
    powerButtonLongPressHandled_ = true;
    exitCompanionSync(nowMs);
    return;
  }

  if (nowMs - lastCompanionSyncRenderMs_ >= 1000) {
    lastCompanionSyncRenderMs_ = nowMs;
    display_.renderStatus("Sync", companionSync_.statusLine1(), companionSync_.statusLine2());
  }
}

void App::exitCompanionSync(uint32_t nowMs) {
  Serial.println("[app] leaving companion sync mode");
  display_.renderStatus("Sync", "Stopping", "");
  companionSync_.end();
  preferences_.end();
  preferences_.begin(kPrefsNamespace, false);
  reloadRuntimePreferences(nowMs, false);
  storage_.refreshBooks();
  menuScreen_ = MenuScreen::Main;
  setState(AppState::Paused, nowMs);
}

void App::runSdCardCheck(uint32_t nowMs) {
  (void)nowMs;
  Serial.println("[app] running SD card check");
  display_.renderStatus("SD check", "Starting", "");
  const StorageManager::DiagnosticResult result = storage_.diagnoseSdCard();

  if (sdCardFolderRepairNeeded(result)) {
    display_.renderStatus("SD check", "Folders missing", "Confirm repair");
    delay(900);
    openSdCardRepairConfirm();
    return;
  }

  String detail = result.detail;
  if (detail.isEmpty() && result.mounted) {
    detail = String(static_cast<unsigned int>(result.sizeMb)) + " MB";
  }
  display_.renderStatus("SD check", result.summary, detail);
  delay(2600);

  if (Board::Config::ENABLE_RESTRUCTURED_MENU && menuScreen_ == MenuScreen::SettingsHome) {
    settingsSelectedIndex_ = kSettingsHomeRestructuredSdCardIndex;
    rebuildSettingsMenuItems();
    renderSettings();
    return;
  }

  menuScreen_ = MenuScreen::Main;
  renderMenu();
}

void App::runSdCardRepair(uint32_t nowMs) {
  (void)nowMs;
  Serial.println("[app] repairing SD card folder layout");
  display_.renderStatus("SD check", "Repairing folders", "Please wait");
  const bool repaired = storage_.repairSdCardFolders();
  if (!repaired) {
    display_.renderStatus("SD check", "Folder repair failed", "Format FAT32 MBR");
    delay(2600);
    menuScreen_ = MenuScreen::Main;
    renderMenu();
    return;
  }

  display_.renderStatus("SD check", "Folders repaired", "Checking card");
  delay(900);

  const StorageManager::DiagnosticResult result = storage_.diagnoseSdCard();
  String detail = result.detail;
  if (detail.isEmpty() && result.mounted) {
    detail = String(static_cast<unsigned int>(result.sizeMb)) + " MB";
  }
  display_.renderStatus("SD check", result.summary, detail);
  delay(2600);

  menuScreen_ = MenuScreen::Main;
  renderMenu();
}

void App::enterUsbTransfer(uint32_t nowMs) {
  Serial.println("[app] entering USB transfer mode");
  saveReadingPosition(true);
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;
  const size_t resumeIndex = reader_.currentIndex();
  const bool storageWasReady = storageReady_;
  setState(AppState::UsbTransfer, nowMs);

  activeBookStore_.close();
  if (!usbTransfer_.begin(true)) {
    Serial.printf("[app] USB transfer failed: %s\n", usbTransfer_.statusMessage());
    display_.renderStatus("USB", usbTransfer_.statusMessage(), "Returning");
    storageReady_ = storageWasReady ? true : storage_.begin();
    if (storageReady_ && usingStorageBook_ && !currentBookPath_.isEmpty()) {
      const int refreshedBookIndex = findBookIndexByPath(currentBookPath_);
      BookOpenOptions reloadOptions;
      reloadOptions.allowIndexBuild = false;
      reloadOptions.allowEpubConversion = false;
      reloadOptions.rebuildTimeEstimate = false;
      if (refreshedBookIndex >= 0 &&
          loadBookAtIndex(static_cast<size_t>(refreshedBookIndex), nowMs,
                          reloadOptions)) {
        reader_.seekTo(resumeIndex);
      }
    }
    setState(AppState::Paused, nowMs);
    return;
  }

  const uint64_t sizeMb = usbTransfer_.cardSizeBytes() / (1024ULL * 1024ULL);
  Serial.printf("[app] USB transfer active (%llu MB). Eject from computer when finished.\n",
                sizeMb);
  display_.renderStatus("USB", "Copy books now", "Eject when done");
}

void App::updateUsbTransfer(uint32_t nowMs) {
  if (!usbTransfer_.active()) {
    return;
  }

  const bool powerExitRequested =
      powerButton_.isHeld() && nowMs - powerButton_.lastEdgeMs() >= kUsbTransferExitHoldMs;
  if (!usbTransfer_.ejected() && !powerExitRequested) {
    return;
  }

  if (powerExitRequested && !usbTransfer_.ejected()) {
    Serial.println("[app] leaving USB transfer by PWR hold; make sure host was ejected first");
  }

  if (powerExitRequested) {
    powerButtonLongPressHandled_ = true;
  }

  exitUsbTransfer(nowMs);
}

void App::exitUsbTransfer(uint32_t nowMs) {
  Serial.println("[app] USB transfer ejected; remounting SD");
  display_.renderStatus("USB", "Remounting SD", "");
  usbTransfer_.end();
  storage_.end();

  storageReady_ = storage_.begin();
  if (storageReady_) {
    const int refreshedBookIndex = findBookIndexByPath(currentBookPath_);
    if (refreshedBookIndex >= 0) {
      const size_t resumeIndex = reader_.currentIndex();
      BookOpenOptions reloadOptions;
      reloadOptions.allowIndexBuild = false;
      reloadOptions.allowEpubConversion = false;
      reloadOptions.rebuildTimeEstimate = false;
      if (loadBookAtIndex(static_cast<size_t>(refreshedBookIndex), nowMs,
                          reloadOptions)) {
        reader_.seekTo(resumeIndex);
      } else {
        Serial.println("[app] current indexed book unavailable after USB transfer");
        usingStorageBook_ = false;
        currentBookPath_ = "";
        currentBookTitle_ = "Demo";
        reader_.clearLoadedBook(nowMs);
        reader_.begin(nowMs);
      }
    } else if (storage_.bookCount() > 0) {
      loadBookAtIndex(0, nowMs);
    }
  } else {
    Serial.println("[app] SD remount failed after USB transfer");
  }

  menuScreen_ = MenuScreen::Main;
  setState(AppState::Paused, nowMs);
}

void App::enterStandby(uint32_t nowMs) {
  if (state_ == AppState::UsbTransfer || state_ == AppState::CompanionSync ||
      state_ == AppState::Sleeping || powerOffStarted_) {
    return;
  }

  standbyReturnState_ = state_ == AppState::Playing ? AppState::Paused : state_;
  if (standbyReturnState_ == AppState::Booting || standbyReturnState_ == AppState::Standby) {
    standbyReturnState_ = AppState::Paused;
  }

  if (state_ == AppState::Playing) {
    saveReadingPosition(true);
  }

  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  playLocked_ = false;
  pauseAtSentenceEndRequested_ = false;
  contextViewVisible_ = false;
  wpmFeedbackVisible_ = false;
  batteryWarningOverlayVisible_ = false;
  standbyEnteredMs_ = nowMs;
  standbyButtonsReleased_ = false;
  lastStandbyFrameMs_ = 0;
  setState(AppState::Standby, nowMs);
  Serial.println("[app] standby screensaver started");
}

void App::exitStandby(uint32_t nowMs) {
  if (state_ != AppState::Standby) {
    return;
  }

  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  playLocked_ = false;
  pauseAtSentenceEndRequested_ = false;
  batteryWarningOverlayVisible_ = false;
  standbyButtonsReleased_ = false;
  lastActivityMs_ = nowMs;

  AppState nextState = standbyReturnState_;
  if (nextState == AppState::Booting || nextState == AppState::Playing ||
      nextState == AppState::CompanionSync || nextState == AppState::UsbTransfer ||
      nextState == AppState::Standby || nextState == AppState::Sleeping) {
    nextState = AppState::Paused;
  }

  Serial.println("[app] leaving standby");
  if (standbyScreenOffActive_) {
    display_.wakeFromSleep();
    standbyScreenOffActive_ = false;
  }
  setState(nextState, nowMs);
}

uint32_t App::standbyRngSeed(uint32_t nowMs) const {
  return nowMs ^ micros() ^ (static_cast<uint32_t>(reader_.currentIndex() + 1) * 2654435761UL) ^
         (static_cast<uint32_t>(batteryDisplayedPercent_) << 24);
}

void App::seedStandbyScreensaver(uint32_t nowMs) {
  if (screensaverMode_ != ScreensaverMode::ScreenOff && standbyScreenOffActive_) {
    display_.wakeFromSleep();
    standbyScreenOffActive_ = false;
  }

  if (screensaverMode_ == ScreensaverMode::ScreenOff) {
    screensaver_.reset();
    seedStandbyScreenOff(nowMs);
    return;
  }

  standby::Kind kind = standby::Kind::Life;
  switch (screensaverMode_) {
    case ScreensaverMode::Maze:
      kind = standby::Kind::Maze;
      break;
    case ScreensaverMode::Voronoi:
      kind = standby::Kind::Voronoi;
      break;
    case ScreensaverMode::Life:
    default:
      kind = standby::Kind::Life;
      break;
  }
  screensaver_ = standby::makeScreensaver(kind, kStandbyLifeColumns, kStandbyLifeRows);
  screensaver_->seed(standbyRngSeed(nowMs));
}

void App::stepStandbyScreensaver(uint32_t nowMs) {
  (void)nowMs;
  if (screensaver_) {
    screensaver_->step();
  }
}

void App::seedStandbyScreenOff(uint32_t nowMs) {
  (void)nowMs;
  screensaver_.reset();
  standbyScreenOffActive_ = true;
  display_.prepareForSleep();
}

void App::updateStandbyScreensaver(uint32_t nowMs, bool force) {
  if (state_ != AppState::Standby) {
    return;
  }

  if (screensaverMode_ == ScreensaverMode::ScreenOff) {
    if (!standbyScreenOffActive_) {
      seedStandbyScreenOff(nowMs);
    }
    lastStandbyFrameMs_ = nowMs;
    return;
  }

  if (!force && nowMs - lastStandbyFrameMs_ < kStandbyFrameMs) {
    return;
  }

  if (!force) {
    stepStandbyScreensaver(nowMs);
  } else if (!screensaver_) {
    seedStandbyScreensaver(nowMs);
  }

  lastStandbyFrameMs_ = nowMs;
  if (screensaver_) {
    const standby::Frame frame = screensaver_->frame();
    display_.renderLifeScreensaver(*frame.cells, kStandbyLifeColumns, kStandbyLifeRows,
                                   frame.generation, frame.dimCells);
  }
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
  contextViewVisible_ = false;
  wpmFeedbackVisible_ = false;
  menuScreen_ = MenuScreen::Main;
  state_ = AppState::Sleeping;

  const int wakePin = Board::Config::PIN_DEEP_SLEEP_WAKE;
  const bool pmuPowerOffOwnsWake = Board::Power::powerOffUsesControllerWake();
  const bool useRecoverableSoftOff = Board::Config::SOFTWARE_POWEROFF_USES_SOFT_LOOP;
  const bool allowSoftOffPowerWake = Board::Config::SOFT_OFF_WAKE_USES_POWER_BUTTON;
  const bool allowSoftOffBootWake = Board::Config::SOFT_OFF_WAKE_USES_BOOT_BUTTON;
  const bool externalPowerPresent = pmuPowerOffOwnsWake && Board::Power::externalPowerPresent();
  const bool pmuWakeUsesPowerButton = pmuPowerOffOwnsWake || useRecoverableSoftOff;
  const bool appPowerUsesBootButton = Board::Config::SWAP_APP_BOOT_AND_POWER_BUTTONS;
  const char *wakeLabel = useRecoverableSoftOff && allowSoftOffPowerWake && allowSoftOffBootWake
                              ? "Press PWR/BOOT to wake"
                          : useRecoverableSoftOff && allowSoftOffPowerWake
                              ? "Press PWR to wake"
                          : useRecoverableSoftOff && allowSoftOffBootWake
                              ? "Press BOOT to wake"
                          : pmuWakeUsesPowerButton
                              ? (externalPowerPresent ? "Press PWR to wake" : "Press PWR to start")
                          : wakePin >= 0 && wakePin == Board::Config::PIN_BOOT_BUTTON
                              ? "Press BOOT to start"
                          : wakePin >= 0 && wakePin == Board::Config::PIN_PWR_BUTTON
                              ? "Hold PWR to start"
                              : "Press wake to start";
  display_.renderStatus("OFF", appPowerUsesBootButton ? "Release BOOT" : "Release PWR",
                        wakeLabel);
  delay(300);

  if (pmuPowerOffOwnsWake || Board::Buttons::usesPowerEvents()) {
    const uint32_t waitStartMs = millis();
    const uint32_t releaseWaitMs =
        useRecoverableSoftOff ? kSoftOffReleaseWaitMs : kPowerOffReleaseWaitMs;
    while (((allowSoftOffPowerWake || !useRecoverableSoftOff) && readLogicalPowerButtonHeld()) &&
           millis() - waitStartMs < releaseWaitMs) {
      delay(10);
    }
  }

  display_.prepareForSleep();

  activeBookStore_.close();
  storage_.end();
  Input::Touch::end();
  touchInitialized_ = false;
  if (!useRecoverableSoftOff) {
    Serial.flush();
  }

  Board::System::holdBacklightOffForDeepSleep();
  const bool requestPmuShutdown =
      Board::Power::shouldRequestShutdownOnPowerOff() && !externalPowerPresent &&
      !useRecoverableSoftOff;
  if (requestPmuShutdown || Board::Power::shouldReleaseBatteryPowerBeforeDeepSleep()) {
    Board::Power::releaseBatteryPowerHold();
  } else if (useRecoverableSoftOff) {
    Serial.println("[app] recoverable soft-off; PMU shutdown skipped");
  } else if (pmuPowerOffOwnsWake && externalPowerPresent) {
    Serial.println("[app] USB power present; using recoverable soft-off");
  }

  if (pmuPowerOffOwnsWake || useRecoverableSoftOff) {
    if (requestPmuShutdown) {
      Serial.println("[app] waiting for PMU power cut");
      Serial.flush();
      delay(1500);
      Serial.println("[app] PMU power cut not observed; using soft-off fallback");
    }

    // With USB attached the PMU may keep VSYS alive. Stay recoverable instead of leaving the
    // reader black in an infinite loop that can only be fixed with reset/reflash.
    Serial.println("[app] soft-off; arming wake buttons");
    if (!useRecoverableSoftOff) {
      Serial.flush();
    }
    const SoftOffWakeSource wakeSource =
        waitForRecoverableSoftOffWake(allowSoftOffPowerWake, allowSoftOffBootWake);
    Serial.printf("[diag] soft_off_wake=%s\n", softOffWakeSourceName(wakeSource));
    wakeFromSleep(true);
    return;
  }

  if (Board::Power::shouldRequestShutdownOnPowerOff()) {
    delay(60);
  }

  const bool wakeUsesPowerButton = wakePin >= 0 && wakePin == Board::Config::PIN_PWR_BUTTON;
  const bool wakeUsesSwappedBootButton =
      Board::Config::SWAP_APP_BOOT_AND_POWER_BUTTONS &&
      wakePin >= 0 && wakePin == Board::Config::PIN_BOOT_BUTTON;
  if (wakeUsesPowerButton || wakeUsesSwappedBootButton) {
    const uint32_t waitStartMs = millis();
    while (readActiveLowButton(wakePin) && millis() - waitStartMs < kPowerOffReleaseWaitMs) {
      delay(10);
    }
  }

  Board::System::deepSleepUntilConfiguredWake();
}

void App::enterSleep(uint32_t nowMs) {
  Serial.println("[app] entering light sleep; press BOOT to wake");
  saveReadingPosition(true);
  setState(AppState::Sleeping, nowMs);
  Serial.flush();
  delay(200);

  display_.prepareForSleep();
  activeBookStore_.close();
  storage_.end();
  Input::Touch::end();
  touchInitialized_ = false;

  Board::System::lightSleepUntilBootButton();
  wakeFromSleep();
}

void App::wakeFromSleep(bool fullPeripheralReset) {
  const uint32_t nowMs = millis();
  Serial.println(fullPeripheralReset ? "[app] woke from soft-off" : "[app] woke from light sleep");

  Board::System::begin();
  if (fullPeripheralReset) {
    Board::System::resetWakePeripherals();
  }
  button_.beginWithState(readLogicalBootButtonHeld());
  powerButton_.beginWithState(readFirmwarePowerButtonHeld());
  if (Board::Config::FIRMWARE_POWER_BUTTON_ENABLED &&
      (Board::Config::PIN_PWR_BUTTON < 0 || Board::Config::SWAP_APP_BOOT_AND_POWER_BUTTONS)) {
    Board::Buttons::consumeVirtualPowerShortPress();
    Board::Buttons::consumeVirtualPowerLongPress();
  }
  keyButton_.begin();
  bootButtonReleasedSinceBoot_ = !button_.isHeld();
  bootButtonLongPressHandled_ = false;
  powerButtonReleasedSinceBoot_ =
      !Board::Config::FIRMWARE_POWER_BUTTON_ENABLED
          ? true
          : (logicalPowerButtonUsesVirtualState() ? false : !powerButton_.isHeld());
  powerButtonLongPressHandled_ = false;
  powerButtonEventArmMs_ = Board::Buttons::usesPowerEvents()
                               ? millis() + Board::Buttons::powerEventIgnoreMs()
                               : 0;
  keyButtonReleasedSinceBoot_ = !keyButton_.isHeld();
  keyButtonLongPressHandled_ = false;
  keyButtonTapArmed_ = false;
  powerOffStarted_ = false;
  updateBatteryStatus(nowMs, true);
  storage_.setStatusCallback(&App::handleStorageStatus, this);
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;
  menuScreen_ = MenuScreen::Main;
  lastStateLogMs_ = nowMs;
  state_ = AppState::Paused;

  const bool displayReady = fullPeripheralReset ? display_.begin() : display_.wakeFromSleep();
  touchInitialized_ = Input::Touch::begin();
  storageReady_ = storage_.begin();

  if (storageReady_ && usingStorageBook_ && !currentBookPath_.isEmpty()) {
    const size_t resumeIndex = reader_.currentIndex();
    const int refreshedBookIndex = findBookIndexByPath(currentBookPath_);
    BookOpenOptions reloadOptions;
    reloadOptions.allowIndexBuild = false;
    reloadOptions.allowEpubConversion = false;
    reloadOptions.rebuildTimeEstimate = false;
    if (refreshedBookIndex >= 0 &&
        loadBookAtIndex(static_cast<size_t>(refreshedBookIndex), nowMs,
                        reloadOptions)) {
      reader_.seekTo(resumeIndex);
    } else {
      Serial.println("[app] current indexed book unavailable after wake");
      usingStorageBook_ = false;
      currentBookPath_ = "";
      currentBookTitle_ = "Demo";
      reader_.clearLoadedBook(nowMs);
      reader_.begin(nowMs);
    }
  }

  if (displayReady) {
    renderActiveReader(nowMs);
  }
  applyStateCpuFrequency();
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

  BookOpenOptions loadOptions;
  loadOptions.allowLegacyPositionFallback = true;
  loadOptions.allowIndexBuild = false;
  loadOptions.allowEpubConversion = false;
  loadOptions.rebuildTimeEstimate = false;
  if (!loadBookAtIndex(static_cast<size_t>(bookIndex), nowMs, loadOptions)) {
    return false;
  }

  Serial.printf("[app] restored %s at word %u\n", savedPath.c_str(),
                static_cast<unsigned int>(reader_.currentIndex()));
  return true;
}

bool App::prepareBootBookLoad() {
  pendingBootBookIndex_ = 0;
  pendingBootBookLegacyFallback_ = false;

  if (!storageReady_ || storage_.bookCount() == 0) {
    return false;
  }

  const String savedPath = preferences_.getString(kPrefBookPath, "");
  if (!savedPath.isEmpty()) {
    const int savedBookIndex = findBookIndexByPath(savedPath);
    if (savedBookIndex >= 0) {
      pendingBootBookIndex_ = static_cast<size_t>(savedBookIndex);
      pendingBootBookLegacyFallback_ = true;
      Serial.printf("[app] deferred saved book load: %s\n", savedPath.c_str());
      return true;
    }

    Serial.printf("[app] saved book not found: %s\n", savedPath.c_str());
  }

  pendingBootBookIndex_ = 0;
  pendingBootBookLegacyFallback_ = false;
  Serial.println("[app] deferred first book load");
  return true;
}

void App::loadPendingBootBook(uint32_t nowMs) {
  if (!pendingBootBookLoad_ || state_ != AppState::Paused) {
    return;
  }

  pendingBootBookLoad_ = false;
  display_.renderStatus("Loading book", currentBookTitle_, "Please wait");
  const uint32_t startedMs = millis();
  BookOpenOptions loadOptions;
  loadOptions.allowLegacyPositionFallback = pendingBootBookLegacyFallback_;
  loadOptions.allowIndexBuild = pendingBootBookLegacyFallback_;
  loadOptions.allowEpubConversion = false;
  loadOptions.rebuildTimeEstimate = false;
  const bool loaded =
      loadBookAtIndex(pendingBootBookIndex_, nowMs, loadOptions);
  const uint32_t elapsedMs = millis() - startedMs;
  Serial.printf("[app] deferred book load %s in %lu ms\n", loaded ? "ok" : "failed",
                static_cast<unsigned long>(elapsedMs));

  if (loaded) {
    usingStorageBook_ = true;
    renderActiveReader(millis());
    return;
  }

  usingStorageBook_ = false;
  chapterMarkers_.clear();
  paragraphStarts_.clear();
  currentBookPath_ = "";
  currentBookTitle_ = "Demo";
  reader_.begin(millis());
  invalidateContextPreviewWindow();
  Serial.println("[app] using built-in demo text");
  renderActiveReader(millis());
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

bool App::loadBookAtIndex(size_t index, uint32_t nowMs,
                          const BookOpenOptions &options) {
  BookMetadata book;
  String loadedPath;
  size_t loadedIndex = index;

  {
    // Open or build the indexed backing store before attaching it to reader.
    const String initialLabel = storage_.bookDisplayName(index);
    renderStorageStatus(
        "Opening book", initialLabel.c_str(),
        options.allowIndexBuild ? "Checking index" : "Checking saved index", 5);

    StorageManager::IndexedBookLoadOptions loadOptions;
    loadOptions.loadedPath = &loadedPath;
    loadOptions.loadedIndex = &loadedIndex;
    loadOptions.allowIndexBuild = options.allowIndexBuild;
    loadOptions.allowEpubConversion = options.allowEpubConversion;
    if (!storage_.loadIndexedBook(index, activeBookStore_, book, loadOptions)) {
      return false;
    }
  }

  const String loadedTitle = [&]() {
    if (!book.title.isEmpty()) {
      return book.title;
    }
    return displayNameForPath(loadedPath);
  }();
  const bool keepingExistingTimeCache = !options.rebuildTimeEstimate &&
                                        timeEstimateCacheValid_ &&
                                        currentBookPath_ == loadedPath;

  {
    // Attach the indexed store and force the first word read while errors are visible.
    renderStorageStatus("Opening book", loadedTitle.c_str(),
                        "Loading word cache", 70);
    reader_.setWordSource(&activeBookStore_, nowMs);
    if (reader_.wordCount() == 0 || reader_.currentWord().isEmpty()) {
      Serial.printf("[app] failed to read first indexed word from %s\n", loadedPath.c_str());
      activeBookStore_.close();
      reader_.clearLoadedBook(nowMs);
      renderStorageStatus("Book open failed", loadedTitle.c_str(), "Word cache unreadable", 100);
      return false;
    }
  }

  chapterMarkers_ = std::move(book.chapters);
  paragraphStarts_ = std::move(book.paragraphStarts);
  invalidateContextPreviewWindow();
  currentBookIndex_ = loadedIndex;
  currentBookPath_ = loadedPath;
  currentBookTitle_ = loadedTitle;
  lastSavedWordIndex_ = static_cast<size_t>(-1);
  usingStorageBook_ = true;
  preferences_.putString(kPrefBookPath, currentBookPath_);
  preferences_.putUInt(bookWordCountKey(currentBookPath_).c_str(),
                       static_cast<uint32_t>(reader_.wordCount()));
  markBookRecent(currentBookPath_);

  {
    // Restore saved position after the active book identity has been committed.
    const uint32_t savedWordIndex = savedWordIndexForBook(
        currentBookPath_, options.allowLegacyPositionFallback);
    if (savedWordIndex != kNoSavedWordIndex) {
      renderStorageStatus("Opening book", currentBookTitle_.c_str(), "Restoring position", 78);
      reader_.seekTo(savedWordIndex);
      lastSavedWordIndex_ = reader_.currentIndex();
      Serial.printf("[app] restored book position word=%u key=%s\n",
                    static_cast<unsigned int>(reader_.currentIndex()),
                    bookPositionKey(currentBookPath_).c_str());
    }
  }

  {
    // Keep, rebuild, or invalidate the time estimate based on how the book was opened.
    if (options.rebuildTimeEstimate) {
      rebuildTimeEstimateCache();
    } else if (!keepingExistingTimeCache) {
      invalidateTimeEstimateCache();
    } else {
      renderStorageStatus("Opening book", currentBookTitle_.c_str(), "Using cached estimate", 92);
    }
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

  if (isSettingsMenuScreen(menuScreen_)) {
    renderSettings();
  } else if (menuScreen_ == MenuScreen::Articles) {
    renderArticlesMenu();
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
  } else if (menuScreen_ == MenuScreen::SdCardRepairConfirm) {
    renderSdCardRepairConfirm();
  } else if (menuScreen_ == MenuScreen::UpdateConfirm) {
    renderUpdateConfirm();
  } else if (menuScreen_ == MenuScreen::PowerOffConfirm) {
    renderPowerOffConfirm();
  } else if (menuScreen_ == MenuScreen::QuickSettings) {
    renderQuickSettings();
  } else if (menuScreen_ == MenuScreen::QuickSync) {
    renderQuickSync();
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
  items.reserve(Board::Config::ENABLE_RESTRUCTURED_MENU
                    ? static_cast<size_t>(RestructuredMenuItemCount)
                    : static_cast<size_t>(MenuItemCount));
  items.push_back(uiText(UiText::Resume));
  items.push_back(uiText(UiText::Chapters));
  items.push_back("Books");
  items.push_back("Articles");
  if (Board::Config::ENABLE_RESTRUCTURED_MENU) {
    items.push_back(uiText(UiText::Settings));
    items.push_back(uiText(UiText::PowerOff));
    display_.renderMenu(items, menuSelectedIndex_);
    return;
  }

  items.push_back("Focus Timer");
  items.push_back(uiText(UiText::Settings));
  items.push_back("SD card check");
  items.push_back("RSS feeds");
  items.push_back("Companion sync");
#if RSVP_USB_TRANSFER_ENABLED
  items.push_back(uiText(UiText::UsbTransfer));
#endif
  items.push_back(uiText(UiText::PowerOff));
  display_.renderMenu(items, menuSelectedIndex_);
}

void App::renderArticlesMenu() {
  std::vector<String> items;
  items.reserve(ArticlesItemCount);
  items.push_back(uiText(UiText::Back));
  items.push_back("Browse articles");
  items.push_back("Update RSS");
  display_.renderMenu(items, articlesSelectedIndex_);
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

void App::renderSdCardRepairConfirm() {
  std::vector<String> items;
  items.reserve(SdCardRepairConfirmItemCount + kSdCardRepairConfirmHeaderRows);
  items.push_back("Repair folders?");
  items.push_back("Not now");
  items.push_back("Create folders");

  display_.renderMenu(items, sdCardRepairConfirmSelectedIndex_ + kSdCardRepairConfirmHeaderRows);
}

void App::renderUpdateConfirm() {
  std::vector<String> items;
  items.reserve(UpdateConfirmItemCount + kUpdateConfirmHeaderRows);
  items.push_back("Update available");
  items.push_back(pendingUpdateCurrentVersion_ + " -> " + pendingUpdateNewVersion_);
  items.push_back("Skip for now");
  items.push_back("Update");

  display_.renderMenu(items, updateConfirmSelectedIndex_ + kUpdateConfirmHeaderRows);
}

void App::renderPowerOffConfirm() {
  std::vector<String> items;
  items.reserve(PowerOffConfirmItemCount + kPowerOffConfirmHeaderRows);
  items.push_back("Power off?");
  items.push_back("Cancel");
  items.push_back("Yes");

  display_.renderMenu(items, powerOffConfirmSelectedIndex_ + kPowerOffConfirmHeaderRows);
}

void App::renderQuickSettings() {
  std::vector<String> items;
  items.reserve(QuickSettingsItemCount);
  items.push_back(String("Brightness: ") + String(currentBrightnessPercent()) + "%");
  items.push_back(String("Theme: ") + themeModeLabel());
  items.push_back("Focus Timer");
  items.push_back("Sync");
  display_.renderMenu(items, quickSettingsSelectedIndex_);
}

void App::renderQuickSync() {
  std::vector<String> items;
  items.reserve(QuickSyncItemCount);
  items.push_back("Wi-Fi Sync");
  items.push_back("USB Sync");
  display_.renderMenu(items, quickSyncSelectedIndex_);
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
      display_.renderFocusTimerScreen("BEGIN", "",
                                      formatFocusTimerDuration(
                                          focusTimer_.selectedTouchDurationMs()),
                                      "Swipe to Change\nPlace to Start");
      return;
    case FocusTimer::State::TouchRunning:
      display_.renderFocusTimerScreen("BEGIN", "", remainingLabel, "",
                                      "Restart", focusTimer_.progressPercent(millis()));
      return;
    case FocusTimer::State::WaitAfterTouch:
      display_.renderFocusTimerScreen("BEGIN", "",
                                      formatFocusTimerDuration(
                                          focusTimer_.selectedTouchDurationMs()),
                                      "Flip to restart");
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
      display_.renderFocusTimerScreen("BREAK", "", "", "Place on side\nfor break", "",
                                      -1, true);
      return;
    case FocusTimer::State::WaitAfterBreak:
      display_.renderFocusTimerScreen("BEGIN", "", "", "Flip to restart");
      return;
    case FocusTimer::State::Cancelled:
      display_.renderFocusTimerScreen("BEGIN", "", "", "Place to begin again");
      return;
    case FocusTimer::State::Complete:
      display_.renderFocusTimerScreen("DONE", "", "", "Session complete");
      return;
  }
}

bool App::updateChapterTransition(uint32_t nowMs) {
  if (!chapterTransitionVisible_) {
    return false;
  }

  if (nowMs < chapterTransitionUntilMs_) {
    return true;
  }

  chapterTransitionVisible_ = false;
  reader_.start(nowMs);
  renderActiveReader(nowMs);
  return true;
}

bool App::maybeStartChapterTransition(size_t previousWordIndex, size_t currentWordIndex,
                                      uint32_t nowMs) {
  if (chapterMarkers_.empty() || currentWordIndex <= previousWordIndex) {
    return false;
  }

  for (size_t i = 0; i < chapterMarkers_.size(); ++i) {
    const size_t chapterWordIndex = chapterMarkers_[i].wordIndex;
    if (chapterWordIndex == 0 || chapterWordIndex <= previousWordIndex ||
        chapterWordIndex > currentWordIndex) {
      continue;
    }

    chapterTransitionIndex_ = i;
    chapterTransitionVisible_ = true;
    chapterTransitionUntilMs_ = nowMs + kChapterTransitionMs;
    contextViewVisible_ = false;
    wpmFeedbackVisible_ = false;
    reader_.seekTo(chapterWordIndex);
    renderChapterTransition();
    Serial.printf("[chapter] transition %u/%u word=%u title=%s\n",
                  static_cast<unsigned int>(i + 1),
                  static_cast<unsigned int>(chapterMarkers_.size()),
                  static_cast<unsigned int>(chapterWordIndex),
                  chapterMarkers_[i].title.c_str());
    return true;
  }

  return false;
}

void App::renderChapterTransition() {
  if (!chapterTransitionVisible_ || chapterTransitionIndex_ >= chapterMarkers_.size()) {
    return;
  }

  applyReaderUiOrientation();
  const String title = String("CHAPTER ") + String(chapterTransitionIndex_ + 1);
  String subtitle = chapterMarkers_[chapterTransitionIndex_].title;
  if (subtitle.length() > 42) {
    subtitle = subtitle.substring(0, 42) + "...";
  }
  display_.renderStatus(title, subtitle, "");
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
  const String fallback = currentBookTitle_.isEmpty() ? uiText(UiText::Start) : currentBookTitle_;
  if (chapterIndex >= chapterMarkers_.size()) {
    return fallback;
  }

  return cleanedChapterTitle(chapterMarkers_[chapterIndex].title, fallback);
}

String App::cleanedChapterTitle(const String &raw, const String &fallback) const {
  if (raw.isEmpty()) {
    return fallback;
  }

  size_t index = 0;
  while (index < static_cast<size_t>(raw.length()) && isDigit(raw[index])) {
    ++index;
  }

  if (index > 0 && index < static_cast<size_t>(raw.length()) && raw[index] == '.') {
    String cleaned = raw.substring(index + 1);
    cleaned.trim();
    return cleaned.isEmpty() ? fallback : cleaned;
  }

  return raw;
}

const char *App::chapterLabelPrefKey() const {
  return readerMode_ == ReaderMode::Scroll ? kPrefChapterLabelScroll : kPrefChapterLabelRsvp;
}

bool App::chapterLabelDefaultForMode(ReaderMode mode) { return mode != ReaderMode::Scroll; }

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
  const bool generatingEstimate = accurateTimeEstimateEnabled_ && timeEstimateBuildInProgress_ &&
                                  timeEstimateBuildMatchesCurrentBook();
  const int generatingPercent =
      generatingEstimate
          ? static_cast<int>((timeEstimateBuildNextBlock_ * 100UL) /
                             std::max<size_t>(1, timeEstimateBuildBlockCount_))
          : 0;

  if (footerMetricMode_ == FooterMetricMode::ChapterTime) {
    const size_t chapterIndex = currentChapterIndex();
    if (chapterIndex < chapterMarkers_.size() && chapterIndex + 1 < chapterMarkers_.size()) {
      endIndex = chapterMarkers_[chapterIndex + 1].wordIndex;
    }
    if (generatingEstimate) {
      return String("CH ") + String(generatingPercent) + "% gen";
    }
    return String("CH ") +
           formatReadingTimeRemaining(estimatedReadingTimeRemainingMs(currentIndex, endIndex));
  }

  if (generatingEstimate) {
    return String("BOOK ") + String(generatingPercent) + "% gen";
  }
  return String("BOOK ") +
         formatReadingTimeRemaining(estimatedReadingTimeRemainingMs(currentIndex, endIndex));
}

String App::currentBatteryLabel() const {
  if (!batteryPresent_ || !batterySampleInitialized_) {
    return "";
  }

  if (batteryLabelMode_ == BatteryLabelMode::TimeRemaining) {
    return batteryTimeRemainingLabel();
  }

  if (batteryLabelMode_ == BatteryLabelMode::Voltage) {
    return batteryVoltageLabel();
  }

  return String(static_cast<unsigned int>(batteryDisplayedPercent_)) + "%";
}

String App::footerMetricModeLabel() const {
  switch (footerMetricMode_) {
    case FooterMetricMode::ChapterTime:
      return "Chapter time";
    case FooterMetricMode::BookTime:
      return "Book time";
    case FooterMetricMode::Percentage:
    default:
      return "Percent read";
  }
}

String App::batteryLabelModeLabel() const {
  switch (batteryLabelMode_) {
    case BatteryLabelMode::TimeRemaining:
      return "Time remaining";
    case BatteryLabelMode::Voltage:
      return "Voltage";
    case BatteryLabelMode::Percent:
    default:
      return "Percentage";
  }
}

String App::screensaverModeLabel() const {
  switch (screensaverMode_) {
    case ScreensaverMode::Maze:
      return "Maze";
    case ScreensaverMode::Voronoi:
      return "Voronoi";
    case ScreensaverMode::ScreenOff:
      return "Screen off";
    case ScreensaverMode::Life:
    default:
      return "Life";
  }
}

String App::standbyTimerLabel() const {
  switch (standbyTimerIndex_) {
    case 1:
      return "1 min";
    case 2:
      return "5 min";
    case 3:
      return "10 min";
    case 4:
      return "30 min";
    case 0:
    default:
      return "Never";
  }
}

uint32_t App::standbyTimerMs() const {
  switch (standbyTimerIndex_) {
    case 1:
      return 60UL * 1000UL;
    case 2:
      return 5UL * 60UL * 1000UL;
    case 3:
      return 10UL * 60UL * 1000UL;
    case 4:
      return 30UL * 60UL * 1000UL;
    case 0:
    default:
      return 0;
  }
}

String App::batteryTimeRemainingLabel() const {
  if (batteryRuntimeEstimateReady_) {
    return formatBatteryTimeRemaining(batteryRuntimeMinutesRemaining_);
  }

  const uint32_t estimatedMinutes =
      (static_cast<uint32_t>(batteryDisplayedPercent_) * nominalBatteryRuntimeMinutes()) / 100UL;
  return formatBatteryTimeRemaining(estimatedMinutes);
}

String App::batteryVoltageLabel() const { return String(batteryFilteredVoltage_, 2) + "V"; }

uint32_t App::nominalBatteryRuntimeMinutes() const {
  auto mhzFactor = [](uint32_t mhz) -> int32_t {
    if (mhz <= 80) {
      return 90;
    }
    if (mhz >= 240) {
      return -60;
    }
    return 0;
  };

  int32_t minutes = static_cast<int32_t>(kNominalBatteryRuntimeMinutes);
  if (scrollModeEnabled()) {
    minutes += mhzFactor(cpuMhzScroll_);
  } else {
    minutes += mhzFactor(cpuMhzPlay_);
    minutes += mhzFactor(cpuMhzPaused_) / 4;
  }
  minutes += mhzFactor(cpuMhzMenu_) / 4;
  minutes += mhzFactor(cpuMhzStandby_) / 4;
  return static_cast<uint32_t>(std::max<int32_t>(60, minutes));
}

String App::cpuMhzLabel(uint32_t mhz) { return String(static_cast<unsigned int>(mhz)) + " MHz"; }

String App::autoDimDelayLabel() const {
  if (autoDimDelayMs_ == 0) {
    return "Off";
  }
  if (autoDimDelayMs_ <= 30000) {
    return "30s";
  }
  if (autoDimDelayMs_ <= 60000) {
    return "60s";
  }
  return "2min";
}

String App::autoDimBrightnessLabel() const {
  return autoDimBrightnessPercent_ == 0
             ? String("Screen off")
             : String(static_cast<unsigned int>(autoDimBrightnessPercent_)) + "%";
}

String App::formatBatteryTimeRemaining(uint32_t minutes) const {
  if (minutes < 1) {
    return "0m";
  }

  if (minutes < 60) {
    return String(minutes) + "m";
  }

  const uint32_t hours = minutes / 60;
  const uint32_t remainder = minutes % 60;
  if (hours >= 10 || remainder < 10) {
    return String(hours) + "h";
  }

  return String(hours) + "h" + String(remainder / 10) + "0";
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

  return baseMs + estimatedPacingBonusMs(startIndex, endIndex);
}

uint32_t App::estimatedPacingBonusMs(size_t startIndex, size_t endIndex) const {
  if (!timeEstimateCacheValid_ || wordBonusBlockPrefixSumMs_.empty() ||
      endIndex <= startIndex) {
    return 0;
  }

  const size_t wordCount = reader_.wordCount();
  startIndex = std::min(startIndex, wordCount);
  endIndex = std::min(endIndex, wordCount);
  if (endIndex <= startIndex) {
    return 0;
  }

  const size_t firstFullBlock = (startIndex + kTimeEstimateBlockWords - 1) /
                                kTimeEstimateBlockWords;
  const size_t lastFullBlockEnd = endIndex / kTimeEstimateBlockWords;
  uint32_t bonusMs = 0;

  if (firstFullBlock < lastFullBlockEnd &&
      lastFullBlockEnd < wordBonusBlockPrefixSumMs_.size()) {
    const size_t startPartialEnd =
        std::min(endIndex, firstFullBlock * kTimeEstimateBlockWords);
    for (size_t i = startIndex; i < startPartialEnd; ++i) {
      bonusMs += reader_.wordPacingBonusMsAt(i);
    }

    bonusMs += wordBonusBlockPrefixSumMs_[lastFullBlockEnd] -
               wordBonusBlockPrefixSumMs_[firstFullBlock];

    const size_t endPartialStart = lastFullBlockEnd * kTimeEstimateBlockWords;
    for (size_t i = endPartialStart; i < endIndex; ++i) {
      bonusMs += reader_.wordPacingBonusMsAt(i);
    }
    return bonusMs;
  }

  for (size_t i = startIndex; i < endIndex; ++i) {
    bonusMs += reader_.wordPacingBonusMsAt(i);
  }
  return bonusMs;
}

void App::invalidateTimeEstimateCache() {
  cancelTimeEstimateBuild();
  timeEstimateCacheValid_ = false;
  std::vector<uint32_t>().swap(wordBonusBlockPrefixSumMs_);
}

void App::rebuildTimeEstimateCache() {
  invalidateTimeEstimateCache();
  pacingCacheDirty_ = false;
  if (!accurateTimeEstimateEnabled_) {
    if (!currentBookTitle_.isEmpty()) {
      renderStorageStatus("Reading time", currentBookTitle_.c_str(), "Fast estimate enabled",
                          100);
    }
    return;
  }

  const size_t n = reader_.wordCount();
  if (n == 0) {
    return;
  }

  const String label = currentBookTitle_.isEmpty() ? String("Current book") : currentBookTitle_;
  timeEstimateBuildWordCount_ = n;
  timeEstimateBuildBlockCount_ =
      (timeEstimateBuildWordCount_ + kTimeEstimateBlockWords - 1) / kTimeEstimateBlockWords;
  if (timeEstimateBuildBlockCount_ == 0) {
    return;
  }

  wordBonusBlockPrefixSumMs_.assign(timeEstimateBuildBlockCount_ + 1, 0);
  timeEstimateBuildBookPath_ = currentBookPath_;
  timeEstimateBuildNextBlock_ = 0;
  timeEstimateBuildRunningMs_ = 0;
  timeEstimateBuildStartedMs_ = millis();
  timeEstimateBuildLastLogMs_ = timeEstimateBuildStartedMs_;
  timeEstimateBuildInProgress_ = true;

  const String detail = String(static_cast<unsigned int>(n)) + " words in background";
  renderStorageStatus("Reading time", label.c_str(), detail.c_str(), 0);
  Serial.printf("[time-est] background build started words=%u blocks=%u book=%s\n",
                static_cast<unsigned int>(timeEstimateBuildWordCount_),
                static_cast<unsigned int>(timeEstimateBuildBlockCount_),
                currentBookPath_.c_str());
}

void App::cancelTimeEstimateBuild() {
  timeEstimateBuildInProgress_ = false;
  timeEstimateBuildBookPath_ = "";
  timeEstimateBuildWordCount_ = 0;
  timeEstimateBuildBlockCount_ = 0;
  timeEstimateBuildNextBlock_ = 0;
  timeEstimateBuildRunningMs_ = 0;
  timeEstimateBuildStartedMs_ = 0;
  timeEstimateBuildLastLogMs_ = 0;
}

bool App::timeEstimateBuildMatchesCurrentBook() const {
  return timeEstimateBuildInProgress_ && timeEstimateBuildBookPath_ == currentBookPath_ &&
         timeEstimateBuildWordCount_ == reader_.wordCount();
}

void App::updateTimeEstimateBuild(uint32_t nowMs) {
  if (!timeEstimateBuildInProgress_) {
    return;
  }

  if (!accurateTimeEstimateEnabled_ || !timeEstimateBuildMatchesCurrentBook()) {
    Serial.println("[time-est] background build cancelled");
    invalidateTimeEstimateCache();
    return;
  }

  if (state_ == AppState::Playing || state_ == AppState::CompanionSync ||
      state_ == AppState::UsbTransfer || state_ == AppState::Standby ||
      state_ == AppState::Sleeping) {
    return;
  }

  size_t processedBlocks = 0;
  while (timeEstimateBuildNextBlock_ < timeEstimateBuildBlockCount_ &&
         processedBlocks < kTimeEstimateBlocksPerUpdate) {
    const size_t block = timeEstimateBuildNextBlock_;
    wordBonusBlockPrefixSumMs_[block] = timeEstimateBuildRunningMs_;
    const size_t blockStart = block * kTimeEstimateBlockWords;
    const size_t blockEnd =
        std::min(timeEstimateBuildWordCount_, blockStart + kTimeEstimateBlockWords);
    for (size_t i = blockStart; i < blockEnd; ++i) {
      timeEstimateBuildRunningMs_ += reader_.wordPacingBonusMsAt(i);
    }
    ++timeEstimateBuildNextBlock_;
    ++processedBlocks;
    delay(0);
  }

  if (timeEstimateBuildNextBlock_ >= timeEstimateBuildBlockCount_) {
    wordBonusBlockPrefixSumMs_[timeEstimateBuildBlockCount_] = timeEstimateBuildRunningMs_;
    timeEstimateCacheValid_ = true;
    const uint32_t elapsedMs = millis() - timeEstimateBuildStartedMs_;
    Serial.printf("[time-est] background cached %u words in %u blocks bonus=%lums took=%lums\n",
                  static_cast<unsigned int>(timeEstimateBuildWordCount_),
                  static_cast<unsigned int>(timeEstimateBuildBlockCount_),
                  static_cast<unsigned long>(timeEstimateBuildRunningMs_),
                  static_cast<unsigned long>(elapsedMs));
    cancelTimeEstimateBuild();
    if (state_ == AppState::Paused || state_ == AppState::Playing) {
      renderActiveReader(nowMs);
    } else if (state_ == AppState::Menu) {
      renderMenu();
    }
    return;
  }

  if (nowMs - timeEstimateBuildLastLogMs_ >= kTimeEstimateProgressLogMs) {
    const int progress =
        static_cast<int>((timeEstimateBuildNextBlock_ * 100UL) /
                         std::max<size_t>(1, timeEstimateBuildBlockCount_));
    Serial.printf("[time-est] background progress %u/%u blocks (%d%%)\n",
                  static_cast<unsigned int>(timeEstimateBuildNextBlock_),
                  static_cast<unsigned int>(timeEstimateBuildBlockCount_), progress);
    timeEstimateBuildLastLogMs_ = nowMs;
    if (state_ == AppState::Paused) {
      renderActiveReader(nowMs);
    }
  }
}

String App::timeEstimateModeLabel() const {
  return uiText(accurateTimeEstimateEnabled_ ? UiText::TimeEstimateAccurate
                                             : UiText::TimeEstimateFast);
}

String App::formatFocusTimerDuration(uint32_t durationMs) const {
  const uint32_t totalSeconds = durationMs / 1000UL;
  const uint32_t minutes = totalSeconds / 60UL;
  const uint32_t seconds = totalSeconds % 60UL;
  char buffer[8];
  std::snprintf(buffer, sizeof(buffer), "%02lu:%02lu", static_cast<unsigned long>(minutes),
                static_cast<unsigned long>(seconds));
  return String(buffer);
}

String App::formatReadingTimeRemaining(uint32_t remainingMs) const {
  const uint32_t totalSeconds = remainingMs / 1000UL;
  if (totalSeconds < 60UL) {
    return "0m";
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

void App::applyUiOrientation(Board::Config::UiOrientation orientation) {
  Input::Touch::setUiOrientation(orientation);
  display_.setUiOrientation(orientation);
}

void App::applyReaderUiOrientation() {
  applyUiOrientation(readerUiOrientation());
}

Board::Config::UiOrientation App::readerUiOrientation() const {
  return uiRotated180() ? Board::Config::UiOrientation::LandscapeFlipped
                        : Board::Config::UiOrientation::Landscape;
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
  if (Board::Audio::beep()) {
    return;
  }

  Board::Display::flashBacklight(3, 55, 45);
}

bool App::scrollModeEnabled() const { return readerMode_ == ReaderMode::Scroll; }

bool App::uiRotated180() const {
  return handednessMode_ == HandednessMode::Right ? Board::Config::UI_ROTATED_180
                                                  : !Board::Config::UI_ROTATED_180;
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
  text.reserve(totalChars);
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
  text.reserve(totalChars);
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
  if (pendingBootBookLoad_) {
    display_.renderStatus("Loading book", currentBookTitle_, "Please wait");
    return;
  }

  if (!ensureCurrentBookWordAvailable(nowMs)) {
    return;
  }

  if (chapterTransitionVisible_) {
    renderChapterTransition();
    return;
  }

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

bool App::ensureCurrentBookWordAvailable(uint32_t nowMs) {
  if (!usingStorageBook_ || reader_.wordCount() == 0 || !reader_.currentWord().isEmpty()) {
    return true;
  }

  handleCurrentBookReadFailure(nowMs, "Word cache unreadable");
  return false;
}

void App::handleCurrentBookReadFailure(uint32_t nowMs, const char *detail) {
  const String failedTitle = currentBookTitle_.isEmpty() ? String("Current book") : currentBookTitle_;
  const String failedPath = currentBookPath_;
  const bool articlesOnly =
      currentBookIndex_ < storage_.bookCount() && storage_.bookIsArticle(currentBookIndex_);

  Serial.printf("[app] active book read failed word=%u book=%s detail=%s\n",
                static_cast<unsigned int>(reader_.currentIndex()), failedPath.c_str(),
                detail == nullptr ? "" : detail);

  saveReadingPosition(true);
  activeBookStore_.close();
  reader_.clearLoadedBook(nowMs);
  chapterMarkers_.clear();
  paragraphStarts_.clear();
  currentBookPath_ = "";
  currentBookTitle_ = "Demo";
  usingStorageBook_ = false;
  contextViewVisible_ = false;
  wpmFeedbackVisible_ = false;
  invalidateContextPreviewWindow();
  invalidateTimeEstimateCache();

  setState(AppState::Menu, nowMs);
  display_.renderStatus("Book read failed", failedTitle,
                        detail == nullptr ? "Reopen from library" : detail);
  delay(1800);
  openBookPicker(articlesOnly);
}

void App::renderReaderWord() {
  applyReaderUiOrientation();
  contextViewVisible_ = false;
  const String beforeText = phantomWordsEnabled_ ? phantomBeforeText() : "";
  const String afterText = phantomWordsEnabled_ ? phantomAfterText() : "";
  const DisplayManager::ReaderChrome chrome = readerChrome();
  const bool showReaderFooter = readerFooterVisible();
  const String footerMetricLabel = readerFooterStatusLabel();
  display_.renderPhantomRsvpWord(beforeText, reader_.currentWord(), afterText,
                                 readerFontSizeIndex_, currentChapterLabel(),
                                 readingProgressPercent(), showReaderFooter, footerMetricLabel,
                                 chrome);
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
  const DisplayManager::ReaderChrome chrome = readerChrome();
  display_.renderScrollView(contextPreviewWords_, currentReaderContentToken(),
                            contextPreviewStartIndex_, currentIndex, 0,
                            currentChapterLabel(), readingProgressPercent(), "",
                            readerFooterStatusLabel(), chrome);
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

  const DisplayManager::ReaderChrome chrome = readerChrome();
  display_.renderScrollView(contextPreviewWords_, currentReaderContentToken(),
                            contextPreviewStartIndex_, currentIndex, scrollProgressPermille,
                            currentChapterLabel(), readingProgressPercent(), overlayText,
                            readerFooterStatusLabel(), chrome);
}

void App::renderWpmFeedback(uint32_t nowMs) {
  if (!ensureCurrentBookWordAvailable(nowMs)) {
    return;
  }

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
  const DisplayManager::ReaderChrome chrome = readerChrome();
  const String footerMetricLabel = readerFooterStatusLabel();
  display_.renderPhantomRsvpWordWithWpm(beforeText, reader_.currentWord(), afterText,
                                        readerFontSizeIndex_, reader_.wpm(),
                                        currentChapterLabel(), readingProgressPercent(),
                                        readerFooterVisible(), footerMetricLabel, chrome);
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
