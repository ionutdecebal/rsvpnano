package com.rsvpnano

import com.rsvpnano.models.PendingUpload
import com.rsvpnano.persistence.PendingUploadJsonStore
import com.rsvpnano.persistence.PendingUploadStorage
import kotlinx.coroutines.runBlocking
import kotlin.test.assertEquals
import kotlin.test.Test

class PendingUploadJsonStoreTest {
    @Test
    fun roundTripPreservesDrafts() {
        val storage = InMemoryStorage()
        val store = PendingUploadJsonStore(storage)

        val item = PendingUpload(
            id = "1",
            title = "Title",
            sourceUrl = "https://example.com",
            body = "Body",
            createdAt = "2026-05-17T10:00:00Z",
        )

        runBlocking {
            store.saveAll(listOf(item))
            assertEquals(listOf(item), store.loadAll())
        }
    }

    private class InMemoryStorage : PendingUploadStorage {
        private var value: String? = null

        override suspend fun readText(): String? = value

        override suspend fun writeText(value: String) {
            this.value = value
        }
    }
}
