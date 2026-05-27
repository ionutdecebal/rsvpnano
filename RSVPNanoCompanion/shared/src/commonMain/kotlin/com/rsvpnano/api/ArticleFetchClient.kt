package com.rsvpnano.api

import com.rsvpnano.converters.ArticleFormatter
import com.rsvpnano.converters.RsvpConverter
import com.rsvpnano.converters.SharedArticle
import io.ktor.client.HttpClient
import io.ktor.client.call.body
import io.ktor.client.request.get
import io.ktor.client.request.header
import io.ktor.http.HttpHeaders
import io.ktor.http.isSuccess
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class ArticleFetchError(message: String) : IllegalArgumentException(message) {
    companion object {
        val invalidUrl = ArticleFetchError("The article URL is not valid.")
        val articleTooLarge = ArticleFetchError("The page is too large to fetch safely.")
        val emptyArticle = ArticleFetchError("No readable article text was found.")
    }
}

class ArticleFetchClient(
    private val httpClient: HttpClient,
    private val maxFetchedBytes: Int = 900_000,
    private val maxTextCharacters: Int = 250_000,
) {
    suspend fun fetch(title: String, source: String): SharedArticle {
        val normalizedSource = source.trim()
        if (!normalizedSource.startsWith("http://") && !normalizedSource.startsWith("https://")) {
            throw ArticleFetchError.invalidUrl
        }

        val response = httpClient.get(normalizedSource) {
            header(HttpHeaders.UserAgent, "Mozilla/5.0 RSVP Nano Companion")
        }
        if (!response.status.isSuccess()) {
            throw NanoClientError("Article fetch failed with HTTP ${response.status.value}")
        }
        val contentLength = response.headers[HttpHeaders.ContentLength]?.toIntOrNull()
        if (contentLength != null && contentLength > maxFetchedBytes) {
            throw ArticleFetchError.articleTooLarge
        }

        val bytes = response.body<ByteArray>()
        if (bytes.size > maxFetchedBytes) {
            throw ArticleFetchError.articleTooLarge
        }

        val article = withContext(Dispatchers.Default) {
            val decoded = RsvpConverter.decodeText(bytes) ?: bytes.decodeToString()
            val clipped = decoded.take(maxTextCharacters)
            ArticleFormatter.article(title = title, source = normalizedSource, htmlOrText = clipped)
        }
        if (article.text.isBlank()) {
            throw ArticleFetchError.emptyArticle
        }
        return article
    }
}
