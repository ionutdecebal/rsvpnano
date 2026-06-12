package com.rsvpnano.persistence

import com.rsvpnano.models.PendingUpload

/**
 * Platform-agnostic persistence interface for pending uploads (draft articles).
 * Implement this in `iosMain` and `androidMain` using platform storage APIs.
 */
interface PendingUploadStore {
    suspend fun loadAll(): List<PendingUpload>
    suspend fun saveAll(items: List<PendingUpload>)
    suspend fun add(item: PendingUpload)
    suspend fun remove(id: String)
}
