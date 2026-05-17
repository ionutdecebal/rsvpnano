package com.rsvpnano

import com.rsvpnano.app.RsvpSharedFacade
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.persistence.PendingUploadArticleService
import com.rsvpnano.persistence.PendingUploadStore
import com.rsvpnano.persistence.RssFeedStore
import kotlin.test.Test
import kotlin.test.assertEquals

class RsvpSharedFacadeTest {
    @Test
    fun delegatesDraftAndFeedOperations() {
        val initialDrafts = listOf(
            PendingUpload(
                id = "1",
                title = "Shared Article",
                sourceUrl = "https://example.com/story",
                body = "https://example.com/story",
                createdAt = "2026-05-17T10:00:00Z",
            )
        )
        val feeds = listOf("https://example.com/feed")
        val facade = RsvpSharedFacade(
            pendingUploadStore = object : PendingUploadStore {
                var items = initialDrafts

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
            },
            rssFeedStore = object : RssFeedStore {
                override suspend fun loadAll(): List<String> = feeds
                override suspend fun saveAll(items: List<String>) = Unit
            },
            articleService = PendingUploadArticleService(),
        )

        kotlinx.coroutines.runBlocking {
            assertEquals(initialDrafts, facade.loadDrafts())
            facade.saveDraft(
                PendingUpload(
                    id = "2",
                    title = "Second",
                    sourceUrl = null,
                    body = "Draft body",
                    createdAt = "2026-05-17T11:00:00Z",
                ),
            )
            assertEquals("2", facade.loadDrafts().first().id)

            val draft = facade.loadDrafts().first()
            facade.updateDraft(draft, title = "Updated Draft", body = "Updated body")
            assertEquals("Updated Draft", facade.loadDrafts().first().title)
            assertEquals("Updated body", facade.loadDrafts().first().body)

            facade.deleteDraft(draft)
            assertEquals(initialDrafts, facade.loadDrafts())

            assertEquals(feeds, facade.loadRssFeeds())
        }
        assertEquals(true, facade.needsArticleFetch(initialDrafts[0]))
    }
}
