package com.rsvpnano.models

import kotlinx.serialization.Serializable

@Serializable
data class NanoBook(
    val id: String,
    val title: String? = null,
    val author: String? = null,
    val bytes: Int = 0,
    val progressPercent: Int? = null,
    val category: String? = null
) {
    val displayTitle: String
        get() = title?.takeIf { it.isNotBlank() } ?: id.substringAfterLast('/').ifBlank { "Untitled" }
}

@Serializable
data class PendingUpload(
    val id: String,
    val title: String,
    val sourceUrl: String? = null,
    val body: String,
    val createdAt: String // ISO-8601 timestamp string; keep simple for portability
)

@Serializable
data class NanoInfo(
    val name: String,
    val mode: String? = null,
    val baseUrl: String? = null,
    val networkSsid: String? = null,
    val pairingCode: String? = null,
    val uploadPath: String? = null
)

@Serializable
data class NanoUploadResponse(
    val ok: Boolean,
    val path: String? = null,
    val error: String? = null,
)

@Serializable
data class NanoRssFeeds(
    val ok: Boolean,
    val feeds: List<String>,
)

@Serializable
data class NanoWifiSettings(
    val ok: Boolean,
    val configured: Boolean,
    val ssid: String,
    val passwordSet: Boolean,
)

@Serializable
data class NanoWifiUpdate(
    val ssid: String,
    val password: String,
)

@Serializable
data class NanoSettings(
    val ok: Boolean,
    val version: Int,
    val reading: Reading,
    val display: Display,
    val typography: Typography,
    val limits: Limits? = null,
) {
    @Serializable
    data class Reading(
        val wpm: Int,
        val readerMode: String,
        val pauseMode: String,
        val accurateTimeEstimate: Boolean,
        val pacing: Pacing,
    )

    @Serializable
    data class Pacing(
        val longWordMs: Int,
        val complexWordMs: Int,
        val punctuationMs: Int,
    )

    @Serializable
    data class Display(
        val brightnessIndex: Int,
        val darkMode: Boolean,
        val nightMode: Boolean,
        val handedness: String,
        val readerControls: String = NanoSettingsSchema.READER_CONTROLS_STANDARD,
        val footerMetric: String,
        val batteryLabel: String,
        val readingBattery: Boolean = true,
        val readingChapter: Boolean = false,
        val readingProgress: Boolean = false,
        val screensaver: Int = NanoSettingsSchema.SCREENSAVER_LIFE,
        val standbyTimerIndex: Int = NanoSettingsSchema.STANDBY_TIMER_NEVER,
        val language: Int,
        val phantomWords: Boolean,
        val fontSizeIndex: Int,
    )

    @Serializable
    data class Typography(
        val typeface: String,
        val focusHighlight: Boolean,
        val tracking: Int,
        val anchorPercent: Int,
        val guideWidth: Int,
        val guideGap: Int,
    )

    @Serializable
    data class Limits(
        val wpm: RangeLimit? = null,
        val brightnessIndex: RangeLimit? = null,
        val pacingMs: RangeLimit? = null,
        val tracking: RangeLimit? = null,
        val anchorPercent: RangeLimit? = null,
        val guideWidth: RangeLimit? = null,
        val guideGap: RangeLimit? = null,
    )

    @Serializable
    data class RangeLimit(
        val min: Int,
        val max: Int,
    )

    fun withAccurateTimeEstimate(value: Boolean): NanoSettings =
        copy(reading = reading.copy(accurateTimeEstimate = value))

    fun withWpm(value: Int): NanoSettings =
        copy(reading = reading.copy(wpm = NanoSettingsSchema.snapWpm(value)))

    fun withReaderMode(value: String): NanoSettings =
        copy(reading = reading.copy(readerMode = value))

    fun withPauseMode(value: String): NanoSettings =
        copy(reading = reading.copy(pauseMode = value))

    fun withPacingLongWordMs(value: Int): NanoSettings =
        copy(
            reading = reading.copy(
                pacing = reading.pacing.copy(longWordMs = NanoSettingsSchema.snapPacingMs(value)),
            ),
        )

    fun withPacingComplexWordMs(value: Int): NanoSettings =
        copy(
            reading = reading.copy(
                pacing = reading.pacing.copy(complexWordMs = NanoSettingsSchema.snapPacingMs(value)),
            ),
        )

    fun withPacingPunctuationMs(value: Int): NanoSettings =
        copy(
            reading = reading.copy(
                pacing = reading.pacing.copy(punctuationMs = NanoSettingsSchema.snapPacingMs(value)),
            ),
        )

    fun withBrightnessIndex(value: Int): NanoSettings =
        copy(display = display.copy(brightnessIndex = NanoSettingsSchema.coerceBrightnessIndex(value)))

    fun withHandedness(value: String): NanoSettings =
        copy(display = display.copy(handedness = value))

    fun withReaderControls(value: String): NanoSettings =
        copy(display = display.copy(readerControls = value))

    fun withFooterMetric(value: String): NanoSettings =
        copy(display = display.copy(footerMetric = value))

    fun withBatteryLabel(value: String): NanoSettings =
        copy(display = display.copy(batteryLabel = value))

    fun withReadingBattery(value: Boolean): NanoSettings =
        copy(display = display.copy(readingBattery = value))

    fun withReadingChapter(value: Boolean): NanoSettings =
        copy(display = display.copy(readingChapter = value))

    fun withReadingProgress(value: Boolean): NanoSettings =
        copy(display = display.copy(readingProgress = value))

    fun withScreensaver(value: Int): NanoSettings =
        copy(display = display.copy(screensaver = NanoSettingsSchema.coerceScreensaver(value)))

    fun withStandbyTimerIndex(value: Int): NanoSettings =
        copy(display = display.copy(standbyTimerIndex = NanoSettingsSchema.coerceStandbyTimerIndex(value)))

    fun withLanguage(value: Int): NanoSettings =
        copy(display = display.copy(language = NanoSettingsSchema.coerceLanguage(value)))

    fun withAppearance(darkMode: Boolean, nightMode: Boolean): NanoSettings =
        copy(display = display.copy(darkMode = darkMode, nightMode = nightMode))

    fun withPhantomWords(value: Boolean): NanoSettings =
        copy(display = display.copy(phantomWords = value))

    fun withFontSizeIndex(value: Int): NanoSettings =
        copy(display = display.copy(fontSizeIndex = NanoSettingsSchema.coerceFontSizeIndex(value)))

    fun withTypeface(value: String): NanoSettings =
        copy(typography = typography.copy(typeface = value))

    fun withFocusHighlight(value: Boolean): NanoSettings =
        copy(typography = typography.copy(focusHighlight = value))

    fun withTracking(value: Int): NanoSettings =
        copy(typography = typography.copy(tracking = NanoSettingsSchema.coerceTracking(value)))

    fun withAnchorPercent(value: Int): NanoSettings =
        copy(typography = typography.copy(anchorPercent = NanoSettingsSchema.coerceAnchorPercent(value)))

    fun withGuideWidth(value: Int): NanoSettings =
        copy(typography = typography.copy(guideWidth = NanoSettingsSchema.snapGuideWidth(value)))

    fun withGuideGap(value: Int): NanoSettings =
        copy(typography = typography.copy(guideGap = NanoSettingsSchema.coerceGuideGap(value)))

    val appearanceMode: String
        get() = when {
            display.nightMode -> NanoSettingsSchema.APPEARANCE_NIGHT
            display.darkMode -> NanoSettingsSchema.APPEARANCE_DARK
            else -> NanoSettingsSchema.APPEARANCE_LIGHT
        }

    fun withAppearanceMode(value: String): NanoSettings =
        withAppearance(
            darkMode = value == NanoSettingsSchema.APPEARANCE_DARK ||
                value == NanoSettingsSchema.APPEARANCE_NIGHT,
            nightMode = value == NanoSettingsSchema.APPEARANCE_NIGHT,
        )
}

object NanoSettingsSchema {
    const val READER_MODE_RSVP = "rsvp"
    const val READER_MODE_SCROLL = "scroll"
    const val PAUSE_MODE_SENTENCE_END = "sentence_end"
    const val PAUSE_MODE_INSTANT = "instant"
    const val APPEARANCE_LIGHT = "light"
    const val APPEARANCE_DARK = "dark"
    const val APPEARANCE_NIGHT = "night"
    const val HANDEDNESS_LEFT = "left"
    const val HANDEDNESS_RIGHT = "right"
    const val READER_CONTROLS_STANDARD = "standard"
    const val READER_CONTROLS_REWIND_TOP_RIGHT = "rewind_top_right"
    const val FOOTER_PERCENTAGE = "percentage"
    const val FOOTER_CHAPTER_TIME = "chapter_time"
    const val FOOTER_BOOK_TIME = "book_time"
    const val BATTERY_PERCENT = "percent"
    const val BATTERY_TIME_REMAINING = "time_remaining"
    const val BATTERY_VOLTAGE = "voltage"
    const val SCREENSAVER_LIFE = 0
    const val SCREENSAVER_MAZE = 2
    const val SCREENSAVER_VORONOI = 3
    const val SCREENSAVER_SCREEN_OFF = 6
    const val TYPEFACE_STANDARD = "standard"
    const val TYPEFACE_ATKINSON = "atkinson"
    const val TYPEFACE_OPEN_DYSLEXIC = "open_dyslexic"

    const val WPM_MIN = 10
    const val WPM_MAX = 1000
    const val WPM_LOW_STEP = 10
    const val WPM_HIGH_STEP = 25
    const val WPM_STEP_CUTOFF = 100
    const val PACING_MS_MIN = 0
    const val PACING_MS_MAX = 600
    const val PACING_MS_STEP = 50
    const val BRIGHTNESS_MIN = 0
    const val BRIGHTNESS_MAX = 4
    const val STANDBY_TIMER_NEVER = 0
    const val STANDBY_TIMER_1_MIN = 1
    const val STANDBY_TIMER_5_MIN = 2
    const val STANDBY_TIMER_10_MIN = 3
    const val STANDBY_TIMER_30_MIN = 4
    const val LANGUAGE_MIN = 0
    const val LANGUAGE_MAX = 5
    const val FONT_SIZE_MIN = 0
    const val FONT_SIZE_MAX = 2
    const val TRACKING_MIN = -2
    const val TRACKING_MAX = 3
    const val ANCHOR_PERCENT_MIN = 30
    const val ANCHOR_PERCENT_MAX = 40
    const val GUIDE_WIDTH_MIN = 12
    const val GUIDE_WIDTH_MAX = 30
    const val GUIDE_WIDTH_STEP = 2
    const val GUIDE_GAP_MIN = 2
    const val GUIDE_GAP_MAX = 8

    fun snapToStep(value: Int, step: Int): Int =
        ((value + step / 2) / step) * step

    fun snapWpm(value: Int): Int {
        val clamped = value.coerceIn(WPM_MIN, WPM_MAX)
        return if (clamped <= WPM_STEP_CUTOFF) {
            snapToStep(clamped, WPM_LOW_STEP).coerceIn(WPM_MIN, WPM_STEP_CUTOFF)
        } else {
            (WPM_STEP_CUTOFF + snapToStep(clamped - WPM_STEP_CUTOFF, WPM_HIGH_STEP))
                .coerceIn(WPM_STEP_CUTOFF, WPM_MAX)
        }
    }

    fun snapPacingMs(value: Int): Int =
        snapToStep(value, PACING_MS_STEP).coerceIn(PACING_MS_MIN, PACING_MS_MAX)

    fun coerceBrightnessIndex(value: Int): Int =
        value.coerceIn(BRIGHTNESS_MIN, BRIGHTNESS_MAX)

    fun coerceScreensaver(value: Int): Int =
        when (value) {
            SCREENSAVER_MAZE,
            SCREENSAVER_VORONOI,
            SCREENSAVER_SCREEN_OFF
            -> value
            else -> SCREENSAVER_LIFE
        }

    fun coerceStandbyTimerIndex(value: Int): Int =
        value.coerceIn(STANDBY_TIMER_NEVER, STANDBY_TIMER_30_MIN)

    fun coerceLanguage(value: Int): Int =
        value.coerceIn(LANGUAGE_MIN, LANGUAGE_MAX)

    fun coerceFontSizeIndex(value: Int): Int =
        value.coerceIn(FONT_SIZE_MIN, FONT_SIZE_MAX)

    fun coerceTracking(value: Int): Int =
        value.coerceIn(TRACKING_MIN, TRACKING_MAX)

    fun coerceAnchorPercent(value: Int): Int =
        value.coerceIn(ANCHOR_PERCENT_MIN, ANCHOR_PERCENT_MAX)

    fun snapGuideWidth(value: Int): Int =
        snapToStep(value, GUIDE_WIDTH_STEP).coerceIn(GUIDE_WIDTH_MIN, GUIDE_WIDTH_MAX)

    fun coerceGuideGap(value: Int): Int =
        value.coerceIn(GUIDE_GAP_MIN, GUIDE_GAP_MAX)
}

@Serializable
data class RememberedNano(
    val ssid: String,
    val bssid: String? = null,
)

@Serializable
data class CompanionAppSettings(
    val defaultAddress: String = "http://192.168.4.1",
    val rememberedNano: RememberedNano? = null,
) {
    fun withDefaultAddress(value: String): CompanionAppSettings =
        copy(defaultAddress = value)

    fun withRememberedNano(value: RememberedNano?): CompanionAppSettings =
        copy(rememberedNano = value)
}
