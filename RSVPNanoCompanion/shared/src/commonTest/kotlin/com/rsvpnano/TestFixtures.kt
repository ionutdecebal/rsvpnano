package com.rsvpnano

import com.rsvpnano.models.NanoSettings

internal fun sampleSettings(): NanoSettings = NanoSettings(
    ok = true,
    version = 1,
    reading = NanoSettings.Reading(
        wpm = 250,
        readerMode = "single",
        pauseMode = "sentence",
        accurateTimeEstimate = true,
        pacing = NanoSettings.Pacing(longWordMs = 0, complexWordMs = 0, punctuationMs = 0),
    ),
    display = NanoSettings.Display(
        brightnessIndex = 1,
        darkMode = false,
        nightMode = false,
        handedness = "right",
        footerMetric = "battery",
        batteryLabel = "battery",
        language = 0,
        phantomWords = false,
        fontSizeIndex = 1,
    ),
    typography = NanoSettings.Typography(
        typeface = "serif",
        focusHighlight = true,
        tracking = 0,
        anchorPercent = 50,
        guideWidth = 1,
        guideGap = 1,
    ),
)
