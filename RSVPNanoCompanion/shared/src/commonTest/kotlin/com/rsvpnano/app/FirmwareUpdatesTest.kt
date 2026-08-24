package com.rsvpnano.app

import com.rsvpnano.models.FirmwareRelease
import com.rsvpnano.models.FirmwareUpdate
import com.rsvpnano.models.FirmwareUpdateTarget
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

class FirmwareUpdatesTest {
    @Test
    fun parsesTheSameCompactReleaseSourceAcceptedByTheNano() {
        assertEquals(
            FirmwareReleaseSource(DefaultFirmwareRepositoryOwner, "rsvpnano", ""),
            releaseSource("", ""),
        )
        assertEquals(
            FirmwareReleaseSource("reader-owner", "reader-firmware", "preview-v2"),
            releaseSource("ignored/default", "reader-owner/reader-firmware@preview-v2"),
        )
        assertEquals(
            FirmwareReleaseSource("reader-owner", "rsvpnano", ""),
            releaseSource("reader-owner", ""),
        )
        assertEquals(
            "https://raw.githubusercontent.com/reader-owner/reader-firmware/preview-v2/themes/index.json",
            requireNotNull(releaseSource("reader-owner/reader-firmware", "preview-v2"))
                .catalogContentUrl("themes/index.json"),
        )
    }

    @Test
    fun onlyOffersAnUnseenReleaseWithTheExactBoardAsset() {
        val target = FirmwareUpdateTarget("v1", "reader-board-ota.bin", "reader-owner", "")

        assertNull(pendingFirmwareUpdate(target, FirmwareRelease("v2", listOf("other-board-ota.bin")), null))
        assertNull(pendingFirmwareUpdate(target, FirmwareRelease("v1", listOf(target.otaAsset)), null))
        assertNull(pendingFirmwareUpdate(target, FirmwareRelease("v2", listOf(target.otaAsset)), "v2"))
        assertNull(
            pendingFirmwareUpdate(
                target.copy(currentVersion = "preview-old+0123456789ab"),
                FirmwareRelease("preview-new+0123456789ab", listOf(target.otaAsset)),
                null,
            ),
        )
        assertEquals(
            FirmwareUpdate("v1", "v2"),
            pendingFirmwareUpdate(target, FirmwareRelease("v2", listOf(target.otaAsset)), null),
        )
    }
}
