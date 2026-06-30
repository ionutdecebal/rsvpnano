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
                "/api/books" -> """{"books":[{"id":"b12345678","name":"books/Book.rsvp","title":"Book","category":"book","sourceSize":1234,"sourceFingerprint":3456,"wordCount":1000,"wordIndex":249,"progressPercent":24,"chapters":[{"title":"Chapter 1","wordIndex":0}]}]}"""
                "/api/rss-feeds" -> """{"ok":true,"feeds":["https://example.com/feed"]}"""
                else -> error("Unexpected request: ${request.url}")
            }
        })

        assertEquals("Nano", client.fetchInfo("http://device.local").name)
        val book = client.listBooks("http://device.local").single()
        assertEquals("b12345678", book.id)
        assertEquals("books/Book.rsvp", book.name)
        assertEquals("Book", book.title)
        assertEquals(1000, book.wordCount)
        assertEquals(249, book.wordIndex)
        assertEquals("Chapter 1", book.chapters.single().title)
        assertEquals(listOf("https://example.com/feed"), client.fetchRssFeeds("http://device.local").feeds)
        assertEquals(listOf("GET /api/info", "GET /api/books", "GET /api/rss-feeds"), seen)
    }

    @Test
    fun bookMutationsUseDeviceContract() = runBlocking {
        val seen = mutableListOf<String>()
        val client = NanoKtorClient(mockHttpClient { request ->
            seen += "${request.method.value} ${request.url.encodedPath}?${request.url.encodedQuery}"
            when (request.method) {
                HttpMethod.Post -> {
                    assertEquals("Story.rsvp", request.url.parameters["name"])
                    assertEquals("article", request.url.parameters["category"])
                    """{"ok":true,"path":"/books/articles/Story.rsvp"}"""
                }
                HttpMethod.Delete -> {
                    assertEquals("b12345678", request.url.parameters["id"])
                    """{"ok":true}"""
                }
                HttpMethod.Patch -> """{"ok":true,"id":"b12345678","wordIndex":250}"""
                else -> error("Unexpected method: ${request.method}")
            }
        })

        val upload = client.uploadBook(
            baseUrl = "http://device.local",
            name = "Story.rsvp",
            data = byteArrayOf(1, 2, 3),
            category = "article",
        )
        val delete = client.deleteBook("http://device.local", "b12345678")
        val position = client.setBookPosition(
            baseUrl = "http://device.local",
            id = "b12345678",
            sourceSize = 1234,
            sourceFingerprint = 3456,
            wordCount = 1000,
            wordIndex = 250,
        )

        assertEquals("/books/articles/Story.rsvp", upload.path)
        assertEquals(true, delete.ok)
        assertEquals(true, position.ok)
        assertEquals(
            listOf(
                "POST /api/books?name=Story.rsvp&category=article",
                "DELETE /api/books?id=b12345678",
                "PATCH /api/books/position?",
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
