package com.rsvpnano

import com.rsvpnano.models.PendingUpload
import com.rsvpnano.models.needsArticleFetch
import kotlin.test.Test
import kotlin.test.assertEquals

class PendingUploadExtensionsTest {
    @Test
    fun mirrorsSwiftNeedsArticleFetchLogic() {
        assertEquals(
            true,
            PendingUpload(
                id = "1",
                title = "Example",
                sourceUrl = "https://example.com/story",
                body = "https://example.com/story",
                createdAt = "2026-05-17T10:00:00Z",
            ).needsArticleFetch(),
        )

        assertEquals(
            false,
            PendingUpload(
                id = "2",
                title = "Example",
                sourceUrl = "mailto:someone@example.com",
                body = "mailto:someone@example.com",
                createdAt = "2026-05-17T10:00:00Z",
            ).needsArticleFetch(),
        )
    }
}
