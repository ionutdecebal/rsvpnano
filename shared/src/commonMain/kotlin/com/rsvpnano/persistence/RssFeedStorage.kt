package com.rsvpnano.persistence

/**
 * Low-level storage for the RSS feed list.
 */
interface RssFeedStorage {
    suspend fun readText(): String?
    suspend fun writeText(value: String)
}
