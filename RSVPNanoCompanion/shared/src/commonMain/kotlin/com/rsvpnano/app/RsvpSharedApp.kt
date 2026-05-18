package com.rsvpnano.app

import com.rsvpnano.api.NanoClient
import com.rsvpnano.persistence.PendingUploadRepository

/**
 * Interop-friendly bundle of shared app services.
 *
 * Platform adapters can hold a single instance and fetch the services they need.
 */
class RsvpSharedApp internal constructor(
    private val dependencies: RsvpSharedDependencies,
    val pendingUploadRepository: PendingUploadRepository = dependencies.createPendingUploadRepository(),
    val pendingDraftService: PendingDraftService = dependencies.createPendingDraftService(),
    val rssFeedService: RssFeedService = dependencies.createRssFeedService(),
) {
    val deviceSyncService: NanoDeviceSyncService by lazy { dependencies.createDeviceSyncService() }
    val companionController: NanoCompanionController by lazy {
        dependencies.createCompanionController(
            draftService = pendingDraftService,
            rssFeedService = rssFeedService,
        )
    }
    val nanoClient: NanoClient? get() = dependencies.nanoClient

    fun createDeviceSyncService(client: NanoClient): NanoDeviceSyncService {
        return dependencies.createDeviceSyncService(client)
    }
}
