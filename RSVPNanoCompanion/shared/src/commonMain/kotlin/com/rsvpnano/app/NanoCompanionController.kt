package com.rsvpnano.app

import com.rsvpnano.api.NanoApi
import com.rsvpnano.api.RepositoryClient
import com.rsvpnano.converters.RsvpBookFile
import com.rsvpnano.converters.SharedArticle
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoFontSummary
import com.rsvpnano.models.NanoFocusTimers
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoReadingProgress
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoStorageRepair
import com.rsvpnano.models.NanoThemeCatalogItem
import com.rsvpnano.models.NanoFontCatalogItem
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.NanoLocaleCatalogItem
import com.rsvpnano.models.NanoLanguageFont
import com.rsvpnano.models.NanoLocaleSummary
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.models.NanoThemeSummary
import com.rsvpnano.models.needsArticleFetch
import com.rsvpnano.sync.RssFeedNormalizer

enum class NanoSettingsResource {
    Reading,
    Display,
    Updates,
}

/**
 * Shared workflow controller for app-level device operations.
 *
 * Platform ViewModels own UI state; this class coordinates local drafts with device resources.
 */
class NanoCompanionController(
    private val draftService: PendingDraftService,
    private val nanoApi: NanoApi,
    private val repository: RepositoryClient,
) {
    fun close() {
        nanoApi.close()
        if (repository !== nanoApi) repository.close()
    }

    suspend fun refreshLocal(): List<PendingUpload> = draftService.loadDrafts()

    suspend fun connect(baseUrl: String): NanoInfo = nanoApi.fetchDevice(baseUrl)

    suspend fun repairStorage(baseUrl: String): NanoStorageRepair = nanoApi.repairStorage(baseUrl)

    suspend fun refreshLibrary(baseUrl: String): List<NanoBook> = nanoApi.listLibrary(baseUrl)

    suspend fun refreshSettings(baseUrl: String): NanoSettings = nanoApi.fetchSettings(baseUrl)

    suspend fun refreshThemes(baseUrl: String): List<NanoThemeSummary> = nanoApi.listThemes(baseUrl)

    suspend fun refreshFonts(baseUrl: String): List<NanoFontSummary> = nanoApi.listFonts(baseUrl)

    suspend fun refreshLocales(baseUrl: String): List<NanoLocaleSummary> = nanoApi.listLocales(baseUrl)

    suspend fun refreshWifiSettings(baseUrl: String): NanoWifiSettings = nanoApi.fetchWifiSettings(baseUrl)

    suspend fun refreshFocusTimers(baseUrl: String): NanoFocusTimers = nanoApi.fetchFocusTimers(baseUrl)

    suspend fun syncPendingUploads(baseUrl: String, items: List<PendingUpload>): CompanionPendingSyncSnapshot {
        val readyItems = items.filterNot(PendingUpload::needsArticleFetch)
        val remaining = draftService.syncPendingUploads(client = nanoApi, baseUrl = baseUrl, items = readyItems)
        return CompanionPendingSyncSnapshot(
            drafts = remaining,
            books = nanoApi.listLibrary(baseUrl),
            syncedCount = readyItems.size,
        )
    }

    suspend fun saveDraft(item: PendingUpload): List<PendingUpload> {
        draftService.saveDraft(item)
        return draftService.loadDrafts()
    }

    suspend fun saveDraftFetchingArticleIfNeeded(item: PendingUpload): CompanionDraftSaveSnapshot {
        val fetched = if (item.needsArticleFetch()) {
            draftService.fetchArticleIfAvailable(
                title = item.title,
                source = item.sourceUrl.orEmpty(),
            )
        } else {
            null
        }
        val savedItem = fetched?.let { article ->
            item.copy(
                title = article.title,
                body = article.text,
            )
        } ?: item
        draftService.saveDraft(savedItem)
        return CompanionDraftSaveSnapshot(
            drafts = draftService.loadDrafts(),
            item = savedItem,
            fetchedArticle = fetched != null,
        )
    }

    suspend fun fetchSharedArticle(title: String, source: String): SharedArticle? {
        return draftService.fetchArticleIfAvailable(title = title, source = source)
    }

    suspend fun deleteDraft(item: PendingUpload): List<PendingUpload> {
        draftService.deleteDraft(item)
        return draftService.loadDrafts()
    }

    suspend fun saveRssFeeds(
        baseUrl: String,
        feeds: List<String>,
    ): List<String> {
        val normalized = RssFeedNormalizer.normalize(feeds)
        nanoApi.updateRssFeeds(baseUrl, NanoRssFeeds(feeds = normalized))
        return normalized
    }

    suspend fun refreshRssFeeds(baseUrl: String): List<String> {
        val deviceFeeds = nanoApi.fetchRssFeeds(baseUrl).feeds
        val syncedFeeds = RssFeedNormalizer.normalize(deviceFeeds)
        return syncedFeeds
    }

    suspend fun uploadBook(
        baseUrl: String,
        file: RsvpBookFile,
        category: String,
        onProgress: ((sent: Long, total: Long) -> Unit)? = null,
    ): NanoBook {
        return nanoApi.uploadBook(
            baseUrl = baseUrl,
            name = file.filename,
            data = file.data,
            category = category,
            onProgress = onProgress,
        )
    }

    suspend fun fetchThemeCatalog(catalogUrl: String): List<NanoThemeCatalogItem> =
        repository.fetchThemeCatalog(catalogUrl)

    suspend fun fetchFontCatalog(catalogUrl: String): List<NanoFontCatalogItem> =
        repository.fetchFontCatalog(catalogUrl)

    suspend fun fetchLocaleCatalog(catalogUrl: String): List<NanoLocaleCatalogItem> =
        repository.fetchLocaleCatalog(catalogUrl)

    suspend fun downloadTheme(
        catalogUrl: String,
        theme: NanoThemeCatalogItem,
        onProgress: ((received: Long, total: Long?) -> Unit)? = null,
    ): CompanionCatalogFile {
        require(theme.file.isNotBlank() && '/' !in theme.file && '\\' !in theme.file) {
            "Theme catalog file path is invalid."
        }
        return CompanionCatalogFile(
            filename = theme.file,
            data = repository.downloadTheme(catalogFileUrl(catalogUrl, theme.file), onProgress),
        )
    }

    suspend fun downloadFont(
        catalogUrl: String,
        font: NanoFontCatalogItem,
        onProgress: ((received: Long, total: Long?) -> Unit)? = null,
    ): CompanionCatalogFile {
        require(isSafeFontCatalogPath(font.file)) {
            "Font catalog file path is invalid."
        }
        return CompanionCatalogFile(
            filename = font.file.substringAfterLast('/'),
            data = repository.downloadFont(catalogFileUrl(catalogUrl, font.file), onProgress),
        )
    }

    suspend fun uploadTheme(
        baseUrl: String,
        filename: String,
        data: ByteArray,
        onProgress: ((sent: Long, total: Long) -> Unit)? = null,
    ): NanoThemeSummary {
        return nanoApi.uploadTheme(
            baseUrl = baseUrl,
            name = filename,
            data = data,
            onProgress = onProgress,
        )
    }

    suspend fun uploadFont(
        baseUrl: String,
        filename: String,
        data: ByteArray,
        onProgress: ((sent: Long, total: Long) -> Unit)? = null,
    ): NanoFontSummary {
        return nanoApi.uploadFont(
            baseUrl = baseUrl,
            name = filename,
            data = data,
            onProgress = onProgress,
        )
    }

    suspend fun removeTheme(baseUrl: String, id: String) =
        nanoApi.deleteTheme(baseUrl, id)

    suspend fun removeFont(baseUrl: String, id: String) =
        nanoApi.deleteFont(baseUrl, id)

    suspend fun downloadLocalePack(
        catalogUrl: String,
        pack: NanoLocaleCatalogItem,
        onProgress: ((received: Long, total: Long?) -> Unit)? = null,
    ): CompanionCatalogFile {
        require(pack.file.isNotBlank() && '/' !in pack.file && '\\' !in pack.file &&
            pack.file.endsWith(".zip", ignoreCase = true)) {
            "Locale-pack catalog file path is invalid."
        }
        return CompanionCatalogFile(
            filename = pack.file,
            data = repository.downloadLocalePack(catalogFileUrl(catalogUrl, pack.file), onProgress),
        )
    }

    suspend fun installLocalePack(
        baseUrl: String,
        filename: String,
        data: ByteArray,
        onProgress: ((sent: Long, total: Long) -> Unit)? = null,
    ): NanoLocaleSummary = nanoApi.uploadLocalePack(baseUrl, filename, data, onProgress)

    suspend fun removeLocalePack(baseUrl: String, id: String) =
        nanoApi.deleteLocalePack(baseUrl, id)

    suspend fun deleteBooks(baseUrl: String, bookIds: List<String>) =
        bookIds.forEach { bookId -> nanoApi.deleteBook(baseUrl, bookId) }

    suspend fun setBookPosition(baseUrl: String, book: NanoBook, wordIndex: Int): NanoBook {
        val wordCount = book.metadata.wordCount
        require(wordCount > 0) {
            "Book position is unavailable."
        }
        val savedIndex = wordIndex.coerceIn(0, wordCount - 1)
        nanoApi.setBookPosition(
            baseUrl = baseUrl,
            id = book.id,
            wordIndex = savedIndex,
        )
        return book.copy(reading = (book.reading ?: NanoReadingProgress(savedIndex)).copy(wordIndex = savedIndex))
    }

    suspend fun setBookLanguageFonts(
        baseUrl: String,
        book: NanoBook,
        languageFonts: List<NanoLanguageFont>,
    ): NanoBook {
        require(book.metadata.wordCount > 0) { "Book language settings are unavailable." }
        nanoApi.setBookLanguageFonts(baseUrl, book.id, languageFonts)
        return book.copy(
            reading = (book.reading ?: NanoReadingProgress(0)).copy(languageFonts = languageFonts),
        )
    }

    suspend fun saveSettings(
        baseUrl: String,
        settings: NanoSettings,
        resources: Set<NanoSettingsResource>,
    ): NanoSettings {
        if (NanoSettingsResource.Reading in resources) {
            nanoApi.updateReadingSettings(baseUrl, settings.reading)
        }
        if (NanoSettingsResource.Display in resources) {
            nanoApi.updateDisplaySettings(baseUrl, settings.`interface`)
        }
        if (NanoSettingsResource.Updates in resources) {
            nanoApi.updateUpdateSettings(baseUrl, settings.updates)
        }
        return settings
    }

    suspend fun selectTheme(baseUrl: String, id: String): String {
        nanoApi.selectTheme(baseUrl, id)
        return id
    }

    suspend fun selectFont(baseUrl: String, id: String): String {
        nanoApi.selectFont(baseUrl, id)
        return id
    }

    suspend fun selectLocale(baseUrl: String, id: String): String {
        nanoApi.selectLocale(baseUrl, id)
        return id
    }

    suspend fun saveWifiSettings(baseUrl: String, ssid: String, password: String): NanoWifiSettings {
        nanoApi.updateWifi(baseUrl, ssid, password)
        return NanoWifiSettings(ssid)
    }

    suspend fun clearWifiSettings(baseUrl: String) = nanoApi.forgetWifi(baseUrl)

    suspend fun saveFocusTimers(baseUrl: String, timers: NanoFocusTimers): NanoFocusTimers {
        nanoApi.updateFocusTimers(baseUrl, timers)
        return timers
    }

    private fun catalogFileUrl(catalogUrl: String, file: String): String =
        catalogUrl.substringBeforeLast('/', missingDelimiterValue = catalogUrl) + "/" + file

    private fun isSafeFontCatalogPath(file: String): Boolean =
        file.isNotBlank() &&
            !file.startsWith('/') &&
            '\\' !in file &&
            ".." !in file.split('/') &&
            file.endsWith(".rfont4", ignoreCase = true)

}

data class CompanionPendingSyncSnapshot(
    val drafts: List<PendingUpload>,
    val books: List<NanoBook>,
    val syncedCount: Int,
)

data class CompanionDraftSaveSnapshot(
    val drafts: List<PendingUpload>,
    val item: PendingUpload,
    val fetchedArticle: Boolean,
)

data class CompanionCatalogFile(
    val filename: String,
    val data: ByteArray,
)
