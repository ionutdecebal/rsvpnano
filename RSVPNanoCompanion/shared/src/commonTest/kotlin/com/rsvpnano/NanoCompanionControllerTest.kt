package com.rsvpnano

import com.rsvpnano.api.NanoClient
import com.rsvpnano.app.NanoCompanionController
import com.rsvpnano.app.NanoDeviceSyncService
import com.rsvpnano.app.RsvpSharedFacade
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoUploadResponse
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.persistence.PendingUploadStore
import com.rsvpnano.persistence.RssFeedStore
import kotlinx.coroutines.runBlocking
import kotlin.test.Test
import kotlin.test.assertEquals

class NanoCompanionControllerTest {
    @Test
    fun connectMergesDeviceFeedsAndLoadsDrafts() = runBlocking {
        val pendingStore = InMemoryPendingStore(listOf(samplePendingUpload()))
        val rssStore = InMemoryRssStore(listOf("https://local.example/feed"))
        val client = RecordingNanoClient(deviceFeeds = listOf("https://device.example/feed"))
        val controller = controller(pendingStore, rssStore, client)

        val snapshot = controller.connect(
            baseUrl = "http://device.local",
            localRssFeeds = listOf("https://local.example/feed"),
        )

        assertEquals("Nano", snapshot.device.info?.name)
        assertEquals(listOf("https://local.example/feed", "https://device.example/feed"), snapshot.rssFeeds)
        assertEquals(snapshot.rssFeeds, rssStore.items)
        assertEquals(1, snapshot.drafts.size)
    }

    @Test
    fun syncPendingUploadsUploadsDraftsAndRefreshesBooks() = runBlocking {
        val pending = samplePendingUpload()
        val pendingStore = InMemoryPendingStore(listOf(pending))
        val client = RecordingNanoClient()
        val controller = controller(pendingStore, InMemoryRssStore(), client)

        val snapshot = controller.syncPendingUploads(
            baseUrl = "http://device.local",
            items = listOf(pending),
        )

        assertEquals(1, snapshot.syncedCount)
        assertEquals(emptyList(), snapshot.drafts)
        assertEquals("Example.rsvp", client.uploadedFilename)
        assertEquals(listOf(NanoBook(id = "Example.rsvp", title = "Example")), snapshot.books)
    }

    @Test
    fun saveRssFeedsCanStayLocalOnly() = runBlocking {
        val rssStore = InMemoryRssStore()
        val client = RecordingNanoClient()
        val controller = controller(InMemoryPendingStore(), rssStore, client)

        val snapshot = controller.saveRssFeeds(
            baseUrl = "http://device.local",
            feeds = listOf(" https://example.com/feed ", "https://example.com/feed"),
            syncToDevice = false,
        )

        assertEquals(listOf("https://example.com/feed"), snapshot.rssFeeds)
        assertEquals(emptyList(), snapshot.syncedRssFeeds)
        assertEquals(false, snapshot.didSyncDevice)
        assertEquals(null, client.savedFeeds)
    }

    @Test
    fun saveRssFeedsSyncsDeviceAndPersistsMergedFeeds() = runBlocking {
        val rssStore = InMemoryRssStore()
        val client = RecordingNanoClient()
        val controller = controller(InMemoryPendingStore(), rssStore, client)

        val snapshot = controller.saveRssFeeds(
            baseUrl = "http://device.local",
            feeds = listOf("https://local.example/feed"),
            syncToDevice = true,
        )

        assertEquals(listOf("https://local.example/feed"), client.savedFeeds)
        assertEquals(listOf("https://local.example/feed"), snapshot.syncedRssFeeds)
        assertEquals(listOf("https://local.example/feed"), snapshot.rssFeeds)
        assertEquals(true, snapshot.didSyncDevice)
        assertEquals(snapshot.rssFeeds, rssStore.items)
    }

    private fun controller(
        pendingStore: PendingUploadStore,
        rssStore: RssFeedStore,
        client: NanoClient,
    ): NanoCompanionController {
        val facade = RsvpSharedFacade(
            pendingUploadStore = pendingStore,
            rssFeedStore = rssStore,
        )
        return NanoCompanionController(
            facade = facade,
            deviceSyncService = NanoDeviceSyncService(client),
            client = client,
        )
    }

    private fun samplePendingUpload(): PendingUpload = PendingUpload(
        id = "1",
        title = "Example",
        sourceUrl = "https://example.com/story",
        body = "Hello reader.",
        createdAt = "2026-05-17T10:00:00Z",
    )

    private class RecordingNanoClient(
        private val deviceFeeds: List<String> = emptyList(),
    ) : NanoClient {
        var uploadedFilename: String? = null
        var savedFeeds: List<String>? = null

        override suspend fun fetchInfo(baseUrl: String): NanoInfo = NanoInfo(name = "Nano")

        override suspend fun listBooks(baseUrl: String): List<NanoBook> =
            uploadedFilename?.let { listOf(NanoBook(id = it, title = "Example")) }.orEmpty()

        override suspend fun fetchSettings(baseUrl: String): NanoSettings = sampleSettings()

        override suspend fun updateSettings(baseUrl: String, settings: NanoSettings): NanoSettings = settings

        override suspend fun fetchWifiSettings(baseUrl: String): NanoWifiSettings =
            NanoWifiSettings(ok = true, configured = true, ssid = "RSVP", passwordSet = false)

        override suspend fun updateWifi(baseUrl: String, ssid: String, password: String): NanoWifiSettings =
            NanoWifiSettings(ok = true, configured = true, ssid = ssid, passwordSet = true)

        override suspend fun forgetWifi(baseUrl: String): NanoWifiSettings =
            NanoWifiSettings(ok = true, configured = false, ssid = "", passwordSet = false)

        override suspend fun fetchRssFeeds(baseUrl: String): NanoRssFeeds =
            NanoRssFeeds(ok = true, feeds = deviceFeeds)

        override suspend fun updateRssFeeds(baseUrl: String, feeds: List<String>): NanoRssFeeds {
            savedFeeds = feeds
            return NanoRssFeeds(ok = true, feeds = feeds)
        }

        override suspend fun uploadBook(
            baseUrl: String,
            name: String,
            data: ByteArray,
            category: String?,
        ): NanoUploadResponse {
            uploadedFilename = name
            return NanoUploadResponse(ok = true, path = "/books/$name")
        }

        override suspend fun deleteBook(baseUrl: String, name: String): NanoUploadResponse =
            NanoUploadResponse(ok = true)
    }

    private class InMemoryPendingStore(var items: List<PendingUpload> = emptyList()) : PendingUploadStore {
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

    private class InMemoryRssStore(var items: List<String> = emptyList()) : RssFeedStore {
        override suspend fun loadAll(): List<String> = items

        override suspend fun saveAll(items: List<String>) {
            this.items = items
        }
    }
}
