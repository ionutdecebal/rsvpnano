#pragma once

// Canonical registry of device preference keys and the NVS namespace they live
// in. App (the device UI) and CompanionSyncManager (the web companion) both
// read and write the same stored settings; before this header each file kept
// its own copy of these key strings, which could silently drift apart. Keys are
// short by necessity -- the ESP32 Preferences/NVS key length limit is 15 chars.
namespace settings {

constexpr const char *kPrefsNamespace = "rsvp";

// Library / reading position.
constexpr const char *kPrefBookPath = "book";
constexpr const char *kPrefRecentSeq = "seq";

// Reader + display.
constexpr const char *kPrefWpm = "wpm";
constexpr const char *kPrefBrightness = "bright";
constexpr const char *kPrefDarkMode = "dark";
constexpr const char *kPrefNightMode = "night";
constexpr const char *kPrefYellowMode = "yellow_md";
constexpr const char *kPrefUiLanguage = "ui_lang";
constexpr const char *kPrefReaderMode = "read_mode";
constexpr const char *kPrefHandedness = "handed";
constexpr const char *kPrefReaderControlsSwapped = "ctrl_swap";
constexpr const char *kPrefPhantomWords = "phantom_on";
constexpr const char *kPrefFooterMetricMode = "prog_md";
constexpr const char *kPrefBatteryLabelMode = "bat_md";
constexpr const char *kPrefScreensaverMode = "scrn_sv";
constexpr const char *kPrefReaderBatteryVisible = "read_bat";
constexpr const char *kPrefReaderChapterVisible = "read_ch";
constexpr const char *kPrefReaderProgressVisible = "read_pct";
constexpr const char *kPrefChapterLabelRsvp = "ch_lbl_rsvp";
constexpr const char *kPrefChapterLabelScroll = "ch_lbl_scroll";
constexpr const char *kPrefReaderFontSize = "font_size";
constexpr const char *kPrefReaderTypeface = "typeface";

// Typography.
constexpr const char *kPrefTypographyFocusHighlight = "type_hlt";
constexpr const char *kPrefTypographyTracking = "type_trk";
constexpr const char *kPrefTypographyAnchor = "type_anc";
constexpr const char *kPrefTypographyGuideWidth = "type_wid";
constexpr const char *kPrefTypographyGuideGap = "type_gap";

// Word pacing.
constexpr const char *kPrefPacingLongMs = "pace_lms";
constexpr const char *kPrefPacingComplexMs = "pace_cms";
constexpr const char *kPrefPacingPunctuationMs = "pace_pms";
constexpr const char *kPrefPauseMode = "pause_md";
constexpr const char *kPrefAccurateTime = "time_est_a";
constexpr const char *kPrefMenuRepeatMs = "menu_rpt";
constexpr const char *kPrefStandbyTimer = "stby_tmr";
constexpr const char *kPrefCpuPlay = "cpu_play";
constexpr const char *kPrefCpuScroll = "cpu_scroll";
constexpr const char *kPrefCpuPaused = "cpu_paused";
constexpr const char *kPrefCpuMenu = "cpu_menu";
constexpr const char *kPrefCpuStandby = "cpu_stby";
constexpr const char *kPrefAutoDimLevel = "dim_lvl";
constexpr const char *kPrefAutoDimDelay = "dim_dly";
constexpr const char *kPrefTimerDurationByGenre[] = {
    "tmr_dur_0",
    "tmr_dur_1",
    "tmr_dur_2",
    "tmr_dur_3",
    "tmr_dur_4",
};

// Wi-Fi + OTA.
constexpr const char *kPrefWifiSsid = "wifi_ssid";
constexpr const char *kPrefWifiPass = "wifi_pass";
constexpr const char *kPrefOtaAuto = "ota_auto";
constexpr const char *kPrefOtaOwner = "ota_owner";
constexpr const char *kPrefOtaTag = "ota_tag";

}  // namespace settings
