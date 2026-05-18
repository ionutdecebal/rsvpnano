package com.rsvpnano.app

import com.rsvpnano.api.NanoClient
import com.rsvpnano.converters.RsvpBookFile
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.PendingUpload

/**
 * Shared workflow controller for app-level device operations.
 *
 * Platform ViewModels should own UI state, but this class owns the repeated sequencing between
 * local companion data, device sync calls, and post-mutation refreshes.
 */
class NanoCompanionController(
    private val facade: RsvpSharedFacade,
    private val deviceSyncService: NanoDeviceSyncService,
    private val client: NanoClient,
) {
    suspend fun refreshLocal(): CompanionLocalSnapshot {
        return CompanionLocalSnapshot(
            drafts = facade.loadDrafts(),
            rssFeeds = facade.loadRssFeeds(),
        )
    }

    suspend fun connect(baseUrl: String, localRssFeeds: List<String>): CompanionConnectSnapshot {
        val device = deviceSyncService.connect(baseUrl)
        val mergedFeeds = saveMergedRssFeeds(localRssFeeds, device.rssFeeds?.feeds.orEmpty())
        return CompanionConnectSnapshot(
            device = device,
            rssFeeds = mergedFeeds,
            drafts = facade.loadDrafts(),
        )
    }

    suspend fun refreshDevice(baseUrl: String, localRssFeeds: List<String>): CompanionDeviceRefreshSnapshot {
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
            syncedRssFeeds = facade.mergeRssFeeds(localFeeds = emptyList(), deviceFeeds = deviceFeeds),
            drafts = facade.loadDrafts(),
        )
    }

    suspend fun syncPendingUploads(baseUrl: String, items: List<PendingUpload>): CompanionPendingSyncSnapshot {
        val remaining = facade.syncPendingUploads(client = client, baseUrl = baseUrl, items = items)
        return CompanionPendingSyncSnapshot(
            drafts = remaining,
            books = deviceSyncService.refreshBooks(baseUrl),
            syncedCount = items.size,
        )
    }

    suspend fun saveRssFeeds(
        baseUrl: String,
        feeds: List<String>,
        syncToDevice: Boolean,
    ): CompanionRssSnapshot {
        val normalized = facade.saveRssFeeds(feeds)
        if (!syncToDevice) {
            return CompanionRssSnapshot(
                rssFeeds = normalized,
                syncedRssFeeds = emptyList(),
                didSyncDevice = false,
            )
        }

        val deviceFeeds = deviceSyncService.saveRssFeeds(baseUrl, normalized).feeds
        val syncedFeeds = facade.mergeRssFeeds(localFeeds = emptyList(), deviceFeeds = deviceFeeds)
        val mergedFeeds = saveMergedRssFeeds(normalized, syncedFeeds)
        return CompanionRssSnapshot(
            rssFeeds = mergedFeeds,
            syncedRssFeeds = syncedFeeds,
            didSyncDevice = true,
        )
    }

    suspend fun refreshRssFeeds(baseUrl: String, localRssFeeds: List<String>): CompanionRssSnapshot {
        val deviceFeeds = deviceSyncService.refreshRssFeeds(baseUrl).feeds
        val syncedFeeds = facade.mergeRssFeeds(localFeeds = emptyList(), deviceFeeds = deviceFeeds)
        val mergedFeeds = saveMergedRssFeeds(localRssFeeds, syncedFeeds)
        return CompanionRssSnapshot(
            rssFeeds = mergedFeeds,
            syncedRssFeeds = syncedFeeds,
            didSyncDevice = false,
        )
    }

    suspend fun uploadBook(baseUrl: String, file: RsvpBookFile, category: String): CompanionBooksSnapshot {
        deviceSyncService.uploadBook(
            baseUrl = baseUrl,
            filename = file.filename,
            data = file.data,
            category = category,
        )
        return CompanionBooksSnapshot(books = deviceSyncService.refreshBooks(baseUrl))
    }

    suspend fun deleteBooks(baseUrl: String, bookIds: List<String>): CompanionBooksSnapshot {
        bookIds.forEach { bookId ->
            deviceSyncService.deleteBook(baseUrl, bookId)
        }
        return CompanionBooksSnapshot(books = deviceSyncService.refreshBooks(baseUrl))
    }

    suspend fun refreshSettings(baseUrl: String): CompanionSettingsSnapshot {
        return CompanionSettingsSnapshot(
            settings = deviceSyncService.refreshSettings(baseUrl),
            wifiSettings = runCatching { deviceSyncService.refreshWifiSettings(baseUrl) }.getOrNull(),
        )
    }

    suspend fun saveSettings(baseUrl: String, settings: NanoSettings): CompanionSettingsSnapshot {
        return CompanionSettingsSnapshot(
            settings = deviceSyncService.saveSettings(baseUrl, settings),
            wifiSettings = null,
        )
    }

    suspend fun refreshWifiSettings(baseUrl: String): CompanionWifiSnapshot {
        return CompanionWifiSnapshot(wifiSettings = deviceSyncService.refreshWifiSettings(baseUrl))
    }

    suspend fun saveWifiSettings(baseUrl: String, ssid: String, password: String): CompanionWifiSnapshot {
        return CompanionWifiSnapshot(
            wifiSettings = deviceSyncService.saveWifiSettings(baseUrl, ssid, password),
        )
    }

    suspend fun clearWifiSettings(baseUrl: String): CompanionWifiSnapshot {
        return CompanionWifiSnapshot(wifiSettings = deviceSyncService.clearWifiSettings(baseUrl))
    }

    private suspend fun saveMergedRssFeeds(localFeeds: List<String>, deviceFeeds: List<String>): List<String> {
        return facade.saveRssFeeds(
            facade.mergeRssFeeds(
                localFeeds = localFeeds,
                deviceFeeds = deviceFeeds,
            )
        )
    }
}

data class CompanionLocalSnapshot(
    val drafts: List<PendingUpload>,
    val rssFeeds: List<String>,
)

data class CompanionConnectSnapshot(
    val device: NanoDeviceSnapshot,
    val rssFeeds: List<String>,
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
