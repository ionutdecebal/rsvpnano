package com.rsvpnano

import com.rsvpnano.models.PendingUpload
import com.rsvpnano.persistence.PendingUploadRepository
import com.rsvpnano.persistence.PendingUploadStore
import kotlin.test.Test
import kotlin.test.assertEquals

class PendingUploadRepositoryTest {
    @Test
    fun updatePreservesItemIdentityAndCreatesNormalizedDrafts() = kotlinx.coroutines.runBlocking {
        val store = InMemoryPendingStore(
            listOf(
                PendingUpload(
                    id = "1",
                    title = "Old",
                    sourceUrl = "https://example.com/story",
                    body = "Old body",
                    createdAt = "2026-05-17T10:00:00Z",
                ),
            ),
        )
        val repository = PendingUploadRepository(store)

        repository.update(store.items.first(), title = "Updated", body = "New body")

        assertEquals("Updated", store.items.first().title)
        assertEquals("New body", store.items.first().body)
        assertEquals("1", store.items.first().id)
    }

    private class InMemoryPendingStore(var items: List<PendingUpload>) : PendingUploadStore {
        override suspend fun loadAll(): List<PendingUpload> = items

        override suspend fun saveAll(items: List<PendingUpload>) {
            this.items = items
        }

        override suspend fun add(item: PendingUpload) {
            items = listOf(item) + items
        }

        override suspend fun remove(id: String) {
            items = items.filterNot { it.id == id }
        }
    }
}
