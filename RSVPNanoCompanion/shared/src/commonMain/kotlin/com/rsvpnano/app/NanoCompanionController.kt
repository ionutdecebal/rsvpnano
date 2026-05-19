package com.rsvpnano.app

import com.rsvpnano.api.NanoClient
import com.rsvpnano.converters.RsvpBookFile
import com.rsvpnano.converters.SharedArticle
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.PendingUpload
import kotlinx.coroutines.delay

/**
 * Shared workflow controller for app-level device operations.
 *
 * Platform ViewModels should own UI state, but this class owns the repeated sequencing between
 * local companion data, device sync calls, and post-mutation refreshes.
 */
class NanoCompanionController(
    private val draftService: PendingDraftService,
    private val rssFeedService: RssFeedService,
    private val deviceSyncService: NanoDeviceSyncService,
    private val client: NanoClient,
) {
    suspend fun refreshLocal(): CompanionLocalSnapshot {
        return CompanionLocalSnapshot(
            drafts = draftService.loadDrafts(),
            rssFeeds = rssFeedService.loadRssFeeds(),
        )
    }

    suspend fun refreshDrafts(): CompanionDraftsSnapshot {
        return CompanionDraftsSnapshot(drafts = draftService.loadDrafts())
    }

    suspend fun connect(baseUrl: String, localRssFeeds: List<String>): CompanionConnectSnapshot {
        val device = deviceSyncService.connect(baseUrl)
        val deviceFeeds = device.rssFeeds?.feeds.orEmpty()
        val syncedFeeds = rssFeedService.mergeRssFeeds(localFeeds = emptyList(), deviceFeeds = deviceFeeds)
        val mergedFeeds = saveMergedRssFeeds(localRssFeeds, syncedFeeds)
        return CompanionConnectSnapshot(
            device = device,
            rssFeeds = mergedFeeds,
            syncedRssFeeds = syncedFeeds,
            drafts = draftService.loadDrafts(),
        )
    }

    suspend fun connectWithRetry(
        baseUrl: String,
        localRssFeeds: List<String>,
        attempts: Int = DEFAULT_CONNECTION_ATTEMPTS,
        retryDelayMillis: Long = DEFAULT_CONNECTION_RETRY_DELAY_MILLIS,
    ): CompanionConnectSnapshot {
        return retryDeviceOperation(attempts, retryDelayMillis) {
            connect(baseUrl, localRssFeeds)
        }
    }

    suspend fun refreshDevice(baseUrl: String, localRssFeeds: List<String>): CompanionDeviceRefreshSnapshot {
        verifyReachable(baseUrl)
        val books = deviceSyncService.refreshBooks(baseUrl)
        val settings = runCatching { deviceSyncService.refreshSettings(baseUrl) }.getOrNull()
        val wifiSettings = runCatching { deviceSyncService.refreshWifiSettings(baseUrl) }.getOrNull()
        val deviceFeeds = runCatching { deviceSyncService.refreshRssFeeds(baseUrl).feeds }.getOrDefault(emptyList())
        val mergedFeeds = saveMergedRssFeeds(localRssFeeds, deviceFeeds)
        return CompanionDeviceRefreshSnapshot(
            books = books,
            settings = settings,
            wifiSettings = wifiSettings,
            rssFeeds = mergedFeeds,
            syncedRssFeeds = rssFeedService.mergeRssFeeds(localFeeds = emptyList(), deviceFeeds = deviceFeeds),
            drafts = draftService.loadDrafts(),
        )
    }

    suspend fun syncPendingUploads(baseUrl: String, items: List<PendingUpload>): CompanionPendingSyncSnapshot {
        verifyReachable(baseUrl)
        val readyItems = items.filterNot(::needsArticleFetch)
        val remaining = draftService.syncPendingUploads(client = client, baseUrl = baseUrl, items = readyItems)
        return CompanionPendingSyncSnapshot(
            drafts = remaining,
            books = deviceSyncService.refreshBooks(baseUrl),
            syncedCount = readyItems.size,
        )
    }

    suspend fun saveDraft(item: PendingUpload): CompanionDraftsSnapshot {
        draftService.saveDraft(item)
        return CompanionDraftsSnapshot(drafts = draftService.loadDrafts())
    }

    suspend fun saveDraftFetchingArticleIfNeeded(item: PendingUpload): CompanionDraftSaveSnapshot {
        val fetched = if (needsArticleFetch(item)) {
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

    suspend fun updateDraft(item: PendingUpload, title: String, body: String): CompanionDraftsSnapshot {
        draftService.updateDraft(item, title, body)
        return CompanionDraftsSnapshot(drafts = draftService.loadDrafts())
    }

    suspend fun deleteDraft(item: PendingUpload): CompanionDraftsSnapshot {
        draftService.deleteDraft(item)
        return CompanionDraftsSnapshot(drafts = draftService.loadDrafts())
    }

    suspend fun deleteDrafts(ids: List<String>): CompanionDraftsSnapshot {
        draftService.deleteDrafts(ids)
        return CompanionDraftsSnapshot(drafts = draftService.loadDrafts())
    }

    suspend fun fetchArticle(item: PendingUpload): CompanionArticleFetchSnapshot {
        val article = draftService.fetchArticle(title = item.title, source = item.sourceUrl.orEmpty())
        draftService.updateDraft(item, article.title, article.text)
        return CompanionArticleFetchSnapshot(
            article = article,
            drafts = draftService.loadDrafts(),
        )
    }

    suspend fun fetchArticles(items: List<PendingUpload>): CompanionDraftsSnapshot {
        items.forEach { item ->
            val article = draftService.fetchArticle(title = item.title, source = item.sourceUrl.orEmpty())
            draftService.updateDraft(item, article.title, article.text)
        }
        return CompanionDraftsSnapshot(drafts = draftService.loadDrafts())
    }

    fun needsArticleFetch(item: PendingUpload): Boolean = draftService.needsArticleFetch(item)

    suspend fun saveRssFeeds(
        baseUrl: String,
        feeds: List<String>,
        syncToDevice: Boolean,
    ): CompanionRssSnapshot {
        val normalized = rssFeedService.saveRssFeeds(feeds)
        if (!syncToDevice) {
            return CompanionRssSnapshot(
                rssFeeds = normalized,
                syncedRssFeeds = emptyList(),
                didSyncDevice = false,
            )
        }

        verifyReachable(baseUrl)
        val deviceFeeds = deviceSyncService.saveRssFeeds(baseUrl, normalized).feeds
        val syncedFeeds = rssFeedService.mergeRssFeeds(localFeeds = emptyList(), deviceFeeds = deviceFeeds)
        val mergedFeeds = saveMergedRssFeeds(normalized, syncedFeeds)
        return CompanionRssSnapshot(
            rssFeeds = mergedFeeds,
            syncedRssFeeds = syncedFeeds,
            didSyncDevice = true,
        )
    }

    suspend fun refreshRssFeeds(baseUrl: String, localRssFeeds: List<String>): CompanionRssSnapshot {
        verifyReachable(baseUrl)
        val deviceFeeds = deviceSyncService.refreshRssFeeds(baseUrl).feeds
        val syncedFeeds = rssFeedService.mergeRssFeeds(localFeeds = emptyList(), deviceFeeds = deviceFeeds)
        val mergedFeeds = saveMergedRssFeeds(localRssFeeds, syncedFeeds)
        return CompanionRssSnapshot(
            rssFeeds = mergedFeeds,
            syncedRssFeeds = syncedFeeds,
            didSyncDevice = false,
        )
    }

    suspend fun uploadBook(baseUrl: String, file: RsvpBookFile, category: String): CompanionBooksSnapshot {
        verifyReachable(baseUrl)
        deviceSyncService.uploadBook(
            baseUrl = baseUrl,
            filename = file.filename,
            data = file.data,
            category = category,
        )
        return CompanionBooksSnapshot(books = deviceSyncService.refreshBooks(baseUrl))
    }

    suspend fun deleteBooks(baseUrl: String, bookIds: List<String>): CompanionBooksSnapshot {
        verifyReachable(baseUrl)
        bookIds.forEach { bookId ->
            deviceSyncService.deleteBook(baseUrl, bookId)
        }
        return CompanionBooksSnapshot(books = deviceSyncService.refreshBooks(baseUrl))
    }

    suspend fun refreshSettings(baseUrl: String): CompanionSettingsSnapshot {
        verifyReachable(baseUrl)
        return CompanionSettingsSnapshot(
            settings = deviceSyncService.refreshSettings(baseUrl),
            wifiSettings = runCatching { deviceSyncService.refreshWifiSettings(baseUrl) }.getOrNull(),
        )
    }

    suspend fun saveSettings(baseUrl: String, settings: NanoSettings): CompanionSettingsSnapshot {
        verifyReachable(baseUrl)
        return CompanionSettingsSnapshot(
            settings = deviceSyncService.saveSettings(baseUrl, settings),
            wifiSettings = null,
        )
    }

    suspend fun refreshWifiSettings(baseUrl: String): CompanionWifiSnapshot {
        verifyReachable(baseUrl)
        return CompanionWifiSnapshot(wifiSettings = deviceSyncService.refreshWifiSettings(baseUrl))
    }

    suspend fun saveWifiSettings(baseUrl: String, ssid: String, password: String): CompanionWifiSnapshot {
        verifyReachable(baseUrl)
        return CompanionWifiSnapshot(
            wifiSettings = deviceSyncService.saveWifiSettings(baseUrl, ssid, password),
        )
    }

    suspend fun clearWifiSettings(baseUrl: String): CompanionWifiSnapshot {
        verifyReachable(baseUrl)
        return CompanionWifiSnapshot(wifiSettings = deviceSyncService.clearWifiSettings(baseUrl))
    }

    suspend fun verifyReachable(baseUrl: String) {
        deviceSyncService.verifyReachable(baseUrl)
    }

    suspend fun verifyReachableWithRetry(
        baseUrl: String,
        attempts: Int = DEFAULT_CONNECTION_ATTEMPTS,
        retryDelayMillis: Long = DEFAULT_CONNECTION_RETRY_DELAY_MILLIS,
    ) {
        retryDeviceOperation(attempts, retryDelayMillis) {
            deviceSyncService.verifyReachable(baseUrl)
        }
    }

    private suspend fun saveMergedRssFeeds(localFeeds: List<String>, deviceFeeds: List<String>): List<String> {
        return rssFeedService.saveRssFeeds(
            rssFeedService.mergeRssFeeds(
                localFeeds = localFeeds,
                deviceFeeds = deviceFeeds,
            )
        )
    }

    private suspend fun <T> retryDeviceOperation(
        attempts: Int,
        retryDelayMillis: Long,
        operation: suspend () -> T,
    ): T {
        var lastError: Throwable? = null
        repeat(attempts.coerceAtLeast(1)) { index ->
            try {
                return operation()
            } catch (error: Throwable) {
                lastError = error
                if (index < attempts - 1 && retryDelayMillis > 0) {
                    delay(retryDelayMillis)
                }
            }
        }
        throw lastError ?: IllegalStateException("Device operation failed.")
    }

    companion object {
        const val DEFAULT_CONNECTION_ATTEMPTS = 4
        const val DEFAULT_CONNECTION_RETRY_DELAY_MILLIS = 750L
    }
}

data class CompanionLocalSnapshot(
    val drafts: List<PendingUpload>,
    val rssFeeds: List<String>,
)

data class CompanionConnectSnapshot(
    val device: NanoDeviceSnapshot,
    val rssFeeds: List<String>,
    val syncedRssFeeds: List<String>,
    val drafts: List<PendingUpload>,
)

data class CompanionDeviceRefreshSnapshot(
    val books: List<NanoBook>,
    val settings: NanoSettings?,
    val wifiSettings: NanoWifiSettings?,
    val rssFeeds: List<String>,
    val syncedRssFeeds: List<String>,
    val drafts: List<PendingUpload>,
)

data class CompanionPendingSyncSnapshot(
    val drafts: List<PendingUpload>,
    val books: List<NanoBook>,
    val syncedCount: Int,
)

data class CompanionDraftsSnapshot(
    val drafts: List<PendingUpload>,
)

data class CompanionDraftSaveSnapshot(
    val drafts: List<PendingUpload>,
    val item: PendingUpload,
    val fetchedArticle: Boolean,
)

data class CompanionArticleFetchSnapshot(
    val article: SharedArticle,
    val drafts: List<PendingUpload>,
)

data class CompanionRssSnapshot(
    val rssFeeds: List<String>,
    val syncedRssFeeds: List<String>,
    val didSyncDevice: Boolean,
)

data class CompanionBooksSnapshot(
    val books: List<NanoBook>,
)

data class CompanionSettingsSnapshot(
    val settings: NanoSettings,
    val wifiSettings: NanoWifiSettings?,
)

data class CompanionWifiSnapshot(
    val wifiSettings: NanoWifiSettings,
)
