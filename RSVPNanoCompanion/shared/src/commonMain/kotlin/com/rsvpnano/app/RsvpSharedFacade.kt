package com.rsvpnano.app

import com.rsvpnano.api.ArticleFetchClient
import com.rsvpnano.converters.SharedArticle
import com.rsvpnano.converters.RsvpBookFile
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.api.NanoClient
import com.rsvpnano.persistence.PendingUploadArticleService
import com.rsvpnano.persistence.PendingUploadRepository
import com.rsvpnano.persistence.PendingUploadStore
import com.rsvpnano.persistence.RssFeedStore
import com.rsvpnano.sync.SyncCoordinator

/**
 * Compatibility facade over focused shared services.
 * Platform app flows should prefer NanoCompanionController.
 */
class RsvpSharedFacade(
    private val pendingUploadStore: PendingUploadStore,
    private val rssFeedStore: RssFeedStore,
    private val articleService: PendingUploadArticleService = PendingUploadArticleService(),
    private val syncCoordinator: SyncCoordinator = SyncCoordinator(rssFeedStore),
    private val articleFetchClient: ArticleFetchClient? = null,
) {
    private val pendingUploadRepository: PendingUploadRepository = PendingUploadRepository(pendingUploadStore, articleService)
    private val draftService: PendingDraftService = PendingDraftService(
        repository = pendingUploadRepository,
        articleService = articleService,
        articleFetchClient = articleFetchClient,
    )
    private val rssFeedService: RssFeedService = RssFeedService(syncCoordinator)

    suspend fun fetchArticle(title: String, source: String): SharedArticle = draftService.fetchArticle(title, source)

    suspend fun loadDrafts(): List<PendingUpload> = draftService.loadDrafts()

    suspend fun saveDraft(item: PendingUpload) {
        draftService.saveDraft(item)
    }

    suspend fun updateDraft(item: PendingUpload, title: String, body: String) {
        draftService.updateDraft(item, title, body)
    }

    suspend fun deleteDraft(item: PendingUpload) {
        draftService.deleteDraft(item)
    }

    suspend fun deleteDrafts(ids: List<String>) {
        draftService.deleteDrafts(ids)
    }

    suspend fun loadRssFeeds(): List<String> = rssFeedService.loadRssFeeds()

    suspend fun saveRssFeeds(localFeeds: List<String>): List<String> = rssFeedService.saveRssFeeds(localFeeds)

    fun mergeRssFeeds(localFeeds: List<String>, deviceFeeds: List<String>): List<String> =
        rssFeedService.mergeRssFeeds(localFeeds, deviceFeeds)

    fun needsArticleFetch(item: PendingUpload): Boolean = draftService.needsArticleFetch(item)

    fun articleFor(item: PendingUpload) = draftService.articleFor(item)

    fun bookFileFor(item: PendingUpload): RsvpBookFile = draftService.bookFileFor(item)

    suspend fun syncPendingUpload(client: NanoClient, baseUrl: String, item: PendingUpload): RsvpBookFile =
        draftService.syncPendingUpload(client, baseUrl, item)

    suspend fun syncPendingUploads(client: NanoClient, baseUrl: String, items: List<PendingUpload>): List<PendingUpload> =
        draftService.syncPendingUploads(client, baseUrl, items)
}
