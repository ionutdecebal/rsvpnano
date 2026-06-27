package com.rsvpnano.persistence

import com.rsvpnano.converters.RsvpBookFile
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.models.needsArticleFetch

/**
 * Shared CRUD logic for pending uploads.
 *
 * The storage backend remains platform-specific; the business rules stay here.
 */
class PendingUploadRepository(
    private val store: PendingUploadStore,
    private val articleService: PendingUploadArticleService = PendingUploadArticleService(),
) {
    suspend fun loadAll(): List<PendingUpload> = store.loadAll()

    suspend fun save(item: PendingUpload) {
        val next = loadAll().toMutableList()
        val index = next.indexOfFirst { it.id == item.id }
        if (index >= 0) {
            next[index] = item
        } else {
            next.add(0, item)
        }
        store.saveAll(next)
    }

    suspend fun update(item: PendingUpload, title: String, body: String) {
        val cleanedBody = body.trim()
        require(cleanedBody.isNotEmpty()) { "Add some text before saving." }
        val cleanedTitle = title.trim().ifBlank { item.title }
        save(
            item.copy(
                title = cleanedTitle,
                body = cleanedBody,
            ),
        )
    }

    suspend fun delete(item: PendingUpload) {
        store.remove(item.id)
    }

    suspend fun delete(ids: List<String>) {
        val next = loadAll().filterNot { ids.contains(it.id) }
        store.saveAll(next)
    }

    fun needsArticleFetch(item: PendingUpload): Boolean = item.needsArticleFetch()

    fun bookFileFor(item: PendingUpload): RsvpBookFile = articleService.bookFileFor(item)
}
