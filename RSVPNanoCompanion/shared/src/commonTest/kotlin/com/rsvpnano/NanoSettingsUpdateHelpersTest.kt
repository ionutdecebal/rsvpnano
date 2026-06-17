package com.rsvpnano

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
            .withWpm(325)
            .withReaderMode("scroll")
            .withPauseMode("instant")
            .withAccurateTimeEstimate(false)
            .withPacingLongWordMs(120)
            .withPacingComplexWordMs(80)
            .withPacingPunctuationMs(200)

        assertNotSame(original, updated)
        assertEquals(250, original.reading.wpm)
        assertEquals(325, updated.reading.wpm)
        assertEquals("scroll", updated.reading.readerMode)
        assertEquals("instant", updated.reading.pauseMode)
        assertFalse(updated.reading.accurateTimeEstimate)
        assertEquals(100, updated.reading.pacing.longWordMs)
        assertEquals(100, updated.reading.pacing.complexWordMs)
        assertEquals(200, updated.reading.pacing.punctuationMs)
        assertEquals(original.display, updated.display)
        assertEquals(original.typography, updated.typography)
    }

    @Test
    fun numericHelpersNormalizeSharedSettingsValues() {
        val updated = sampleSettings()
            .withWpm(103)
            .withPacingLongWordMs(626)
            .withPacingComplexWordMs(-20)
            .withBrightnessIndex(99)
            .withFontSizeIndex(-1)
            .withTracking(10)
            .withAnchorPercent(12)
            .withGuideWidth(19)
            .withGuideGap(99)

        assertEquals(100, updated.reading.wpm)
        assertEquals(600, updated.reading.pacing.longWordMs)
        assertEquals(0, updated.reading.pacing.complexWordMs)
        assertEquals(4, updated.display.brightnessIndex)
        assertEquals(0, updated.display.fontSizeIndex)
        assertEquals(3, updated.typography.tracking)
        assertEquals(30, updated.typography.anchorPercent)
        assertEquals(20, updated.typography.guideWidth)
        assertEquals(8, updated.typography.guideGap)
    }

    @Test
    fun displayHelpersReturnUpdatedCopiesWithoutMutatingOriginal() {
        val original = sampleSettings()
        val updated = original
            .withBrightnessIndex(4)
            .withHandedness("left")
            .withReaderControls("rewind_top_right")
            .withFooterMetric("chapter_time")
            .withBatteryLabel("time_remaining")
            .withAppearance(darkMode = true, nightMode = true)
            .withPhantomWords(true)
            .withFontSizeIndex(2)

        assertNotSame(original, updated)
        assertEquals(1, original.display.brightnessIndex)
        assertEquals(4, updated.display.brightnessIndex)
        assertEquals("left", updated.display.handedness)
        assertEquals("rewind_top_right", updated.display.readerControls)
        assertEquals("chapter_time", updated.display.footerMetric)
        assertEquals("time_remaining", updated.display.batteryLabel)
        assertTrue(updated.display.darkMode)
        assertTrue(updated.display.nightMode)
        assertEquals("night", updated.appearanceMode)
        assertTrue(updated.display.phantomWords)
        assertEquals(2, updated.display.fontSizeIndex)
        assertEquals(original.reading, updated.reading)
        assertEquals(original.typography, updated.typography)
    }

    @Test
    fun appearanceModeHelperUpdatesDarkAndNightModeTogether() {
        val light = sampleSettings().withAppearanceMode("light")
        val dark = sampleSettings().withAppearanceMode("dark")
        val night = sampleSettings().withAppearanceMode("night")

        assertFalse(light.display.darkMode)
        assertFalse(light.display.nightMode)
        assertEquals("light", light.appearanceMode)
        assertTrue(dark.display.darkMode)
        assertFalse(dark.display.nightMode)
        assertEquals("dark", dark.appearanceMode)
        assertTrue(night.display.darkMode)
        assertTrue(night.display.nightMode)
        assertEquals("night", night.appearanceMode)
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
        assertEquals("serif", original.typography.typeface)
        assertEquals("atkinson", updated.typography.typeface)
        assertFalse(updated.typography.focusHighlight)
        assertEquals(2, updated.typography.tracking)
        assertEquals(36, updated.typography.anchorPercent)
        assertEquals(18, updated.typography.guideWidth)
        assertEquals(4, updated.typography.guideGap)
        assertEquals(original.reading, updated.reading)
        assertEquals(original.display, updated.display)
    }
}
