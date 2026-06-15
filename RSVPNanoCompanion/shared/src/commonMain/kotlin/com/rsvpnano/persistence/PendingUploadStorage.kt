package com.rsvpnano.persistence

/**
 * Low-level platform storage for the pending upload JSON blob.
 *
 * Keep the platform-specific file access behind this interface so the shared logic stays reusable.
 */
interface PendingUploadStorage {
    suspend fun readText(): String?
    suspend fun writeText(value: String)
}
