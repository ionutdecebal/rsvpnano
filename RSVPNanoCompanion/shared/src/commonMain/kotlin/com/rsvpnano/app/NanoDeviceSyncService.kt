package com.rsvpnano.app

import com.rsvpnano.api.NanoClient
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoUploadResponse
import com.rsvpnano.models.NanoWifiSettings

/**
 * Shared orchestration for the device-facing screen state.
 */
class NanoDeviceSyncService(
    private val client: NanoClient,
) {
    suspend fun connect(baseUrl: String): NanoDeviceSnapshot {
        val info = client.fetchInfo(baseUrl)
        val books = runCatching { client.listBooks(baseUrl) }.getOrDefault(emptyList())
        val settings = runCatching { client.fetchSettings(baseUrl) }.getOrNull()
        val wifiSettings = runCatching { client.fetchWifiSettings(baseUrl) }.getOrNull()
        val rssFeeds = runCatching { client.fetchRssFeeds(baseUrl) }.getOrNull()
        return NanoDeviceSnapshot(
            info = info,
            books = books,
            settings = settings,
            wifiSettings = wifiSettings,
            rssFeeds = rssFeeds,
        )
    }

    suspend fun verifyReachable(baseUrl: String) {
        client.fetchInfo(baseUrl)
    }

    suspend fun refreshBooks(baseUrl: String): List<NanoBook> = client.listBooks(baseUrl)

    suspend fun refreshSettings(baseUrl: String): NanoSettings = client.fetchSettings(baseUrl)

    suspend fun refreshWifiSettings(baseUrl: String): NanoWifiSettings = client.fetchWifiSettings(baseUrl)

    suspend fun refreshRssFeeds(baseUrl: String) = client.fetchRssFeeds(baseUrl)

    suspend fun saveRssFeeds(baseUrl: String, feeds: List<String>): NanoRssFeeds = client.updateRssFeeds(baseUrl, feeds)

    suspend fun saveSettings(baseUrl: String, settings: NanoSettings): NanoSettings = client.updateSettings(baseUrl, settings)

    suspend fun saveWifiSettings(baseUrl: String, ssid: String, password: String): NanoWifiSettings =
        client.updateWifi(baseUrl, ssid, password)

    suspend fun clearWifiSettings(baseUrl: String): NanoWifiSettings = client.forgetWifi(baseUrl)

    suspend fun uploadBook(
        baseUrl: String,
        filename: String,
        data: ByteArray,
        category: String? = null,
        onProgress: ((sent: Long, total: Long) -> Unit)? = null,
    ): NanoUploadResponse =
        client.uploadBook(baseUrl = baseUrl, name = filename, data = data, category = category, onProgress = onProgress)

    suspend fun deleteBook(baseUrl: String, id: String): NanoUploadResponse = client.deleteBook(baseUrl, id)

    suspend fun setBookPosition(
        baseUrl: String,
        id: String,
        sourceSize: Long,
        sourceFingerprint: Long,
        wordCount: Int,
        wordIndex: Int,
    ): NanoUploadResponse =
        client.setBookPosition(
            baseUrl = baseUrl,
            id = id,
            sourceSize = sourceSize,
            sourceFingerprint = sourceFingerprint,
            wordCount = wordCount,
            wordIndex = wordIndex,
        )
}
