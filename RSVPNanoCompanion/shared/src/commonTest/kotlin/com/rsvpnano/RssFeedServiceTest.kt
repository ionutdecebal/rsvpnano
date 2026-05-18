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

    private class InMemoryRssStore(var items: List<String> = emptyList()) : RssFeedStore {
        override suspend fun loadAll(): List<String> = items

        override suspend fun saveAll(items: List<String>) {
            this.items = items
        }
    }
}
