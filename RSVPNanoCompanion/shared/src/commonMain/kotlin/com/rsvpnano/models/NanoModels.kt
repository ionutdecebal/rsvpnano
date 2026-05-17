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
        val footerMetric: String,
        val batteryLabel: String,
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
        copy(reading = reading.copy(wpm = value))

    fun withReaderMode(value: String): NanoSettings =
        copy(reading = reading.copy(readerMode = value))

    fun withPauseMode(value: String): NanoSettings =
        copy(reading = reading.copy(pauseMode = value))

    fun withPacingLongWordMs(value: Int): NanoSettings =
        copy(reading = reading.copy(pacing = reading.pacing.copy(longWordMs = value)))

    fun withPacingComplexWordMs(value: Int): NanoSettings =
        copy(reading = reading.copy(pacing = reading.pacing.copy(complexWordMs = value)))

    fun withPacingPunctuationMs(value: Int): NanoSettings =
        copy(reading = reading.copy(pacing = reading.pacing.copy(punctuationMs = value)))

    fun withBrightnessIndex(value: Int): NanoSettings =
        copy(display = display.copy(brightnessIndex = value))

    fun withHandedness(value: String): NanoSettings =
        copy(display = display.copy(handedness = value))

    fun withFooterMetric(value: String): NanoSettings =
        copy(display = display.copy(footerMetric = value))

    fun withBatteryLabel(value: String): NanoSettings =
        copy(display = display.copy(batteryLabel = value))

    fun withAppearance(darkMode: Boolean, nightMode: Boolean): NanoSettings =
        copy(display = display.copy(darkMode = darkMode, nightMode = nightMode))

    fun withPhantomWords(value: Boolean): NanoSettings =
        copy(display = display.copy(phantomWords = value))

    fun withFontSizeIndex(value: Int): NanoSettings =
        copy(display = display.copy(fontSizeIndex = value))

    fun withTypeface(value: String): NanoSettings =
        copy(typography = typography.copy(typeface = value))

    fun withFocusHighlight(value: Boolean): NanoSettings =
        copy(typography = typography.copy(focusHighlight = value))

    fun withTracking(value: Int): NanoSettings =
        copy(typography = typography.copy(tracking = value))

    fun withAnchorPercent(value: Int): NanoSettings =
        copy(typography = typography.copy(anchorPercent = value))

    fun withGuideWidth(value: Int): NanoSettings =
        copy(typography = typography.copy(guideWidth = value))

    fun withGuideGap(value: Int): NanoSettings =
        copy(typography = typography.copy(guideGap = value))
}
