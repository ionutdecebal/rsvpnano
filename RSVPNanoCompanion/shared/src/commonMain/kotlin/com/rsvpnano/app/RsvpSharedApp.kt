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
    val facade: RsvpSharedFacade = dependencies.createFacade(),
    val pendingUploadRepository: PendingUploadRepository = dependencies.createPendingUploadRepository(),
) {
    val deviceSyncService: NanoDeviceSyncService by lazy { dependencies.createDeviceSyncService() }
    val nanoClient: NanoClient? get() = dependencies.nanoClient

    fun createDeviceSyncService(client: NanoClient): NanoDeviceSyncService {
        return dependencies.createDeviceSyncService(client)
    }
}
