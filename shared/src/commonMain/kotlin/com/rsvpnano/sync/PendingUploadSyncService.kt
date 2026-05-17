package com.rsvpnano.sync

import com.rsvpnano.api.NanoClient
import com.rsvpnano.converters.RsvpBookFile
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.persistence.PendingUploadArticleService
import com.rsvpnano.persistence.PendingUploadRepository

/**
 * Shared upload flow for pending drafts.
 *
 * This keeps the article conversion and upload bookkeeping in one place.
 */
class PendingUploadSyncService(
    private val pendingUploadRepository: PendingUploadRepository,
    private val articleService: PendingUploadArticleService = PendingUploadArticleService(),
) {
    fun bookFileFor(item: PendingUpload): RsvpBookFile = articleService.bookFileFor(item)

    suspend fun syncAll(client: NanoClient, baseUrl: String, items: List<PendingUpload>): List<PendingUpload> {
        items.forEach { syncOne(client, baseUrl, it) }
        return pendingUploadRepository.loadAll()
    }

    suspend fun syncOne(client: NanoClient, baseUrl: String, item: PendingUpload): RsvpBookFile {
        val file = bookFileFor(item)
        client.uploadBook(baseUrl = baseUrl, name = file.filename, data = file.data, category = "article")
        pendingUploadRepository.delete(item)
        return file
    }
}
