package com.rsvpnano

import com.rsvpnano.api.NanoApi
import com.rsvpnano.api.NanoClientError
import com.rsvpnano.api.RepositoryClient
import com.rsvpnano.app.NanoCompanionController
import com.rsvpnano.app.NanoSettingsResource
import com.rsvpnano.app.PendingDraftService
import com.rsvpnano.converters.RsvpBookFile
import com.rsvpnano.models.FirmwareRelease
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoFocusTimers
import com.rsvpnano.models.NanoFontCatalogItem
import com.rsvpnano.models.NanoFontSummary
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoLanguageFont
import com.rsvpnano.models.NanoLocaleCatalogItem
import com.rsvpnano.models.NanoLocaleSummary
import com.rsvpnano.models.NanoReadingProgress
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoStorageRepair
import com.rsvpnano.models.NanoThemeCatalogItem
import com.rsvpnano.models.NanoThemeSummary
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.persistence.PendingUploadJsonStore
import com.rsvpnano.persistence.TextStorage
import kotlinx.coroutines.runBlocking
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class NanoCompanionControllerTest {
    @Test
    fun connectRequestsOnlyDeviceIdentity() = runBlocking {
        val client = RecordingNanoClient()

        val device = controller(client).connect("http://device.local")

        assertEquals("RSVP-Nano-123456", device.ssid)
        assertEquals("preview-v0.0.9+abc", device.firmwareVersion)
        assertEquals(1, client.fetchDeviceCalls)
        assertEquals(0, client.listLibraryCalls)
    }

    @Test
    fun unavailableLibraryDoesNotPreventConnecting() = runBlocking {
        val client = RecordingNanoClient(failLibrary = true)
        val controller = controller(client)

        assertEquals("preview-v0.0.9+abc", controller.connect("http://device.local").firmwareVersion)
        assertFailsWith<NanoClientError> {
            controller.refreshLibrary("http://device.local")
        }

        assertEquals(1, client.fetchDeviceCalls)
        assertEquals(1, client.listLibraryCalls)
    }

    @Test
    fun connectDoesNotHideFailureBehindRetries() = runBlocking {
        val client = RecordingNanoClient(deviceFailures = 1)

        assertFailsWith<NanoClientError> {
            controller(client).connect("http://device.local")
        }

        assertEquals(1, client.fetchDeviceCalls)
        assertEquals(0, client.listLibraryCalls)
    }

    @Test
    fun uploadReturnsCreatedBookAndDeleteReturnsNoDuplicateLibrary() = runBlocking {
        val client = RecordingNanoClient()
        val controller = controller(client)

        val uploaded = controller.uploadBook(
            "http://device.local",
            RsvpBookFile("Manual.rsvp", byteArrayOf(1, 2, 3), "Manual", 1, 1),
            "book",
        )
        controller.deleteBooks("http://device.local", listOf(uploaded.id))

        assertEquals("Manual.rsvp", client.uploadedFilename)
        assertEquals("book", client.uploadedCategory)
        assertEquals(listOf("Manual.rsvp"), client.deletedIds)
        assertEquals(emptyList(), client.books)
        assertEquals(0, client.listLibraryCalls)
    }

    @Test
    fun bookPositionReturnsOnlyTheUpdatedBook() = runBlocking {
        val book = sampleBook(id = "b12345678", title = "Manual", wordCount = 1000).copy(
            reading = NanoReadingProgress(100),
        )
        val client = RecordingNanoClient(initialBooks = listOf(book))

        val updated = controller(client).setBookPosition(
            "http://device.local",
            book,
            250,
        )

        assertEquals("b12345678", client.savedPositionId)
        assertEquals(250, client.savedPositionWordIndex)
        assertEquals(250, updated.reading?.wordIndex)
    }

    @Test
    fun themeUploadReturnsOnlyTheCreatedTheme() = runBlocking {
        val client = RecordingNanoClient()

        val response = controller(client).uploadTheme(
            "http://device.local",
            "night.toml",
            "theme-data".encodeToByteArray(),
        )

        assertEquals("night.toml", client.uploadedThemeFilename)
        assertEquals(NanoThemeSummary("night", "Night"), response)
    }

    @Test
    fun settingsWifiAndFeedsReturnTheSavedResourcesWithoutRefreshes() = runBlocking {
        val client = RecordingNanoClient()
        val controller = controller(client)
        val settings = sampleSettings().withWpm(320).withBrightnessPercent(20)

        assertEquals(
            settings,
            controller.saveSettings(
                "http://device.local",
                settings,
                setOf(NanoSettingsResource.Reading, NanoSettingsResource.Display),
            ),
        )
        assertEquals(NanoWifiSettings("Home"), controller.saveWifiSettings("http://device.local", "Home", "secret"))
        controller.clearWifiSettings("http://device.local")
        assertEquals(
            listOf("https://local.example/feed"),
            controller.saveRssFeeds(
                "http://device.local",
                listOf(" https://local.example/feed ", "https://local.example/feed"),
            ),
        )
        assertEquals(settings, client.savedSettings)
        assertEquals(
            listOf(NanoSettingsResource.Reading, NanoSettingsResource.Display),
            client.savedSettingsResources,
        )
        assertEquals("Home" to "secret", client.savedWifi)
        assertEquals(listOf("https://local.example/feed"), client.savedFeeds)
        assertEquals(0, client.fetchDeviceCalls)
    }

    @Test
    fun appearanceSelectionsUpdateOnlyTheirRequestedResource() = runBlocking {
        val client = RecordingNanoClient()
        val controller = controller(client)

        assertEquals("night", controller.selectTheme("http://device.local", "night"))
        assertEquals("andika", controller.selectFont("http://device.local", "andika"))
        assertEquals("he", controller.selectLocale("http://device.local", "he"))
        assertEquals("night", client.selectedThemeId)
        assertEquals("andika", client.selectedFontId)
        assertEquals("he", client.selectedLocaleId)
        assertEquals(null, client.savedSettings)
    }

    private fun controller(client: RecordingNanoClient) = NanoCompanionController(
        draftService = PendingDraftService(PendingUploadJsonStore(InMemoryTextStorage())),
        nanoApi = client,
        repository = client,
    )

    private class RecordingNanoClient(
        private val deviceFeeds: List<String> = emptyList(),
        initialBooks: List<NanoBook> = emptyList(),
        private val failLibrary: Boolean = false,
        private var deviceFailures: Int = 0,
    ) : NanoApi, RepositoryClient {
        override fun close() = Unit

        var books: List<NanoBook> = initialBooks
        var fetchDeviceCalls = 0
        var listLibraryCalls = 0
        var uploadedFilename: String? = null
        var uploadedCategory: String? = null
        var uploadedThemeFilename: String? = null
        var savedFeeds: List<String>? = null
        var savedSettings: NanoSettings? = null
        val savedSettingsResources = mutableListOf<NanoSettingsResource>()
        var savedWifi: Pair<String, String>? = null
        var savedPositionId: String? = null
        var savedPositionWordIndex: Int? = null
        var selectedThemeId: String? = null
        var selectedFontId: String? = null
        var selectedLocaleId: String? = null
        val deletedIds = mutableListOf<String>()

        override suspend fun fetchDevice(baseUrl: String): NanoInfo {
            fetchDeviceCalls++
            if (deviceFailures-- > 0) throw NanoClientError("device not ready")
            return NanoInfo("RSVP-Nano-123456", "preview-v0.0.9+abc", "reader-ota.bin")
        }

        override suspend fun repairStorage(baseUrl: String) = NanoStorageRepair(
            healthy = true,
            checked = 0,
            moved = 0,
            removed = 0,
            diagnosticSummary = "Storage OK",
            diagnosticDetail = "",
        )

        override suspend fun listLibrary(baseUrl: String): List<NanoBook> {
            listLibraryCalls++
            if (failLibrary) throw NanoClientError("library unavailable")
            return books
        }

        override suspend fun listThemes(baseUrl: String) = listOf(NanoThemeSummary("night", "Night"))
        override suspend fun listFonts(baseUrl: String) = emptyList<NanoFontSummary>()
        override suspend fun listLocales(baseUrl: String) = emptyList<NanoLocaleSummary>()

        override suspend fun fetchSettings(baseUrl: String) = sampleSettings()
        override suspend fun updateReadingSettings(baseUrl: String, settings: NanoSettings.Reading) {
            savedSettingsResources += NanoSettingsResource.Reading
            savedSettings = (savedSettings ?: sampleSettings()).copy(reading = settings)
        }
        override suspend fun updateDisplaySettings(baseUrl: String, settings: NanoSettings.Interface) {
            savedSettingsResources += NanoSettingsResource.Display
            savedSettings = (savedSettings ?: sampleSettings()).copy(`interface` = settings)
        }
        override suspend fun updateUpdateSettings(baseUrl: String, settings: NanoSettings.Updates) {
            savedSettingsResources += NanoSettingsResource.Updates
            savedSettings = (savedSettings ?: sampleSettings()).copy(updates = settings)
        }
        override suspend fun selectTheme(baseUrl: String, id: String) { selectedThemeId = id }
        override suspend fun selectFont(baseUrl: String, id: String) { selectedFontId = id }
        override suspend fun selectLocale(baseUrl: String, id: String) { selectedLocaleId = id }
        override suspend fun fetchWifiSettings(baseUrl: String) = NanoWifiSettings("")
        override suspend fun updateWifi(baseUrl: String, ssid: String, password: String) {
            savedWifi = ssid to password
        }
        override suspend fun forgetWifi(baseUrl: String) = Unit
        override suspend fun fetchRssFeeds(baseUrl: String) = NanoRssFeeds(deviceFeeds)
        override suspend fun updateRssFeeds(baseUrl: String, config: NanoRssFeeds) { savedFeeds = config.feeds }
        override suspend fun fetchFocusTimers(baseUrl: String) = NanoFocusTimers(emptyList())
        override suspend fun updateFocusTimers(baseUrl: String, timers: NanoFocusTimers) = Unit

        override suspend fun uploadBook(
            baseUrl: String,
            name: String,
            data: ByteArray,
            category: String?,
            onProgress: ((Long, Long) -> Unit)?,
        ): NanoBook {
            onProgress?.invoke(data.size.toLong(), data.size.toLong())
            uploadedFilename = name
            uploadedCategory = category
            books = listOf(sampleBook(id = name, title = name.substringBeforeLast('.')))
            return books.single()
        }

        override suspend fun deleteBook(baseUrl: String, id: String) {
            deletedIds += id
            books = books.filterNot { it.id == id }
        }

        override suspend fun setBookPosition(baseUrl: String, id: String, wordIndex: Int) {
            savedPositionId = id
            savedPositionWordIndex = wordIndex
            books = books.map { book ->
                if (book.id != id) book else book.copy(reading = book.reading?.copy(wordIndex = wordIndex))
            }
        }

        override suspend fun setBookLanguageFonts(
            baseUrl: String,
            id: String,
            languageFonts: List<NanoLanguageFont>,
        ) = Unit

        override suspend fun uploadTheme(
            baseUrl: String,
            name: String,
            data: ByteArray,
            onProgress: ((Long, Long) -> Unit)?,
        ): NanoThemeSummary {
            uploadedThemeFilename = name
            return NanoThemeSummary("night", "Night")
        }

        override suspend fun deleteTheme(baseUrl: String, id: String) = Unit
        override suspend fun uploadFont(baseUrl: String, name: String, data: ByteArray, onProgress: ((Long, Long) -> Unit)?) =
            NanoFontSummary("font", "Font")
        override suspend fun deleteFont(baseUrl: String, id: String) = Unit
        override suspend fun uploadLocalePack(baseUrl: String, name: String, data: ByteArray, onProgress: ((Long, Long) -> Unit)?) =
            NanoLocaleSummary("ja", "日本語", "ja")
        override suspend fun deleteLocalePack(baseUrl: String, id: String) = Unit

        override suspend fun fetchThemeCatalog(url: String): List<NanoThemeCatalogItem> = emptyList()
        override suspend fun fetchFirmwareRelease(owner: String, repository: String, tag: String) =
            FirmwareRelease("", emptyList())
        override suspend fun downloadTheme(url: String, onProgress: ((Long, Long?) -> Unit)?) = byteArrayOf()
        override suspend fun fetchFontCatalog(url: String): List<NanoFontCatalogItem> = emptyList()
        override suspend fun downloadFont(url: String, onProgress: ((Long, Long?) -> Unit)?) = byteArrayOf()
        override suspend fun fetchLocaleCatalog(url: String): List<NanoLocaleCatalogItem> = emptyList()
        override suspend fun downloadLocalePack(url: String, onProgress: ((Long, Long?) -> Unit)?) = byteArrayOf()
    }

    private class InMemoryTextStorage : TextStorage {
        private var value: String? = null
        override suspend fun readText(): String? = value
        override suspend fun writeText(value: String) { this.value = value }
    }
}
