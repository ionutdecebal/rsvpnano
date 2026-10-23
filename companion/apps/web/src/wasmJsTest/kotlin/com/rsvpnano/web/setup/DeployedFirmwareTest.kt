package com.rsvpnano.web.setup

import kotlin.test.Test
import kotlin.test.assertEquals

class DeployedFirmwareTest {
    private val classic = "\"version\":\"v0.1.5\",\"firmware\":{\"lcd349-v1\":\"classic1.bin\",\"lcd349-v2\":\"classic2.bin\",\"amoled206\":\"amoled.bin\"}"
    private val rev1 = "{\"version\":\"v0.2.0\",\"firmware\":{\"lcd349-v1\":\"new1.bin\"}}"
    private val rev2 = "{\"version\":\"v0.2.0-rev2.1\",\"firmware\":{\"lcd349-v2\":\"new2.bin\"}}"

    @Test
    fun selectsEachBoardsImageAndVersionWithoutDuplicatingLegacyEntry() {
        val releases = parseDeployedReleases("{$classic,\"newUi\":$rev1,\"newUiVariants\":[$rev1,$rev2]}")
        val first = releases.forBoard("lcd349-v1")
        val second = releases.forBoard("lcd349-v2")
        assertEquals(listOf("classic", "new-ui"), first.map { it.id })
        assertEquals(listOf("classic", "new-ui"), second.map { it.id })
        assertEquals("v0.2.0", first.last().version)
        assertEquals("new1.bin", first.last().firmware["lcd349-v1"])
        assertEquals("v0.2.0-rev2.1", second.last().version)
        assertEquals("new2.bin", second.last().firmware["lcd349-v2"])
        assertEquals(listOf("classic"), releases.forBoard("amoled206").map { it.id })
        assertEquals(emptyList(), releases.forBoard("unknown"))
    }

    @Test
    fun acceptsOlderDeployments() {
        val releases = parseDeployedReleases("{$classic,\"newUi\":$rev1}")
        assertEquals(2, releases.forBoard("lcd349-v1").size)
        assertEquals(listOf("classic"), releases.forBoard("lcd349-v2").map { it.id })
        assertEquals(listOf("classic"), parseDeployedReleases("{$classic}").map { it.id })
    }
}
