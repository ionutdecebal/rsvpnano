package com.rsvpnano.models

import kotlinx.serialization.Serializable

@Serializable
data class NanoBook(
    val name: String = "",
    val id: String? = null,
    val title: String? = null,
    val author: String? = null,
    val bytes: Int = 0,
    val progressPercent: Int? = null,
    val category: String? = null
) {
    val apiName: String
        get() = name.ifBlank { id.orEmpty() }

    val displayTitle: String
        get() = title?.takeIf { it.isNotBlank() } ?: apiName.substringAfterLast('/').ifBlank { "Untitled" }
}

@Serializable
data class NanoBooksResponse(
    val books: List<NanoBook>,
)

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
    var ok: Boolean,
    var version: Int,
    var reading: Reading,
    var display: Display,
    var typography: Typography,
    var limits: Limits? = null,
) {
    @Serializable
    data class Reading(
        var wpm: Int,
        var readerMode: String,
        var pauseMode: String,
        var accurateTimeEstimate: Boolean,
        var pacing: Pacing,
    )

    @Serializable
    data class Pacing(
        var longWordMs: Int,
        var complexWordMs: Int,
        var punctuationMs: Int,
    )

    @Serializable
    data class Display(
        var brightnessIndex: Int,
        var darkMode: Boolean,
        var nightMode: Boolean,
        var handedness: String,
        var footerMetric: String,
        var batteryLabel: String,
        var language: Int,
        var phantomWords: Boolean,
        var fontSizeIndex: Int,
    )

    @Serializable
    data class Typography(
        var typeface: String,
        var focusHighlight: Boolean,
        var tracking: Int,
        var anchorPercent: Int,
        var guideWidth: Int,
        var guideGap: Int,
    )

    @Serializable
    data class Limits(
        var wpm: RangeLimit? = null,
        var brightnessIndex: RangeLimit? = null,
        var pacingMs: RangeLimit? = null,
        var tracking: RangeLimit? = null,
        var anchorPercent: RangeLimit? = null,
        var guideWidth: RangeLimit? = null,
        var guideGap: RangeLimit? = null,
    )

    @Serializable
    data class RangeLimit(
        var min: Int,
        var max: Int,
    )
}
