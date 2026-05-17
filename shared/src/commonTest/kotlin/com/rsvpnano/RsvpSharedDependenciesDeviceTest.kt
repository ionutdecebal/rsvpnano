package com.rsvpnano

import com.rsvpnano.api.NanoClient
import com.rsvpnano.app.RsvpSharedDependencies
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoUploadResponse
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.persistence.PendingUploadStorage
import com.rsvpnano.persistence.RssFeedStorage
import kotlin.test.Test
import kotlin.test.assertNotNull

class RsvpSharedDependenciesDeviceTest {
    @Test
    fun createsDeviceSyncService() {
        val dependencies = RsvpSharedDependencies(
            pendingUploadStorage = object : PendingUploadStorage {
                override suspend fun readText(): String? = null
                override suspend fun writeText(value: String) = Unit
            },
            rssFeedStorage = object : RssFeedStorage {
                override suspend fun readText(): String? = null
                override suspend fun writeText(value: String) = Unit
            },
        )

        assertNotNull(dependencies.createDeviceSyncService(FakeClient()))
        assertNotNull(dependencies.createApp().createDeviceSyncService(FakeClient()))
    }

    private class FakeClient : NanoClient {
        override suspend fun fetchInfo(baseUrl: String): NanoInfo = NanoInfo(name = "Nano")
        override suspend fun listBooks(baseUrl: String): List<NanoBook> = emptyList()
        override suspend fun fetchSettings(baseUrl: String): NanoSettings = sampleSettings()
        override suspend fun updateSettings(baseUrl: String, settings: NanoSettings): NanoSettings = settings
        override suspend fun fetchWifiSettings(baseUrl: String): NanoWifiSettings = NanoWifiSettings(ok = true, configured = false, ssid = "", passwordSet = false)
        override suspend fun updateWifi(baseUrl: String, ssid: String, password: String): NanoWifiSettings = NanoWifiSettings(ok = true, configured = true, ssid = ssid, passwordSet = true)
        override suspend fun forgetWifi(baseUrl: String): NanoWifiSettings = NanoWifiSettings(ok = true, configured = false, ssid = "", passwordSet = false)
        override suspend fun fetchRssFeeds(baseUrl: String): NanoRssFeeds = NanoRssFeeds(ok = true, feeds = emptyList())
        override suspend fun updateRssFeeds(baseUrl: String, feeds: List<String>): NanoRssFeeds = NanoRssFeeds(ok = true, feeds = feeds)
        override suspend fun uploadBook(baseUrl: String, name: String, data: ByteArray, category: String?): NanoBook = NanoBook(id = name, title = name, category = category)
        override suspend fun deleteBook(baseUrl: String, name: String): NanoUploadResponse = NanoUploadResponse(ok = true)
    }

    private fun sampleSettings(): NanoSettings = NanoSettings(
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
}
