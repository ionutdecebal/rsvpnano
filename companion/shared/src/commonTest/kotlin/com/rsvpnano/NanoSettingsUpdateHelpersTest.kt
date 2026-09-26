package com.rsvpnano

import com.rsvpnano.models.NanoSettings
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotSame
import kotlin.test.assertTrue

class NanoSettingsUpdateHelpersTest {
    @Test
    fun readingHelpersReturnUpdatedCopiesWithoutMutatingOriginal() {
        val original = sampleSettings()
        val updated = original
            .withWpm(320)
            .withPauseMode("instant")
            .withPacingLongWordMs(120)
            .withPacingComplexWordMs(80)
            .withPacingPunctuationMs(200)

        assertNotSame(original, updated)
        assertEquals(250, original.reading.wpm)
        assertEquals(320, updated.reading.wpm)
        assertEquals("instant", updated.reading.pauseMode)
        assertEquals(100, updated.reading.pacing.longWordDelayMs)
        assertEquals(100, updated.reading.pacing.complexWordDelayMs)
        assertEquals(200, updated.reading.pacing.punctuationDelayMs)
        assertEquals(original.`interface`, updated.`interface`)
    }

    @Test
    fun numericHelpersNormalizeSharedSettingsValues() {
        val updated = sampleSettings()
            .withWpm(103)
            .withPacingLongWordMs(626)
            .withPacingComplexWordMs(-20)
            .withBrightnessPercent(101)
            .withFontSizeIndex(9)
            .withTracking(10)
            .withAnchorPercent(12)
            .withGuideWidth(19)
            .withGuideGap(99)

        assertEquals(100, updated.reading.wpm)
        assertEquals(600, updated.reading.pacing.longWordDelayMs)
        assertEquals(0, updated.reading.pacing.complexWordDelayMs)
        assertEquals(100, updated.`interface`.brightnessPercent)
        assertEquals(4, updated.reading.typography.fontSizeIndex)
        assertEquals(3, updated.reading.typography.tracking)
        assertEquals(30, updated.reading.typography.anchor)
        assertEquals(20, updated.reading.typography.guideWidth)
        assertEquals(8, updated.reading.typography.guideGap)
    }

    @Test
    fun pacingResetRestoresReaderDefaults() {
        val reset = sampleSettings()
            .withPacingLongWordMs(0)
            .withPacingComplexWordMs(600)
            .withPacingPunctuationMs(50)
            .withDefaultPacing()

        assertEquals(NanoSettings.Pacing(), reset.reading.pacing)
    }

    @Test
    fun displayHelpersReturnUpdatedCopiesWithoutMutatingOriginal() {
        val original = sampleSettings()
        val updated = original
            .withBrightnessPercent(25)
            .withHandedness("left")
            .withFooterMetric("chapterTime")
            .withBatteryLabel("timeRemaining")
            .withThemeId("night")
            .withPhantomWords(true)
            .withFontSizeIndex(3)

        assertNotSame(original, updated)
        assertEquals(10, original.`interface`.brightnessPercent)
        assertEquals(25, updated.`interface`.brightnessPercent)
        assertTrue(updated.reading.leftHanded)
        assertEquals("chapterTime", updated.reading.footerMetric)
        assertEquals("timeRemaining", updated.reading.batteryLabel)
        assertEquals("night", updated.`interface`.selectedThemeId)
        assertTrue(updated.reading.phantomWords)
        assertEquals(3, updated.reading.typography.fontSizeIndex)
    }

    @Test
    fun themeIdHelperFallsBackToDefaultWhenBlank() {
        val custom = sampleSettings().withThemeId("catppuccin-mocha")
        val blank = sampleSettings().withThemeId("")

        assertEquals("catppuccin-mocha", custom.`interface`.selectedThemeId)
        assertEquals("default", blank.`interface`.selectedThemeId)
    }

    @Test
    fun typographyHelpersReturnUpdatedCopiesWithoutMutatingOriginal() {
        val original = sampleSettings()
        val updated = original
            .withTypeface("atkinson")
            .withFocusHighlight(false)
            .withTracking(2)
            .withAnchorPercent(36)
            .withGuideWidth(18)
            .withGuideGap(4)

        assertNotSame(original, updated)
        assertEquals("serif", original.reading.typography.fontId)
        assertEquals("atkinson", updated.reading.typography.fontId)
        assertFalse(updated.reading.typography.focusHighlight)
        assertEquals(2, updated.reading.typography.tracking)
        assertEquals(36, updated.reading.typography.anchor)
        assertEquals(18, updated.reading.typography.guideWidth)
        assertEquals(4, updated.reading.typography.guideGap)
        assertEquals(original.`interface`, updated.`interface`)
    }
}
