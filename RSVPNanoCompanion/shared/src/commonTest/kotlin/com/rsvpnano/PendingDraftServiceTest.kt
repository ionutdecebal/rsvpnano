package com.rsvpnano

import com.rsvpnano.app.PendingDraftService
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.persistence.PendingUploadRepository
import com.rsvpnano.persistence.PendingUploadStore
import kotlinx.coroutines.runBlocking
import kotlin.test.Test
import kotlin.test.assertEquals

class PendingDraftServiceTest {
    @Test
    fun savesUpdatesAndDeletesDrafts() = runBlocking {
        val store = InMemoryPendingStore(emptyList())
        val service = PendingDraftService(PendingUploadRepository(store))
        val item = PendingUpload(
            id = "1",
            title = "Draft",
            sourceUrl = "https://example.com",
            body = "https://example.com",
            createdAt = "2026-05-18T10:00:00Z",
        )

        service.saveDraft(item)
        assertEquals(listOf("Draft"), service.loadDrafts().map { it.title })
        assertEquals(true, service.needsArticleFetch(item))

        service.updateDraft(item, title = "Updated", body = "Article body")
        assertEquals("Updated", service.loadDrafts().single().title)
        assertEquals("Article body", service.loadDrafts().single().body)

        service.deleteDrafts(listOf("1"))
        assertEquals(emptyList(), service.loadDrafts())
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
