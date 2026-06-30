package com.rsvpnano.api

import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoUploadResponse
import com.rsvpnano.models.NanoWifiSettings

/**
 * Lightweight API client interface for device interactions. Implement with Ktor in commonMain
 * or provide a platform-backed implementation if preferred.
 */
interface NanoClient {
    suspend fun fetchInfo(baseUrl: String): NanoInfo
    suspend fun listBooks(baseUrl: String): List<NanoBook>
    suspend fun fetchSettings(baseUrl: String): NanoSettings
    suspend fun updateSettings(baseUrl: String, settings: NanoSettings): NanoSettings
    suspend fun fetchWifiSettings(baseUrl: String): NanoWifiSettings
    suspend fun updateWifi(baseUrl: String, ssid: String, password: String): NanoWifiSettings
    suspend fun forgetWifi(baseUrl: String): NanoWifiSettings
    suspend fun fetchRssFeeds(baseUrl: String): NanoRssFeeds
    suspend fun updateRssFeeds(baseUrl: String, feeds: List<String>): NanoRssFeeds
    suspend fun uploadBook(
        baseUrl: String,
        name: String,
        data: ByteArray,
        category: String? = null,
        onProgress: ((sent: Long, total: Long) -> Unit)? = null,
    ): NanoUploadResponse
    suspend fun deleteBook(baseUrl: String, id: String): NanoUploadResponse
    suspend fun setBookPosition(
        baseUrl: String,
        id: String,
        sourceSize: Long,
        sourceFingerprint: Long,
        wordCount: Int,
        wordIndex: Int,
    ): NanoUploadResponse
}
