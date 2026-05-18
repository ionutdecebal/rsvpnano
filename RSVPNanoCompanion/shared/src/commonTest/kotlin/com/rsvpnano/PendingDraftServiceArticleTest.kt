package com.rsvpnano

import com.rsvpnano.app.PendingDraftService
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.persistence.PendingUploadArticleService
import com.rsvpnano.persistence.PendingUploadRepository
import com.rsvpnano.persistence.PendingUploadStore
import kotlin.test.Test
import kotlin.test.assertEquals

class PendingDraftServiceArticleTest {
    @Test
    fun producesBookFileFromPendingUpload() {
        val articleService = PendingUploadArticleService()
        val service = PendingDraftService(
            repository = PendingUploadRepository(
                store = object : PendingUploadStore {
                    override suspend fun loadAll(): List<PendingUpload> = emptyList()
                    override suspend fun saveAll(items: List<PendingUpload>) = Unit
                    override suspend fun add(item: PendingUpload) = Unit
                    override suspend fun remove(id: String) = Unit
                },
                articleService = articleService,
            ),
            articleService = articleService,
        )

        val item = PendingUpload(
            id = "1",
            title = "Shared Article",
            sourceUrl = "https://example.com/story",
            body = "<html><body><p>Hello world.</p></body></html>",
            createdAt = "2026-05-17T10:00:00Z",
        )

        val bookFile = service.bookFileFor(item)

        assertEquals("Shared Article.rsvp", bookFile.filename)
        assertEquals(true, bookFile.data.decodeToString().contains("Hello world."))
    }
}
