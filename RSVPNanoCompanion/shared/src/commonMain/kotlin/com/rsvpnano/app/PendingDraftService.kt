package com.rsvpnano.app

import com.rsvpnano.api.ArticleFetchClient
import com.rsvpnano.api.NanoClient
import com.rsvpnano.converters.RsvpBookFile
import com.rsvpnano.converters.SharedArticle
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.persistence.PendingUploadArticleService
import com.rsvpnano.persistence.PendingUploadRepository
import com.rsvpnano.sync.PendingUploadSyncService

/**
 * Shared domain service for saved article drafts and pending uploads.
 */
class PendingDraftService(
    private val repository: PendingUploadRepository,
    private val articleService: PendingUploadArticleService = PendingUploadArticleService(),
    private val articleFetchClient: ArticleFetchClient? = null,
) {
    private val uploadSyncService = PendingUploadSyncService(repository, articleService)

    suspend fun fetchArticle(title: String, source: String): SharedArticle {
        val client = articleFetchClient ?: throw IllegalStateException("ArticleFetchClient not provided to draft service")
        return client.fetch(title, source)
    }

    suspend fun fetchArticleIfAvailable(title: String, source: String): SharedArticle? {
        val client = articleFetchClient ?: return null
        return runCatching { client.fetch(title, source) }.getOrNull()
    }

    suspend fun loadDrafts(): List<PendingUpload> = repository.loadAll()

    suspend fun saveDraft(item: PendingUpload) {
        repository.save(item)
    }

    suspend fun updateDraft(item: PendingUpload, title: String, body: String) {
        repository.update(item, title, body)
    }

    suspend fun deleteDraft(item: PendingUpload) {
        repository.delete(item)
    }

    suspend fun deleteDrafts(ids: List<String>) {
        repository.delete(ids)
    }

    fun needsArticleFetch(item: PendingUpload): Boolean = repository.needsArticleFetch(item)

    fun articleFor(item: PendingUpload) = articleService.articleFor(item)

    fun bookFileFor(item: PendingUpload): RsvpBookFile = repository.bookFileFor(item)

    suspend fun syncPendingUpload(client: NanoClient, baseUrl: String, item: PendingUpload): RsvpBookFile =
        uploadSyncService.syncOne(client, baseUrl, item)

    suspend fun syncPendingUploads(client: NanoClient, baseUrl: String, items: List<PendingUpload>): List<PendingUpload> =
        uploadSyncService.syncAll(client, baseUrl, items)
}
