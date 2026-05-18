package com.rsvpnano

import com.rsvpnano.app.RssFeedService
import com.rsvpnano.persistence.RssFeedStore
import kotlinx.coroutines.runBlocking
import kotlin.test.Test
import kotlin.test.assertEquals

class RssFeedServiceTest {
    @Test
    fun savesAndMergesNormalizedFeeds() = runBlocking {
        val store = InMemoryRssStore()
        val service = RssFeedService(store)

        val saved = service.saveRssFeeds(
            listOf(" https://example.com/feed ", "ftp://bad", "https://example.com/feed"),
        )
        val merged = service.mergeRssFeeds(
            localFeeds = saved,
            deviceFeeds = listOf("http://reader.local/rss", "https://example.com/feed"),
        )

        assertEquals(listOf("https://example.com/feed"), store.items)
        assertEquals(listOf("https://example.com/feed"), saved)
        assertEquals(listOf("https://example.com/feed", "http://reader.local/rss"), merged)
    }

    @Test
    fun saveRssFeedsTrimsFiltersAndPreservesFirstValidOccurrence() = runBlocking {
        val store = InMemoryRssStore()
        val service = RssFeedService(store)

        val saved = service.saveRssFeeds(
            listOf(
                "",
                "  ",
                "ftp://example.com/feed",
                " https://example.com/feed ",
                "https://example.com/feed",
                "http://reader.local/rss",
            ),
        )

        assertEquals(
            listOf("https://example.com/feed", "http://reader.local/rss"),
            saved,
        )
        assertEquals(saved, store.items)
    }

    @Test
    fun mergeRssFeedsPreservesLocalOrderThenAddsNewDeviceFeeds() {
        val service = RssFeedService(InMemoryRssStore())

        val merged = service.mergeRssFeeds(
            localFeeds = listOf(
                "https://local.example.com/a",
                "https://shared.example.com/feed",
                "https://local.example.com/b",
            ),
            deviceFeeds = listOf(
                "https://reader.example.com/feed",
                "https://shared.example.com/feed",
                "https://reader.example.com/feed",
            ),
        )

        assertEquals(
            listOf(
                "https://local.example.com/a",
                "https://shared.example.com/feed",
                "https://local.example.com/b",
                "https://reader.example.com/feed",
            ),
            merged,
        )
    }

    @Test
    fun mergeRssFeedsFiltersInvalidDeviceFeeds() {
        val service = RssFeedService(InMemoryRssStore())

        val merged = service.mergeRssFeeds(
            localFeeds = listOf("https://local.example.com/feed"),
            deviceFeeds = listOf(
                " ",
                "mailto:reader@example.com",
                " http://reader.example.com/feed ",
            ),
        )

        assertEquals(
            listOf("https://local.example.com/feed", "http://reader.example.com/feed"),
            merged,
        )
    }

    private class InMemoryRssStore(var items: List<String> = emptyList()) : RssFeedStore {
        override suspend fun loadAll(): List<String> = items

        override suspend fun saveAll(items: List<String>) {
            this.items = items
        }
    }
}
