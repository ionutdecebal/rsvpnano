package com.rsvpnano

import com.rsvpnano.api.NanoClient
import com.rsvpnano.converters.RsvpBookFile
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoUploadResponse
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.persistence.PendingUploadArticleService
import com.rsvpnano.persistence.PendingUploadRepository
import com.rsvpnano.persistence.PendingUploadStore
import com.rsvpnano.sync.PendingUploadSyncService
import kotlinx.coroutines.runBlocking
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class PendingUploadSyncServiceTest {
    @Test
    fun syncOneUploadsAndRemovesDraft() {
        val storage = InMemoryPendingStore(
            listOf(
                PendingUpload(
                    id = "1",
                    title = "Example",
                    sourceUrl = "https://example.com/story",
                    body = "Hello reader.",
                    createdAt = "2026-05-17T10:00:00Z",
                )
            )
        )
        val repository = PendingUploadRepository(storage, PendingUploadArticleService())
        val service = PendingUploadSyncService(repository, PendingUploadArticleService())
        val client = RecordingNanoClient()

        runBlocking {
            val file = service.syncOne(client, "http://device.local", storage.items.first())
            assertEquals("Example.rsvp", file.filename)
            assertEquals(emptyList(), storage.items)
            assertEquals("Example.rsvp", client.uploadedFilename)
        }
    }

    @Test
    fun syncOneKeepsDraftWhenUploadFails() {
        val draft = pendingUpload(id = "1", title = "Failed")
        val storage = InMemoryPendingStore(listOf(draft))
        val repository = PendingUploadRepository(storage, PendingUploadArticleService())
        val service = PendingUploadSyncService(repository, PendingUploadArticleService())
        val client = RecordingNanoClient(failingFilenames = setOf("Failed.rsvp"))

        runBlocking {
            assertFailsWith<IllegalStateException> {
                service.syncOne(client, "http://device.local", draft)
            }

            assertEquals(listOf(draft), storage.items)
            assertEquals(listOf("Failed.rsvp"), client.uploadedFilenames)
        }
    }

    @Test
    fun syncAllRemovesSuccessfulDraftsAndKeepsFailedAndUnattemptedDrafts() {
        val first = pendingUpload(id = "1", title = "First")
        val failing = pendingUpload(id = "2", title = "Failing")
        val unattempted = pendingUpload(id = "3", title = "Unattempted")
        val storage = InMemoryPendingStore(listOf(first, failing, unattempted))
        val repository = PendingUploadRepository(storage, PendingUploadArticleService())
        val service = PendingUploadSyncService(repository, PendingUploadArticleService())
        val client = RecordingNanoClient(failingFilenames = setOf("Failing.rsvp"))

        runBlocking {
            assertFailsWith<IllegalStateException> {
                service.syncAll(
                    client = client,
                    baseUrl = "http://device.local",
                    items = listOf(first, failing, unattempted),
                )
            }

            assertEquals(listOf(failing, unattempted), storage.items)
            assertEquals(listOf("First.rsvp", "Failing.rsvp"), client.uploadedFilenames)
        }
    }

    private class RecordingNanoClient : NanoClient {
        constructor()

        constructor(failingFilenames: Set<String>) {
            this.failingFilenames = failingFilenames
        }

        var uploadedFilename: String? = null
        var failingFilenames: Set<String> = emptySet()
        val uploadedFilenames: MutableList<String> = mutableListOf()

        override suspend fun fetchInfo(baseUrl: String): NanoInfo = NanoInfo(name = "Nano")

        override suspend fun listBooks(baseUrl: String): List<NanoBook> = emptyList()

        override suspend fun fetchSettings(baseUrl: String): NanoSettings = sampleSettings()

        override suspend fun updateSettings(baseUrl: String, settings: NanoSettings): NanoSettings = settings

        override suspend fun fetchWifiSettings(baseUrl: String): NanoWifiSettings =
            NanoWifiSettings(ok = true, configured = false, ssid = "", passwordSet = false)

        override suspend fun updateWifi(baseUrl: String, ssid: String, password: String): NanoWifiSettings =
            NanoWifiSettings(ok = true, configured = true, ssid = ssid, passwordSet = true)

        override suspend fun forgetWifi(baseUrl: String): NanoWifiSettings =
            NanoWifiSettings(ok = true, configured = false, ssid = "", passwordSet = false)

        override suspend fun fetchRssFeeds(baseUrl: String): NanoRssFeeds = NanoRssFeeds(ok = true, feeds = emptyList())

        override suspend fun updateRssFeeds(baseUrl: String, feeds: List<String>): NanoRssFeeds =
            NanoRssFeeds(ok = true, feeds = feeds)

        override suspend fun uploadBook(baseUrl: String, name: String, data: ByteArray, category: String?): NanoUploadResponse {
            uploadedFilename = name
            uploadedFilenames += name
            if (name in failingFilenames) {
                throw IllegalStateException("Upload failed for $name")
            }
            return NanoUploadResponse(ok = true, path = "/books/$name")
        }

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

    private class InMemoryPendingStore(var items: List<PendingUpload>) : PendingUploadStore {
        override suspend fun loadAll(): List<PendingUpload> = items

        override suspend fun saveAll(items: List<PendingUpload>) {
            this.items = items
        }

        override suspend fun add(item: PendingUpload) {
            items = listOf(item) + items
        }

        override suspend fun remove(id: String) {
            items = items.filterNot { it.id == id }
        }
    }

    private fun pendingUpload(id: String, title: String): PendingUpload = PendingUpload(
        id = id,
        title = title,
        sourceUrl = "https://example.com/$id",
        body = "Body for $title",
        createdAt = "2026-05-17T10:00:00Z",
    )
}
