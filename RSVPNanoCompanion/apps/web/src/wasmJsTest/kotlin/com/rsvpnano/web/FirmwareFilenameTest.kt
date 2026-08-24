package com.rsvpnano.web

import kotlin.test.Test
import kotlin.test.assertEquals

class FirmwareFilenameTest {
    @Test
    fun recognizesReleaseAndLegacyBoardFilenames() {
        assertEquals(
            FirmwareFilenameMatch.Match,
            firmwareFilenameMatch("lcd349-v1", "rsvp-nano-esp32-s3-touch-lcd-3.49.bin"),
        )
        assertEquals(FirmwareFilenameMatch.Match, firmwareFilenameMatch("lcd349-v1", "rsvp-nano.bin"))
        assertEquals(FirmwareFilenameMatch.Match, firmwareFilenameMatch("lcd349-v2", "rsvp-nano-rev2.bin"))
        assertEquals(
            FirmwareFilenameMatch.Match,
            firmwareFilenameMatch("amoled18-v2", "rsvp-nano-esp32-s3-touch-amoled-1.8-v2.bin"),
        )
        assertEquals(
            FirmwareFilenameMatch.Match,
            firmwareFilenameMatch("lcd147-c6", "rsvp-nano-esp32-c6-touch-lcd-1.47.bin"),
        )
    }

    @Test
    fun warnsAboutOtherBoardsButAllowsUnknownCustomBuilds() {
        assertEquals(
            FirmwareFilenameMatch.DifferentBoard,
            firmwareFilenameMatch("amoled241", "rsvp-nano-esp32-s3-touch-amoled-2.16.bin"),
        )
        assertEquals(FirmwareFilenameMatch.Unknown, firmwareFilenameMatch("lcd349-v1", "my-custom-build.bin"))
    }

    @Test
    fun rejectsUpdateOnlyImages() {
        assertEquals(
            FirmwareFilenameMatch.Ota,
            firmwareFilenameMatch("lcd349-v1", "rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin"),
        )
    }
}
