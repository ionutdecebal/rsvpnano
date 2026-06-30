package com.rsvpnano

import com.rsvpnano.api.NanoClient
import com.rsvpnano.app.NanoDeviceSyncService
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoUploadResponse
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoWifiSettings
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertContentEquals

class NanoDeviceSyncServiceTest {
    @Test
    fun connectBuildsSnapshotFromClientCalls() = kotlinx.coroutines.runBlocking {
        val service = NanoDeviceSyncService(FakeClient())
        val snapshot = service.connect("http://device.local")

        assertEquals("Nano", snapshot.info?.name)
        assertEquals(1, snapshot.books.size)
        assertEquals(true, snapshot.rssFeeds?.ok)
    }

    @Test
    fun deviceMutationsDelegateToClient() = kotlinx.coroutines.runBlocking {
        val client = FakeClient()
        val service = NanoDeviceSyncService(client)

        val feeds = service.saveRssFeeds("http://device.local", listOf("https://example.com/feed"))
        var uploadProgress: Pair<Long, Long>? = null
        val upload = service.uploadBook(
            baseUrl = "http://device.local",
            filename = "Story.rsvp",
            data = byteArrayOf(1, 2, 3),
            category = "article",
            onProgress = { sent, total -> uploadProgress = sent to total },
        )
        val delete = service.deleteBook("http://device.local", "b12345678")
        val position = service.setBookPosition(
            baseUrl = "http://device.local",
            id = "b12345678",
            sourceSize = 1234,
            sourceFingerprint = 3456,
            wordCount = 1000,
            wordIndex = 250,
        )

        assertEquals(listOf("https://example.com/feed"), feeds.feeds)
        assertEquals("/books/Story.rsvp", upload.path)
        assertEquals(true, delete.ok)
        assertEquals(true, position.ok)
        assertEquals("Story.rsvp", client.uploadedName)
        assertEquals("article", client.uploadedCategory)
        assertContentEquals(byteArrayOf(1, 2, 3), client.uploadedData)
        assertEquals(3L to 3L, uploadProgress)
        assertEquals("b12345678", client.deletedId)
        assertEquals(250, client.savedWordIndex)
    }

    private class FakeClient : NanoClient {
        var uploadedName: String? = null
        var uploadedCategory: String? = null
        var uploadedData: ByteArray? = null
        var deletedId: String? = null
        var savedWordIndex: Int? = null

        override suspend fun fetchInfo(baseUrl: String): NanoInfo = NanoInfo(name = "Nano")
        override suspend fun listBooks(baseUrl: String): List<NanoBook> = listOf(NanoBook(id = "1", title = "Book"))
        override suspend fun fetchSettings(baseUrl: String): NanoSettings = sampleSettings()
        override suspend fun updateSettings(baseUrl: String, settings: NanoSettings): NanoSettings = settings
        override suspend fun fetchWifiSettings(baseUrl: String): NanoWifiSettings = NanoWifiSettings(ok = true, configured = true, ssid = "RSSP", passwordSet = false)
        override suspend fun updateWifi(baseUrl: String, ssid: String, password: String): NanoWifiSettings = NanoWifiSettings(ok = true, configured = true, ssid = ssid, passwordSet = true)
        override suspend fun forgetWifi(baseUrl: String): NanoWifiSettings = NanoWifiSettings(ok = true, configured = false, ssid = "", passwordSet = false)
        override suspend fun fetchRssFeeds(baseUrl: String): NanoRssFeeds = NanoRssFeeds(ok = true, feeds = listOf("https://example.com/feed"))
        override suspend fun updateRssFeeds(baseUrl: String, feeds: List<String>): NanoRssFeeds = NanoRssFeeds(ok = true, feeds = feeds)
        override suspend fun uploadBook(
            baseUrl: String,
            name: String,
            data: ByteArray,
            category: String?,
            onProgress: ((sent: Long, total: Long) -> Unit)?,
        ): NanoUploadResponse {
            onProgress?.invoke(data.size.toLong(), data.size.toLong())
            uploadedName = name
            uploadedCategory = category
            uploadedData = data
            return NanoUploadResponse(ok = true, path = "/books/$name")
        }

        override suspend fun deleteBook(baseUrl: String, id: String): NanoUploadResponse {
            deletedId = id
            return NanoUploadResponse(ok = true)
        }

        override suspend fun setBookPosition(
            baseUrl: String,
            id: String,
            sourceSize: Long,
            sourceFingerprint: Long,
            wordCount: Int,
            wordIndex: Int,
        ): NanoUploadResponse {
            savedWordIndex = wordIndex
            return NanoUploadResponse(ok = true)
        }
    }
}
