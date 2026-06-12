package com.rsvpnano

import com.rsvpnano.api.NanoKtorClient
import io.ktor.client.HttpClient
import io.ktor.client.engine.mock.MockEngine
import io.ktor.client.engine.mock.respond
import io.ktor.client.plugins.contentnegotiation.ContentNegotiation
import io.ktor.http.ContentType
import io.ktor.http.HttpHeaders
import io.ktor.http.HttpMethod
import io.ktor.http.HttpStatusCode
import io.ktor.http.headersOf
import io.ktor.serialization.kotlinx.json.json
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.Json
import kotlin.test.Test
import kotlin.test.assertEquals

class NanoKtorClientAndroidTest {
    @Test
    fun fetchesDeviceSnapshotEndpoints() = runBlocking {
        val seen = mutableListOf<String>()
        val client = NanoKtorClient(mockHttpClient { request ->
            seen += "${request.method.value} ${request.url.encodedPath}"
            when (request.url.encodedPath) {
                "/api/info" -> """{"name":"Nano"}"""
                "/api/books" -> """{"books":[{"name":"books/Book.rsvp","title":"Book","category":"book"}]}"""
                "/api/rss-feeds" -> """{"ok":true,"feeds":["https://example.com/feed"]}"""
                else -> error("Unexpected request: ${request.url}")
            }
        })

        assertEquals("Nano", client.fetchInfo("http://device.local").name)
        val book = client.listBooks("http://device.local").single()
        assertEquals("books/Book.rsvp", book.id)
        assertEquals("Book", book.title)
        assertEquals(listOf("https://example.com/feed"), client.fetchRssFeeds("http://device.local").feeds)
        assertEquals(listOf("GET /api/info", "GET /api/books", "GET /api/rss-feeds"), seen)
    }

    @Test
    fun uploadAndDeleteUseDeviceQueryContract() = runBlocking {
        val seen = mutableListOf<String>()
        val client = NanoKtorClient(mockHttpClient { request ->
            seen += "${request.method.value} ${request.url.encodedPath}?${request.url.encodedQuery}"
            assertEquals("Story.rsvp", request.url.parameters["name"])
            when (request.method) {
                HttpMethod.Post -> {
                    assertEquals("article", request.url.parameters["category"])
                    """{"ok":true,"path":"/books/articles/Story.rsvp"}"""
                }
                HttpMethod.Delete -> """{"ok":true}"""
                else -> error("Unexpected method: ${request.method}")
            }
        })

        val upload = client.uploadBook(
            baseUrl = "http://device.local",
            name = "Story.rsvp",
            data = byteArrayOf(1, 2, 3),
            category = "article",
        )
        val delete = client.deleteBook("http://device.local", "Story.rsvp")

        assertEquals("/books/articles/Story.rsvp", upload.path)
        assertEquals(true, delete.ok)
        assertEquals(
            listOf(
                "POST /api/books?name=Story.rsvp&category=article",
                "DELETE /api/books?name=Story.rsvp",
            ),
            seen,
        )
    }

    private fun mockHttpClient(handler: (io.ktor.client.request.HttpRequestData) -> String): HttpClient {
        return HttpClient(MockEngine) {
            engine {
                addHandler { request ->
                    respond(
                        content = handler(request),
                        status = HttpStatusCode.OK,
                        headers = headersOf(HttpHeaders.ContentType, ContentType.Application.Json.toString()),
                    )
                }
            }
            install(ContentNegotiation) {
                json(
                    Json {
                        ignoreUnknownKeys = true
                        encodeDefaults = true
                        explicitNulls = false
                    }
                )
            }
        }
    }
}
