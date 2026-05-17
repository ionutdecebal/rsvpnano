package com.rsvpnano.sync

import com.rsvpnano.persistence.PendingUploadStore
import com.rsvpnano.persistence.RssFeedStore

/**
 * Small shared coordinator for feed and draft synchronization.
 *
 * Keep UI state in Swift/Compose; this layer only handles data normalization and persistence.
 */
class SyncCoordinator(
    private val pendingUploadStore: PendingUploadStore,
    private val rssFeedStore: RssFeedStore,
) {
    suspend fun loadDrafts() = pendingUploadStore.loadAll()

    suspend fun loadRssFeeds(): List<String> = RssFeedNormalizer.normalize(rssFeedStore.loadAll())

    suspend fun saveRssFeeds(localFeeds: List<String>): List<String> {
        val normalized = RssFeedNormalizer.normalize(localFeeds)
        rssFeedStore.saveAll(normalized)
        return normalized
    }

    fun mergeRssFeeds(localFeeds: List<String>, deviceFeeds: List<String>): List<String> {
        return RssFeedNormalizer.normalize(localFeeds + deviceFeeds)
    }
}
