package com.rsvpnano.app

import com.rsvpnano.api.ArticleFetchClient
import com.rsvpnano.api.ArticleFetchError
import com.rsvpnano.converters.SharedArticle
import com.rsvpnano.converters.RsvpBookFile
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.models.needsArticleFetch
import com.rsvpnano.api.NanoClient
import com.rsvpnano.persistence.PendingUploadArticleService
import com.rsvpnano.persistence.PendingUploadRepository
import com.rsvpnano.persistence.PendingUploadStore
import com.rsvpnano.persistence.RssFeedStore
import com.rsvpnano.sync.SyncCoordinator
import com.rsvpnano.sync.PendingUploadSyncService

/**
 * Small shared facade that keeps the platform adapters thin.
 *
 * iOS and Android can use this as the single entry point for draft, feed, and article logic.
 */
class RsvpSharedFacade(
    private val pendingUploadStore: PendingUploadStore,
    private val rssFeedStore: RssFeedStore,
    private val articleService: PendingUploadArticleService = PendingUploadArticleService(),
    private val syncCoordinator: SyncCoordinator = SyncCoordinator(rssFeedStore),
    private val articleFetchClient: ArticleFetchClient? = null,
) {
    private val pendingUploadRepository: PendingUploadRepository = PendingUploadRepository(pendingUploadStore, articleService)
    private val uploadSyncService: PendingUploadSyncService = PendingUploadSyncService(pendingUploadRepository, articleService)

    suspend fun fetchArticle(title: String, source: String): SharedArticle {
        val client = articleFetchClient ?: throw IllegalStateException("ArticleFetchClient not provided to facade")
        return client.fetch(title, source)
    }

    suspend fun loadDrafts(): List<PendingUpload> = pendingUploadRepository.loadAll()

    suspend fun saveDraft(item: PendingUpload) {
        pendingUploadRepository.save(item)
    }

    suspend fun updateDraft(item: PendingUpload, title: String, body: String) {
        pendingUploadRepository.update(item, title, body)
    }

    suspend fun deleteDraft(item: PendingUpload) {
        pendingUploadRepository.delete(item)
    }

    suspend fun deleteDrafts(ids: List<String>) {
        pendingUploadRepository.delete(ids)
    }

    suspend fun loadRssFeeds(): List<String> = syncCoordinator.loadRssFeeds()

    suspend fun saveRssFeeds(localFeeds: List<String>): List<String> = syncCoordinator.saveRssFeeds(localFeeds)

    fun mergeRssFeeds(localFeeds: List<String>, deviceFeeds: List<String>): List<String> =
        syncCoordinator.mergeRssFeeds(localFeeds, deviceFeeds)

    fun needsArticleFetch(item: PendingUpload): Boolean = pendingUploadRepository.needsArticleFetch(item)

    fun articleFor(item: PendingUpload) = articleService.articleFor(item)

    fun bookFileFor(item: PendingUpload): RsvpBookFile = pendingUploadRepository.bookFileFor(item)

    suspend fun syncPendingUpload(client: NanoClient, baseUrl: String, item: PendingUpload): RsvpBookFile =
        uploadSyncService.syncOne(client, baseUrl, item)

    suspend fun syncPendingUploads(client: NanoClient, baseUrl: String, items: List<PendingUpload>): List<PendingUpload> =
        uploadSyncService.syncAll(client, baseUrl, items)
}
