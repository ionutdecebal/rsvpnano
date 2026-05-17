package com.rsvpnano

import com.rsvpnano.persistence.JsonRssFeedStore
import com.rsvpnano.persistence.RssFeedStorage
import kotlinx.coroutines.runBlocking
import kotlin.test.Test
import kotlin.test.assertEquals

class JsonRssFeedStoreTest {
    @Test
    fun roundTripKeepsNormalizedFeeds() {
        val storage = InMemoryRssStorage()
        val store = JsonRssFeedStore(storage)

        runBlocking {
            store.saveAll(listOf(" https://example.com/feed ", "http://example.org/rss"))
            assertEquals(
                listOf("https://example.com/feed", "http://example.org/rss"),
                store.loadAll(),
            )
        }
    }

    private class InMemoryRssStorage : RssFeedStorage {
        private var value: String? = null

        override suspend fun readText(): String? = value

        override suspend fun writeText(value: String) {
            this.value = value
        }
    }
}
