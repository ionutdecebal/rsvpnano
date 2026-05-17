package com.rsvpnano

import com.rsvpnano.api.NanoClient
import com.rsvpnano.converters.RsvpBookFile
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.persistence.PendingUploadArticleService
import com.rsvpnano.persistence.PendingUploadRepository
import com.rsvpnano.persistence.PendingUploadStore
import com.rsvpnano.sync.PendingUploadSyncService
import kotlinx.coroutines.runBlocking
import kotlin.test.Test
import kotlin.test.assertEquals

class PendingUploadSyncServiceTest {
    @Test
    fun syncOneUploadsAndRemovesDraft() {
        val storage = InMemoryPendingStore(
            listOf(
                PendingUpload(
                    id = "1",
                    title = "Example",
                    sourceUrl = "https://example.com/story",
                    body = "Hello reader.",
                    createdAt = "2026-05-17T10:00:00Z",
                )
            )
        )
        val repository = PendingUploadRepository(storage, PendingUploadArticleService())
        val service = PendingUploadSyncService(repository, PendingUploadArticleService())
        val client = RecordingNanoClient()

        runBlocking {
            val file = service.syncOne(client, "http://device.local", storage.items.first())
            assertEquals("Example.rsvp", file.filename)
            assertEquals(emptyList(), storage.items)
            assertEquals("Example.rsvp", client.uploadedFilename)
        }
    }

    private class RecordingNanoClient : NanoClient {
        var uploadedFilename: String? = null

        override suspend fun fetchInfo(baseUrl: String): NanoInfo = NanoInfo(name = "Nano")

        override suspend fun listBooks(baseUrl: String): List<NanoBook> = emptyList()

        override suspend fun uploadBook(baseUrl: String, name: String, data: ByteArray, category: String?): NanoBook {
            uploadedFilename = name
            return NanoBook(id = name, title = name, category = category)
        }
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
