package com.rsvpnano

import com.rsvpnano.models.NanoLocales
import com.rsvpnano.models.NanoFontSummary
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoSettingsSchema
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class NanoSettingsWireFormatTest {
    private val json = Json {
        encodeDefaults = true
        ignoreUnknownKeys = true
    }

    @Test
    fun serializesTheFirmwareDeviceSettingsShapeWithoutLegacyFields() {
        val document = json.parseToJsonElement(json.encodeToString(sampleSettings())).jsonObject

        assertEquals(setOf("reading", "interface", "updates"), document.keys)
        assertFalse("display" in document)
        assertFalse("themes" in document)
        assertFalse("fonts" in document)

        val reading = document.getValue("reading").jsonObject
        assertEquals(
            setOf(
                "wpm",
                "mode",
                "pauseMode",
                "phantomWords",
                "chapterScrollReversed",
                "footerMetric",
                "batteryLabel",
                "batteryIconVisibility",
                "batteryLabelVisibility",
                "chapterVisibility",
                "progressVisibility",
                "arrowsVisibility",
                "leftHanded",
                "typography",
                "pacing",
            ),
            reading.keys,
        )
        assertEquals("page", reading.getValue("mode").jsonPrimitive.content)
        assertEquals(
            setOf("fontId", "fontSizeIndex", "focusHighlight", "tracking", "anchor", "guideWidth", "guideGap"),
            reading.getValue("typography").jsonObject.keys,
        )
        assertEquals(
            setOf("longWordDelayMs", "complexWordDelayMs", "punctuationDelayMs"),
            reading.getValue("pacing").jsonObject.keys,
        )
        assertEquals(
            setOf("checkOnStartup", "repositoryOwner", "releaseTag"),
            document.getValue("updates").jsonObject.keys,
        )
        assertEquals(
            setOf("brightnessPercent", "locale", "standbyTimerIndex", "screensaver", "selectedThemeId"),
            document.getValue("interface").jsonObject.keys,
        )
        assertFalse("automatic" in document.getValue("updates").jsonObject)
    }

    @Test
    fun decodesStableEnumNamesFromFirmware() {
        val settings = json.decodeFromString<NanoSettings>(
            """{"obsolete":true,"interface":{"locale":"ru","screensaver":"screenOff"},"reading":{"mode":"page","pauseMode":"sentenceEnd","footerMetric":"bookTime","batteryLabel":"timeRemaining"},"updates":{"checkOnStartup":true}}""",
        )

        assertEquals("ru", settings.`interface`.locale)
        assertEquals("screenOff", settings.`interface`.screensaver)
        assertEquals("sentenceEnd", settings.reading.pauseMode)
        assertEquals("page", settings.reading.mode)
        assertEquals("bookTime", settings.reading.footerMetric)
        assertEquals("timeRemaining", settings.reading.batteryLabel)
        assertEquals("always", settings.reading.batteryIconVisibility)
        assertTrue(settings.updates.checkOnStartup)
    }

    @Test
    fun defaultsMatchFirmwareSettingsModel() {
        val settings = NanoSettings()

        assertEquals(300, settings.reading.wpm)
        assertEquals(NanoSettingsSchema.READING_MODE_RSVP, settings.reading.mode)
        assertEquals(NanoSettingsSchema.STANDBY_TIMER_1_MIN, settings.`interface`.standbyTimerIndex)
        assertEquals(NanoSettingsSchema.TYPEFACE_DEFAULT, settings.reading.typography.fontId)
        assertFalse(settings.updates.checkOnStartup)
    }

    @Test
    fun acceptsExternalLocaleTags() {
        assertEquals("es", NanoSettingsSchema.coerceLocale("es"))
        assertEquals("zh-Hans", NanoSettingsSchema.coerceLocale("zh-Hans"))
        assertEquals("ja", NanoSettingsSchema.coerceLocale("ja"))
        assertEquals(NanoLocales.DEFAULT, NanoSettingsSchema.coerceLocale(""))
    }

    @Test
    fun localeAffinityKeepsMixedScriptFontsSelectable() {
        val font = NanoFontSummary("hebrew", "Noto Serif Hebrew", listOf("he"), scripts = listOf("Hebr"))

        assertTrue(font.usableFor("he", listOf("Latn", "Hebr")))
        assertTrue(font.usableFor("he-IL", listOf("Latn", "Hebr")))
        assertFalse(font.usableFor("en", listOf("Latn", "Hebr")))

        val math = NanoFontSummary("math", "STIX Two Math", scripts = listOf("Zmth"))
        assertFalse(math.usableFor("en", listOf("Zmth", "Latn")))
        assertTrue(math.usableFor("en", listOf("Zmth")))
        assertFalse(math.usableFor("en", listOf("Latn")))
    }
}
