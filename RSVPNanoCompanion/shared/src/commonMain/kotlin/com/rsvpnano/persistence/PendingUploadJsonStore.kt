package com.rsvpnano.persistence

import com.rsvpnano.models.PendingUpload
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json

/**
 * JSON-backed implementation of the pending-upload store.
 *
 * The storage backend is injected, which keeps the file system, app group, or app sandbox
 * details out of common code.
 */
class PendingUploadJsonStore(
    private val storage: PendingUploadStorage,
    private val json: Json = Json {
        ignoreUnknownKeys = true
        encodeDefaults = true
        explicitNulls = false
        prettyPrint = false
    },
) : PendingUploadStore {
    override suspend fun loadAll(): List<PendingUpload> {
        val text = storage.readText() ?: return emptyList()
        return runCatching { json.decodeFromString(PendingUploadList.serializer(), text).items }
            .getOrDefault(emptyList())
            .sortedByDescending(PendingUpload::createdAt)
    }

    override suspend fun saveAll(items: List<PendingUpload>) {
        storage.writeText(json.encodeToString(PendingUploadList.serializer(), PendingUploadList(items.sortedByDescending(PendingUpload::createdAt))))
    }

    override suspend fun add(item: PendingUpload) {
        val next = loadAll().toMutableList()
        next.add(0, item)
        saveAll(next)
    }

    override suspend fun remove(id: String) {
        saveAll(loadAll().filterNot { it.id == id })
    }

    @Serializable
    private data class PendingUploadList(
        val items: List<PendingUpload>,
    )
}
