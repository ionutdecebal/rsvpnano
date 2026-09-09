@file:OptIn(kotlinx.coroutines.ExperimentalCoroutinesApi::class)

package com.rsvpnano.app

import com.rsvpnano.presentation.CompanionPresenter
import com.rsvpnano.presentation.CatalogAsset
import com.rsvpnano.connection.*
import com.rsvpnano.persistence.JsonAppSettingsStore
import com.rsvpnano.updates.FirmwareUpdates
import com.rsvpnano.models.RememberedNano
import com.rsvpnano.models.NanoSettingsSchema
import com.rsvpnano.models.NanoLocales
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.emptyFlow
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlin.test.assertTrue
import kotlin.test.assertFalse
import kotlin.test.assertNull

import com.rsvpnano.api.NanoApi
import com.rsvpnano.api.NanoClientError
import com.rsvpnano.api.RepositoryClient
import com.rsvpnano.sampleBook
import com.rsvpnano.sampleSettings
import com.rsvpnano.library.PendingDraftService
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
import kotlinx.coroutines.test.runTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class NanoCompanionControllerTest {
    @Test
    fun connectRequestsOnlyDeviceIdentity() = runTest {
        val client = RecordingNanoClient()

        val device = controller(client).connect("http://device.local")

        assertEquals("RSVP-Nano-123456", device.ssid)
        assertEquals("preview-v0.0.9+abc", device.firmwareVersion)
        assertEquals(1, client.fetchDeviceCalls)
        assertEquals(0, client.listLibraryCalls)
    }

    @Test
    fun unavailableLibraryDoesNotPreventConnecting() = runTest {
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
    fun connectDoesNotHideFailureBehindRetries() = runTest {
        val client = RecordingNanoClient(deviceFailures = 1)

        assertFailsWith<NanoClientError> {
            controller(client).connect("http://device.local")
        }

        assertEquals(1, client.fetchDeviceCalls)
        assertEquals(0, client.listLibraryCalls)
    }

    @Test
    fun uploadReturnsCreatedBookAndDeleteReturnsNoDuplicateLibrary() = runTest {
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
    fun bookPositionReturnsOnlyTheUpdatedBook() = runTest {
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
    fun themeUploadReturnsOnlyTheCreatedTheme() = runTest {
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
    fun settingsWifiAndFeedsReturnTheSavedResourcesWithoutRefreshes() = runTest {
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
    fun appearanceSelectionsUpdateOnlyTheirRequestedResource() = runTest {
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

    @Test
    fun activeBookNeedsExplicitSecondConfirmationAndCancelNeverForcesDelete() = runTest {
        val book = sampleBook("active-book")
        val client = RecordingNanoClient(initialBooks = listOf(book)).apply { activeBookId = book.id }
        val presenter = presenter(client)
        presenter.deleteDeviceBook(book)
        assertTrue(client.deleteForces.isEmpty())
        presenter.confirmDeviceDeletion()
        assertEquals(listOf(false), client.deleteForces)
        assertTrue(presenter.uiState.value.deletion!!.inUse)
        presenter.dismissDeviceDeletion()
        presenter.confirmDeviceDeletion()
        assertEquals(listOf(false), client.deleteForces)
        assertEquals(listOf(book), client.books)
        presenter.deleteDeviceBook(book)
        presenter.confirmDeviceDeletion()
        presenter.confirmDeviceDeletion()
        assertEquals(listOf(false, false, true), client.deleteForces)
        assertTrue(client.books.isEmpty())
        assertNull(presenter.uiState.value.deletion)
    }

    @Test
    fun selectedCatalogAssetsSwitchToFallbackOnlyAfterConfirmation() = runTest {
        for (asset in CatalogAsset.entries) {
            val client = RecordingNanoClient().apply {
                deviceSettings = sampleSettings().let {
                    it.copy(`interface` = it.`interface`.copy(selectedThemeId = "night", locale = "ja"))
                }
            }
            val presenter = presenter(client)
            presenter.refreshSettings()
            presenter.refreshThemes()
            presenter.refreshFonts()
            presenter.refreshLocales()
            val request = when (asset) {
                CatalogAsset.Theme -> { { presenter.removeTheme("night") } }
                CatalogAsset.Font -> { { presenter.removeFont("serif") } }
                CatalogAsset.Locale -> { { presenter.removeLocalePack("ja-pack") } }
            }
            request()
            assertTrue(presenter.uiState.value.deletion!!.inUse)
            presenter.dismissDeviceDeletion()
            assertTrue(client.catalogMutations.isEmpty())
            request()
            presenter.confirmDeviceDeletion()
            val fallback = when (asset) {
                CatalogAsset.Theme -> NanoSettingsSchema.THEME_DEFAULT
                CatalogAsset.Font -> "builtin"
                CatalogAsset.Locale -> NanoLocales.DEFAULT
            }
            assertEquals(listOf("select:$fallback", "delete"), client.catalogMutations)
            assertNull(presenter.uiState.value.deletion)
        }
    }

    @Test
    fun staleSelectionIsWarnedAndDisconnectDiscardsConfirmation() = runTest {
        val client = RecordingNanoClient()
        val presenter = presenter(client)
        presenter.refreshSettings()
        presenter.removeTheme("night")
        assertFalse(presenter.uiState.value.deletion!!.inUse)
        client.deviceSettings = client.deviceSettings.let { it.copy(`interface` = it.`interface`.copy(selectedThemeId = "night")) }
        presenter.confirmDeviceDeletion()
        assertTrue(presenter.uiState.value.deletion!!.inUse)
        assertTrue(client.catalogMutations.isEmpty())
        presenter.reportConnectionFailure("Disconnected")
        presenter.confirmDeviceDeletion()
        assertNull(presenter.uiState.value.deletion)
        assertTrue(client.catalogMutations.isEmpty())
    }

    private fun TestScope.presenter(client: RecordingNanoClient): CompanionPresenter {
        val settings = JsonAppSettingsStore(InMemoryTextStorage())
        val network = object : NanoWifiConnector {
            override val snapshot = MutableStateFlow(NanoWifiSnapshot())
            override val events = emptyFlow<NanoWifiEvent>()
            override fun start() = Unit
            override fun stop() = Unit
            override fun refreshSnapshot() = Unit
            override suspend fun discoverNanos() = emptyList<NanoEndpoint>()
            override fun requestNanoNetwork(rememberedNano: RememberedNano?) = NanoWifiRequestResult.Started
        }
        return CompanionPresenter(controller(client), FirmwareUpdates(client, settings), network, settings,
            CoroutineScope(backgroundScope.coroutineContext + UnconfinedTestDispatcher(testScheduler))).also {
            it.connectEndpoint(NanoEndpoint("http://device.local", RememberedNano("RSVP-Nano-123456")))
            assertTrue(it.uiState.value.isConnected)
        }
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
        val deleteForces = mutableListOf<Boolean>()
        var activeBookId: String? = null
        var deviceSettings = sampleSettings()
        val catalogMutations = mutableListOf<String>()

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
        override suspend fun listFonts(baseUrl: String) = listOf(NanoFontSummary("builtin", "Built-in", builtIn = true), NanoFontSummary("serif", "Serif"))
        override suspend fun listLocales(baseUrl: String) = listOf(NanoLocaleSummary("ja-pack", "Japanese", "ja"))

        override suspend fun fetchSettings(baseUrl: String) = deviceSettings
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
        override suspend fun selectTheme(baseUrl: String, id: String) { selectedThemeId = id; catalogMutations += "select:$id" }
        override suspend fun selectFont(baseUrl: String, id: String) { selectedFontId = id; catalogMutations += "select:$id" }
        override suspend fun selectLocale(baseUrl: String, id: String) { selectedLocaleId = id; catalogMutations += "select:$id" }
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

        override suspend fun deleteBook(baseUrl: String, id: String, force: Boolean) {
            deleteForces += force
            if (id == activeBookId && !force) throw NanoClientError("Book is open", status = 409, code = "resource_in_use")
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

        override suspend fun deleteTheme(baseUrl: String, id: String) { catalogMutations += "delete" }
        override suspend fun uploadFont(baseUrl: String, name: String, data: ByteArray, onProgress: ((Long, Long) -> Unit)?) =
            NanoFontSummary("font", "Font")
        override suspend fun deleteFont(baseUrl: String, id: String) { catalogMutations += "delete" }
        override suspend fun uploadLocalePack(baseUrl: String, name: String, data: ByteArray, onProgress: ((Long, Long) -> Unit)?) =
            NanoLocaleSummary("ja", "日本語", "ja")
        override suspend fun deleteLocalePack(baseUrl: String, id: String) { catalogMutations += "delete" }

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
