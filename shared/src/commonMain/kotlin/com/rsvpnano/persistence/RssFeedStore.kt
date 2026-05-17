package com.rsvpnano.persistence

/**
 * Platform-agnostic persistence for RSS feeds.
 *
 * This stays intentionally small so the platform adapters only need to bridge storage.
 */
interface RssFeedStore {
    suspend fun loadAll(): List<String>
    suspend fun saveAll(items: List<String>)
}
