package com.rsvpnano

import com.rsvpnano.api.ArticleFetchClient
import com.rsvpnano.api.ArticleFetchError
import io.ktor.client.HttpClient
import io.ktor.client.engine.mock.MockEngine
import io.ktor.client.engine.mock.respond
import io.ktor.http.ContentType
import io.ktor.http.HttpHeaders
import io.ktor.http.HttpStatusCode
import io.ktor.http.headersOf
import kotlinx.coroutines.runBlocking
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class ArticleFetchClientAndroidTest {
    @Test
    fun fetchFormatsReadableArticleFromHtml() = runBlocking {
        val client = ArticleFetchClient(
            httpClient = mockClient(
                """
                <html>
                  <head><title>Fetched Title</title></head>
                  <body><nav>skip</nav><article><p>Hello reader.</p></article></body>
                </html>
                """.trimIndent(),
            )
        )

        val article = client.fetch(title = "https://example.com/story", source = "https://example.com/story")

        assertEquals("Fetched Title", article.title)
        assertEquals("https://example.com/story", article.source)
        assertEquals("Hello reader.", article.text)
    }

    @Test
    fun rejectsOversizedContentLengthBeforeReadingBody() = runBlocking {
        val client = ArticleFetchClient(
            httpClient = mockClient(
                body = "0123456789",
                headers = headersOf(
                    HttpHeaders.ContentType to listOf(ContentType.Text.Html.toString()),
                    HttpHeaders.ContentLength to listOf("10"),
                ),
            ),
            maxFetchedBytes = 4,
        )

        assertFailsWith<ArticleFetchError> {
            client.fetch(title = "Story", source = "https://example.com/story")
        }
        Unit
    }

    @Test
    fun rejectsNonHttpSources() = runBlocking {
        val client = ArticleFetchClient(mockClient("unused"))

        assertFailsWith<ArticleFetchError> {
            client.fetch(title = "Story", source = "mailto:person@example.com")
        }
        Unit
    }

    private fun mockClient(
        body: String,
        headers: io.ktor.http.Headers = headersOf(HttpHeaders.ContentType, ContentType.Text.Html.toString()),
    ): HttpClient {
        return HttpClient(MockEngine) {
            engine {
                addHandler {
                    respond(
                        content = body,
                        status = HttpStatusCode.OK,
                        headers = headers,
                    )
                }
            }
        }
    }
}
