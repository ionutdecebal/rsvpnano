package com.rsvpnano.app

import com.rsvpnano.api.ArticleFetchClient
import com.rsvpnano.api.NanoClient
import com.rsvpnano.persistence.PendingUploadArticleService
import com.rsvpnano.persistence.PendingUploadRepository
import com.rsvpnano.persistence.PendingUploadJsonStore
import com.rsvpnano.persistence.PendingUploadStorage
import com.rsvpnano.persistence.JsonRssFeedStore
import com.rsvpnano.persistence.RssFeedStorage

/**
 * Small dependency container for shared app wiring.
 *
 * Platform code can build this once and keep the actual app adapters lightweight.
 */
data class RsvpSharedDependencies(
    val pendingUploadStorage: PendingUploadStorage,
    val rssFeedStorage: RssFeedStorage,
    val articleFetchClient: ArticleFetchClient? = null,
    val nanoClient: NanoClient? = null,
) {
    fun createApp(): RsvpSharedApp {
        return RsvpSharedApp(this)
    }

    fun createPendingUploadRepository(): PendingUploadRepository {
        return PendingUploadRepository(PendingUploadJsonStore(pendingUploadStorage))
    }

    fun createPendingDraftService(): PendingDraftService {
        val articleService = PendingUploadArticleService()
        return PendingDraftService(
            repository = PendingUploadRepository(
                store = PendingUploadJsonStore(pendingUploadStorage),
                articleService = articleService,
            ),
            articleService = articleService,
            articleFetchClient = articleFetchClient,
        )
    }

    fun createRssFeedService(): RssFeedService {
        return RssFeedService(JsonRssFeedStore(rssFeedStorage))
    }

    fun createDeviceSyncService(): NanoDeviceSyncService {
        val client = nanoClient ?: throw IllegalStateException("NanoClient not provided to dependencies")
        return NanoDeviceSyncService(client)
    }

    fun createDeviceSyncService(client: NanoClient): NanoDeviceSyncService {
        return NanoDeviceSyncService(client)
    }

    fun createCompanionController(
        draftService: PendingDraftService = createPendingDraftService(),
        rssFeedService: RssFeedService = createRssFeedService(),
    ): NanoCompanionController {
        val client = nanoClient ?: throw IllegalStateException("NanoClient not provided to dependencies")
        return NanoCompanionController(
            draftService = draftService,
            rssFeedService = rssFeedService,
            deviceSyncService = NanoDeviceSyncService(client),
            client = client,
        )
    }
}
