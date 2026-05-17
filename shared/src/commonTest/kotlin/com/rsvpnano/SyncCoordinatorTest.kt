package com.rsvpnano

import com.rsvpnano.persistence.RssFeedStore
import com.rsvpnano.sync.SyncCoordinator
import kotlin.test.Test
import kotlin.test.assertEquals

class SyncCoordinatorTest {
    @Test
    fun mergeRssFeedsDeduplicatesAndFiltersInvalidValues() {
        val coordinator = SyncCoordinator(
            rssFeedStore = NoopRssFeedStore(),
        )

        assertEquals(
            listOf("https://example.com/feed", "http://example.org/rss"),
            coordinator.mergeRssFeeds(
                localFeeds = listOf(" https://example.com/feed ", "ftp://bad", "https://example.com/feed"),
                deviceFeeds = listOf("http://example.org/rss", "", "https://example.com/feed"),
            ),
        )
    }

    private class NoopRssFeedStore : RssFeedStore {
        override suspend fun loadAll() = emptyList<String>()
        override suspend fun saveAll(items: List<String>) = Unit
    }
}
