package com.rsvpnano.app

import com.rsvpnano.persistence.RssFeedStore
import com.rsvpnano.sync.SyncCoordinator

/**
 * Shared domain service for local RSS feed persistence and merge rules.
 */
class RssFeedService(
    private val syncCoordinator: SyncCoordinator,
) {
    constructor(rssFeedStore: RssFeedStore) : this(SyncCoordinator(rssFeedStore))

    suspend fun loadRssFeeds(): List<String> = syncCoordinator.loadRssFeeds()

    suspend fun saveRssFeeds(localFeeds: List<String>): List<String> =
        syncCoordinator.saveRssFeeds(localFeeds)

    fun mergeRssFeeds(localFeeds: List<String>, deviceFeeds: List<String>): List<String> =
        syncCoordinator.mergeRssFeeds(localFeeds, deviceFeeds)
}
