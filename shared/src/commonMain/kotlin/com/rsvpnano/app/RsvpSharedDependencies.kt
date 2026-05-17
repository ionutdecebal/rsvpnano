package com.rsvpnano.app

import com.rsvpnano.api.NanoClient
import com.rsvpnano.app.NanoDeviceSyncService
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
) {
    fun createFacade(): RsvpSharedFacade {
        return RsvpSharedFacade(
            pendingUploadStore = PendingUploadJsonStore(pendingUploadStorage),
            rssFeedStore = JsonRssFeedStore(rssFeedStorage),
        )
    }

    fun createPendingUploadRepository(): PendingUploadRepository {
        return PendingUploadRepository(PendingUploadJsonStore(pendingUploadStorage))
    }

    fun createDeviceSyncService(client: NanoClient): NanoDeviceSyncService {
        return NanoDeviceSyncService(client)
    }
}
