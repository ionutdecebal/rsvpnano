package com.rsvpnano.persistence

import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json

/**
 * JSON-backed RSS feed store.
 *
 * The storage backend stays platform-specific; the merge and normalization rules stay shared.
 */
class JsonRssFeedStore(
    private val storage: RssFeedStorage,
    private val json: Json = Json {
        ignoreUnknownKeys = true
        encodeDefaults = true
        explicitNulls = false
        prettyPrint = false
    },
) : RssFeedStore {
    override suspend fun loadAll(): List<String> {
        val text = storage.readText() ?: return emptyList()
        return runCatching { json.decodeFromString(RssFeedList.serializer(), text).items }
            .getOrDefault(emptyList())
    }

    override suspend fun saveAll(items: List<String>) {
        storage.writeText(json.encodeToString(RssFeedList.serializer(), RssFeedList(items)))
    }

    @Serializable
    private data class RssFeedList(
        val items: List<String>,
    )
}
