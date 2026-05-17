package com.rsvpnano.api

import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoInfo
import io.ktor.client.HttpClient
import io.ktor.client.call.body
import io.ktor.client.request.forms.MultiPartFormDataContent
import io.ktor.client.request.forms.formData
import io.ktor.client.request.get
import io.ktor.client.request.post
import io.ktor.client.request.setBody
import io.ktor.http.ContentType
import io.ktor.http.HttpStatusCode
import io.ktor.http.URLBuilder
import io.ktor.http.appendPathSegments
import io.ktor.http.isSuccess
import kotlinx.serialization.json.Json
import kotlinx.serialization.Serializable

class NanoKtorClient(
    private val httpClient: HttpClient,
    private val json: Json = Json {
        ignoreUnknownKeys = true
        encodeDefaults = true
        explicitNulls = false
    },
) : NanoClient {
    override suspend fun fetchInfo(baseUrl: String): NanoInfo =
        json.decodeFromString(NanoInfo.serializer(), requestText(baseUrl, "api/info"))

    override suspend fun listBooks(baseUrl: String): List<NanoBook> {
        val response = requestText(baseUrl, "api/books")
        val wrapper = json.decodeFromString(BookListResponse.serializer(), response)
        return wrapper.books
    }

    override suspend fun uploadBook(baseUrl: String, name: String, data: ByteArray, category: String?): NanoBook {
        val response = httpClient.post(buildUrl(baseUrl, "api/books")) {
            setBody(
                MultiPartFormDataContent(
                    formData {
                        append("file", data, headers = io.ktor.http.Headers.build {
                            append("Content-Disposition", "filename=\"$name\"")
                            append("Content-Type", ContentType.Application.OctetStream.toString())
                        })
                        if (!category.isNullOrBlank()) {
                            append("category", category)
                        }
                        append("name", name)
                    }
                )
            )
        }

        ensureSuccess(response.status)
        val body = response.body<String>()
        return json.decodeFromString(NanoBook.serializer(), body)
    }

    private suspend fun requestText(baseUrl: String, path: String): String {
        val response = httpClient.get(buildUrl(baseUrl, path))
        ensureSuccess(response.status)
        return response.body<String>()
    }

    private fun buildUrl(baseUrl: String, path: String) = URLBuilder(baseUrl).apply {
        appendPathSegments(path.split('/').filter { it.isNotBlank() })
    }.build()

    private fun ensureSuccess(status: HttpStatusCode) {
        if (!status.isSuccess()) {
            throw NanoClientError("Device rejected request with HTTP $status")
        }
    }

    @Serializable
    private data class BookListResponse(
        val books: List<NanoBook>,
    )
}
