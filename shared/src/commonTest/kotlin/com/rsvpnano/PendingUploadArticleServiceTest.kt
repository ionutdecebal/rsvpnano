package com.rsvpnano

import com.rsvpnano.models.PendingUpload
import com.rsvpnano.persistence.PendingUploadArticleService
import kotlin.test.Test
import kotlin.test.assertEquals

class PendingUploadArticleServiceTest {
    @Test
    fun detectsArticleFetchDrafts() {
        val service = PendingUploadArticleService()

        assertEquals(
            true,
            service.needsArticleFetch(
                PendingUpload(
                    id = "1",
                    title = "Example",
                    sourceUrl = "https://example.com/story",
                    body = "https://example.com/story",
                    createdAt = "2026-05-17T10:00:00Z",
                ),
            ),
        )
        assertEquals(
            false,
            service.needsArticleFetch(
                PendingUpload(
                    id = "2",
                    title = "Example",
                    sourceUrl = "mailto:someone@example.com",
                    body = "mailto:someone@example.com",
                    createdAt = "2026-05-17T10:00:00Z",
                ),
            ),
        )
    }
}
