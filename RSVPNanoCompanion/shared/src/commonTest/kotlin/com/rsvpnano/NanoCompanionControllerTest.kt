package com.rsvpnano

import com.rsvpnano.api.NanoClient
import com.rsvpnano.app.NanoCompanionController
import com.rsvpnano.app.NanoDeviceSyncService
import com.rsvpnano.app.PendingDraftService
import com.rsvpnano.app.RssFeedService
import com.rsvpnano.converters.RsvpBookFile
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoUploadResponse
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.persistence.PendingUploadStore
import com.rsvpnano.persistence.PendingUploadRepository
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
        assertEquals(listOf("https://device.example/feed"), snapshot.syncedRssFeeds)
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
    fun draftMutationsReturnLatestDrafts() = runBlocking {
        val existing = samplePendingUpload()
        val pendingStore = InMemoryPendingStore(listOf(existing))
        val controller = controller(pendingStore, InMemoryRssStore(), RecordingNanoClient())
        val added = existing.copy(id = "2", title = "Second")

        val saved = controller.saveDraft(added)
        val updated = controller.updateDraft(added, title = "Updated", body = "Updated body")
        val deleted = controller.deleteDraft(existing)

        assertEquals(listOf("Second", "Example"), saved.drafts.map { it.title })
        assertEquals(listOf("Updated", "Example"), updated.drafts.map { it.title })
        assertEquals(listOf("Updated"), deleted.drafts.map { it.title })
    }

    @Test
    fun deleteDraftsRemovesMatchingIds() = runBlocking {
        val pendingStore = InMemoryPendingStore(
            listOf(
                samplePendingUpload().copy(id = "1"),
                samplePendingUpload().copy(id = "2"),
                samplePendingUpload().copy(id = "3"),
            )
        )
        val controller = controller(pendingStore, InMemoryRssStore(), RecordingNanoClient())

        val snapshot = controller.deleteDrafts(listOf("1", "3"))

        assertEquals(listOf("2"), snapshot.drafts.map { it.id })
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

    @Test
    fun uploadBookUploadsFileAndRefreshesBooks() = runBlocking {
        val client = RecordingNanoClient()
        val controller = controller(InMemoryPendingStore(), InMemoryRssStore(), client)

        val snapshot = controller.uploadBook(
            baseUrl = "http://device.local",
            file = RsvpBookFile(
                filename = "Manual.rsvp",
                data = byteArrayOf(1, 2, 3),
                title = "Manual",
                wordCount = 1,
                chapterCount = 1,
            ),
            category = "book",
        )

        assertEquals("Manual.rsvp", client.uploadedFilename)
        assertEquals("book", client.uploadedCategory)
        assertEquals(listOf(NanoBook(id = "Manual.rsvp", title = "Manual")), snapshot.books)
    }

    @Test
    fun deleteBooksDeletesEachBookAndRefreshesBooks() = runBlocking {
        val client = RecordingNanoClient(
            initialBooks = listOf(
                NanoBook(id = "One.rsvp", title = "One"),
                NanoBook(id = "Two.rsvp", title = "Two"),
            )
        )
        val controller = controller(InMemoryPendingStore(), InMemoryRssStore(), client)

        val snapshot = controller.deleteBooks(
            baseUrl = "http://device.local",
            bookIds = listOf("One.rsvp", "Two.rsvp"),
        )

        assertEquals(listOf("One.rsvp", "Two.rsvp"), client.deletedFilenames)
        assertEquals(emptyList(), snapshot.books)
    }

    @Test
    fun saveSettingsPersistsDeviceSettings() = runBlocking {
        val client = RecordingNanoClient()
        val controller = controller(InMemoryPendingStore(), InMemoryRssStore(), client)
        val settings = sampleSettings().withWpm(320).withBrightnessIndex(3)

        val snapshot = controller.saveSettings(
            baseUrl = "http://device.local",
            settings = settings,
        )

        assertEquals(settings, client.savedSettings)
        assertEquals(settings, snapshot.settings)
        assertEquals(null, snapshot.wifiSettings)
    }

    @Test
    fun wifiMutationsReturnUpdatedWifiSnapshot() = runBlocking {
        val client = RecordingNanoClient()
        val controller = controller(InMemoryPendingStore(), InMemoryRssStore(), client)

        val saved = controller.saveWifiSettings(
            baseUrl = "http://device.local",
            ssid = "Home",
            password = "secret",
        )
        val cleared = controller.clearWifiSettings(baseUrl = "http://device.local")

        assertEquals("Home" to "secret", client.savedWifi)
        assertEquals(NanoWifiSettings(ok = true, configured = true, ssid = "Home", passwordSet = true), saved.wifiSettings)
        assertEquals(NanoWifiSettings(ok = true, configured = false, ssid = "", passwordSet = false), cleared.wifiSettings)
    }

    private fun controller(
        pendingStore: PendingUploadStore,
        rssStore: RssFeedStore,
        client: NanoClient,
    ): NanoCompanionController {
        return NanoCompanionController(
            draftService = PendingDraftService(
                repository = PendingUploadRepository(pendingStore),
            ),
            rssFeedService = RssFeedService(rssStore),
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
        initialBooks: List<NanoBook> = emptyList(),
    ) : NanoClient {
        private var books: List<NanoBook> = initialBooks
        var uploadedFilename: String? = null
        var uploadedCategory: String? = null
        var savedFeeds: List<String>? = null
        var savedSettings: NanoSettings? = null
        var savedWifi: Pair<String, String>? = null
        val deletedFilenames = mutableListOf<String>()

        override suspend fun fetchInfo(baseUrl: String): NanoInfo = NanoInfo(name = "Nano")

        override suspend fun listBooks(baseUrl: String): List<NanoBook> = books

        override suspend fun fetchSettings(baseUrl: String): NanoSettings = sampleSettings()

        override suspend fun updateSettings(baseUrl: String, settings: NanoSettings): NanoSettings {
            savedSettings = settings
            return settings
        }

        override suspend fun fetchWifiSettings(baseUrl: String): NanoWifiSettings =
            NanoWifiSettings(ok = true, configured = true, ssid = "RSVP", passwordSet = false)

        override suspend fun updateWifi(baseUrl: String, ssid: String, password: String): NanoWifiSettings {
            savedWifi = ssid to password
            return NanoWifiSettings(ok = true, configured = true, ssid = ssid, passwordSet = true)
        }

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
            uploadedCategory = category
            books = listOf(NanoBook(id = name, title = name.substringBeforeLast('.')))
            return NanoUploadResponse(ok = true, path = "/books/$name")
        }

        override suspend fun deleteBook(baseUrl: String, name: String): NanoUploadResponse {
            deletedFilenames += name
            books = books.filterNot { it.id == name }
            return NanoUploadResponse(ok = true)
        }
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
