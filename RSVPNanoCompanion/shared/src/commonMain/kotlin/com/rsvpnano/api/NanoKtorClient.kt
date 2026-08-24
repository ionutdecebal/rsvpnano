package com.rsvpnano.api

import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoFontSummary
import com.rsvpnano.models.NanoFocusTimers
import com.rsvpnano.models.NanoFocusTimer
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoLocaleSummary
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoStorageRepair
import com.rsvpnano.models.NanoThemeSummary
import com.rsvpnano.models.NanoThemeCatalogItem
import com.rsvpnano.models.NanoFontCatalogItem
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.NanoWifiUpdate
import com.rsvpnano.models.FirmwareRelease
import com.rsvpnano.models.NanoLocaleCatalogItem
import com.rsvpnano.models.NanoLanguageFont
import io.ktor.client.HttpClient
import io.ktor.client.call.body
import io.ktor.client.plugins.onUpload
import io.ktor.client.plugins.onDownload
import io.ktor.client.request.delete
import io.ktor.client.request.get
import io.ktor.client.request.patch
import io.ktor.client.request.post
import io.ktor.client.request.put
import io.ktor.client.request.setBody
import io.ktor.http.ContentType
import io.ktor.http.HttpHeaders
import io.ktor.http.HttpStatusCode
import io.ktor.http.URLBuilder
import io.ktor.http.appendPathSegments
import io.ktor.http.contentType
import io.ktor.http.isSuccess
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.builtins.serializer
import kotlinx.serialization.KSerializer
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.decodeFromJsonElement
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.Serializable
import kotlinx.serialization.SerialName

class NanoKtorClient(
    private val httpClient: HttpClient,
    private val json: Json = Json {
        ignoreUnknownKeys = true
        encodeDefaults = true
        explicitNulls = false
    },
) : NanoApi, RepositoryClient {
    override fun close() = httpClient.close()

    override suspend fun fetchFirmwareRelease(owner: String, repository: String, tag: String): FirmwareRelease {
        val url = URLBuilder("https://api.github.com").apply {
            appendPathSegments("repos", owner, repository, "releases")
            if (tag.isBlank()) {
                appendPathSegments("latest")
            } else {
                appendPathSegments("tags", tag)
            }
        }.build()
        val response = httpClient.get(url) {
            headers.append(HttpHeaders.Accept, "application/vnd.github+json")
            headers.append(HttpHeaders.UserAgent, "RSVP-Nano-Companion")
        }
        if (!response.status.isSuccess()) {
            throw NanoClientError("Firmware release lookup returned HTTP ${response.status}")
        }
        val release = json.decodeFromString(GithubRelease.serializer(), response.body<String>())
        val commit = release.targetCommitish.trim()
        if (commit.length != 40 || !commit.all { it.digitToIntOrNull(16) != null }) {
            throw NanoClientError("Firmware release target is not a commit SHA.")
        }
        return FirmwareRelease(
            version = "${release.tagName}+${commit.take(12)}",
            assets = release.assets.map(GithubAsset::name),
        )
    }

    override suspend fun fetchDevice(baseUrl: String): NanoInfo =
        requestData(baseUrl, "api/v2/device", NanoInfo.serializer())

    override suspend fun repairStorage(baseUrl: String): NanoStorageRepair {
        val response = httpClient.post(buildUrl(baseUrl, "api/v2/storage/repair"))
        return decodeDeviceResponse(response.status, response.body<String>(), NanoStorageRepair.serializer())
    }

    override suspend fun listLibrary(baseUrl: String): List<NanoBook> {
        return requestData(baseUrl, "api/v2/library", ListSerializer(NanoBook.serializer()))
    }

    override suspend fun listThemes(baseUrl: String): List<NanoThemeSummary> =
        requestData(baseUrl, "api/v2/themes", ListSerializer(NanoThemeSummary.serializer()))

    override suspend fun listFonts(baseUrl: String): List<NanoFontSummary> =
        requestData(baseUrl, "api/v2/fonts", ListSerializer(NanoFontSummary.serializer()))

    override suspend fun listLocales(baseUrl: String): List<NanoLocaleSummary> =
        requestData(baseUrl, "api/v2/locales", ListSerializer(NanoLocaleSummary.serializer()))

    override suspend fun fetchSettings(baseUrl: String): NanoSettings =
        requestData(baseUrl, "api/v2/settings", NanoSettings.serializer())

    override suspend fun fetchLocaleCatalog(url: String): List<NanoLocaleCatalogItem> {
        val response = httpClient.get(url)
        if (!response.status.isSuccess()) {
            throw NanoClientError("Locale-pack catalog returned HTTP ${response.status}")
        }
        return decodeCatalog(response.body(), NanoLocaleCatalogItem.serializer())
    }

    override suspend fun downloadLocalePack(
        url: String,
        onProgress: ((received: Long, total: Long?) -> Unit)?,
    ): ByteArray {
        val response = httpClient.get(url) {
            onProgress?.let { progress -> onDownload(progress) }
        }
        if (!response.status.isSuccess()) {
            throw NanoClientError("Locale-pack download returned HTTP ${response.status}")
        }
        return response.body()
    }

    override suspend fun updateReadingSettings(
        baseUrl: String,
        settings: NanoSettings.Reading,
    ) = updateSettingsResource(baseUrl, "reading", settings, NanoSettings.Reading.serializer())

    override suspend fun updateDisplaySettings(
        baseUrl: String,
        settings: NanoSettings.Interface,
    ) = updateSettingsResource(baseUrl, "display", settings, NanoSettings.Interface.serializer())

    override suspend fun updateUpdateSettings(
        baseUrl: String,
        settings: NanoSettings.Updates,
    ) = updateSettingsResource(baseUrl, "updates", settings, NanoSettings.Updates.serializer())

    private suspend fun <T> updateSettingsResource(
        baseUrl: String,
        resource: String,
        settings: T,
        serializer: kotlinx.serialization.KSerializer<T>,
    ) {
        val response = httpClient.patch(buildUrl(baseUrl, "api/v2/settings/$resource")) {
            contentType(ContentType.Application.Json)
            setBody(json.encodeToString(serializer, settings))
        }
        requireNoContent(response.status, response.body())
    }

    override suspend fun selectTheme(baseUrl: String, id: String) =
        selectAppearance(baseUrl, "theme", id)

    override suspend fun selectFont(baseUrl: String, id: String) =
        selectAppearance(baseUrl, "font", id)

    override suspend fun selectLocale(baseUrl: String, id: String) =
        selectAppearance(baseUrl, "locale", id)

    override suspend fun fetchWifiSettings(baseUrl: String): NanoWifiSettings =
        requestData(baseUrl, "api/v2/network", NanoWifiSettings.serializer())

    override suspend fun updateWifi(baseUrl: String, ssid: String, password: String) {
        val response = httpClient.put(buildUrl(baseUrl, "api/v2/network")) {
            contentType(ContentType.Application.Json)
            setBody(NanoWifiUpdate(ssid = ssid, password = password))
        }
        requireNoContent(response.status, response.body())
    }

    override suspend fun forgetWifi(baseUrl: String) {
        val response = httpClient.delete(buildUrl(baseUrl, "api/v2/network"))
        requireNoContent(response.status, response.body())
    }

    override suspend fun fetchRssFeeds(baseUrl: String): NanoRssFeeds =
        NanoRssFeeds(requestData(baseUrl, "api/v2/feeds", ListSerializer(String.serializer())))

    override suspend fun updateRssFeeds(baseUrl: String, config: NanoRssFeeds) {
        val response = httpClient.put(buildUrl(baseUrl, "api/v2/feeds")) {
            contentType(ContentType.Application.Json)
            setBody(config)
        }
        requireNoContent(response.status, response.body())
    }

    override suspend fun fetchFocusTimers(baseUrl: String): NanoFocusTimers =
        NanoFocusTimers(requestData(baseUrl, "api/v2/focus-timers", ListSerializer(NanoFocusTimer.serializer())))

    override suspend fun updateFocusTimers(baseUrl: String, timers: NanoFocusTimers) {
        val response = httpClient.put(buildUrl(baseUrl, "api/v2/focus-timers")) {
            contentType(ContentType.Application.Json)
            setBody(timers)
        }
        requireNoContent(response.status, response.body())
    }

    override suspend fun uploadBook(
        baseUrl: String,
        name: String,
        data: ByteArray,
        category: String?,
        onProgress: ((sent: Long, total: Long) -> Unit)?,
    ): NanoBook {
        val response = httpClient.post(
            buildUrl(
                baseUrl = baseUrl,
                path = "api/v2/library",
                query = listOfNotNull("name" to name, category?.let { "category" to it }),
            )
        ) {
            contentType(ContentType.Application.OctetStream)
            setBody(data)
            onProgress?.let { progress ->
                onUpload { sent, total ->
                    progress(sent, total ?: data.size.toLong())
                }
            }
        }

        val body = response.body<String>()
        return decodeDeviceResponse(response.status, body, NanoBook.serializer())
    }

    override suspend fun uploadTheme(
        baseUrl: String,
        name: String,
        data: ByteArray,
        onProgress: ((sent: Long, total: Long) -> Unit)?,
    ): NanoThemeSummary {
        val response = httpClient.post(buildUrl(baseUrl, "api/v2/themes", listOf("name" to name))) {
            contentType(ContentType.Application.OctetStream)
            setBody(data)
            onProgress?.let { progress ->
                onUpload { sent, total ->
                    progress(sent, total ?: data.size.toLong())
                }
            }
        }

        val body = response.body<String>()
        return decodeDeviceResponse(response.status, body, NanoThemeSummary.serializer())
    }

    override suspend fun uploadFont(
        baseUrl: String,
        name: String,
        data: ByteArray,
        onProgress: ((sent: Long, total: Long) -> Unit)?,
    ): NanoFontSummary {
        val response = httpClient.post(buildUrl(baseUrl, "api/v2/fonts")) {
            contentType(ContentType.Application.OctetStream)
            setBody(data)
            onProgress?.let { progress ->
                onUpload { sent, total ->
                    progress(sent, total ?: data.size.toLong())
                }
            }
        }

        val body = response.body<String>()
        return decodeDeviceResponse(response.status, body, NanoFontSummary.serializer())
    }

    override suspend fun fetchThemeCatalog(url: String): List<NanoThemeCatalogItem> {
        val response = httpClient.get(url)
        if (!response.status.isSuccess()) {
            throw NanoClientError("Theme catalog returned HTTP ${response.status}")
        }
        return decodeCatalog(response.body(), NanoThemeCatalogItem.serializer())
    }

    override suspend fun downloadTheme(
        url: String,
        onProgress: ((received: Long, total: Long?) -> Unit)?,
    ): ByteArray {
        val response = httpClient.get(url) {
            onProgress?.let { progress -> onDownload(progress) }
        }
        if (!response.status.isSuccess()) {
            throw NanoClientError("Theme download returned HTTP ${response.status}")
        }
        return response.body()
    }

    override suspend fun fetchFontCatalog(url: String): List<NanoFontCatalogItem> {
        val response = httpClient.get(url)
        if (!response.status.isSuccess()) {
            throw NanoClientError("Font catalog returned HTTP ${response.status}")
        }
        return decodeCatalog(response.body(), NanoFontCatalogItem.serializer())
    }

    override suspend fun downloadFont(
        url: String,
        onProgress: ((received: Long, total: Long?) -> Unit)?,
    ): ByteArray {
        val response = httpClient.get(url) {
            onProgress?.let { progress -> onDownload(progress) }
        }
        if (!response.status.isSuccess()) {
            throw NanoClientError("Font download returned HTTP ${response.status}")
        }
        return response.body()
    }

    override suspend fun uploadLocalePack(
        baseUrl: String,
        name: String,
        data: ByteArray,
        onProgress: ((sent: Long, total: Long) -> Unit)?,
    ): NanoLocaleSummary {
        val response = httpClient.post(buildUrl(baseUrl, "api/v2/locales")) {
            contentType(ContentType.Application.OctetStream)
            setBody(data)
            onProgress?.let { progress ->
                onUpload { sent, total -> progress(sent, total ?: data.size.toLong()) }
            }
        }
        return decodeDeviceResponse(response.status, response.body<String>(), NanoLocaleSummary.serializer())
    }

    override suspend fun deleteTheme(baseUrl: String, id: String) = deleteResource(baseUrl, "themes", id)

    override suspend fun deleteFont(baseUrl: String, id: String) = deleteResource(baseUrl, "fonts", id)

    override suspend fun deleteLocalePack(baseUrl: String, id: String) = deleteResource(baseUrl, "locales", id)

    private suspend fun deleteResource(baseUrl: String, collection: String, id: String) {
        val response = httpClient.delete(buildUrl(baseUrl, "api/v2/$collection/$id"))
        requireNoContent(response.status, response.body())
    }

    private fun requireNoContent(status: HttpStatusCode, body: String) {
        if (!status.isSuccess())
            throwDeviceError(status, body)
        if (status != HttpStatusCode.NoContent || body.isNotEmpty())
            throw NanoClientError("Device returned an invalid empty response.")
    }

    override suspend fun deleteBook(baseUrl: String, id: String) {
        val response = httpClient.delete(buildUrl(baseUrl, "api/v2/library/$id"))
        requireNoContent(response.status, response.body())
    }

    override suspend fun setBookPosition(
        baseUrl: String,
        id: String,
        wordIndex: Int,
    ) {
        val response = httpClient.put(buildUrl(baseUrl, "api/v2/library/$id/position")) {
            contentType(ContentType.Application.Json)
            setBody(
                BookPositionUpdate(
                    wordIndex = wordIndex,
                )
            )
        }
        requireNoContent(response.status, response.body())
    }

    override suspend fun setBookLanguageFonts(
        baseUrl: String,
        id: String,
        languageFonts: List<NanoLanguageFont>,
    ) {
        val response = httpClient.put(buildUrl(baseUrl, "api/v2/library/$id/language-fonts")) {
            contentType(ContentType.Application.Json)
            setBody(BookLanguageFontsUpdate(languageFonts))
        }
        requireNoContent(response.status, response.body())
    }

    private suspend fun <T> requestData(
        baseUrl: String,
        path: String,
        serializer: kotlinx.serialization.KSerializer<T>,
    ): T {
        val response = httpClient.get(buildUrl(baseUrl, path))
        return decodeDeviceResponse(response.status, response.body<String>(), serializer)
    }

    private suspend fun selectAppearance(baseUrl: String, resource: String, id: String) {
        val response = httpClient.put(buildUrl(baseUrl, "api/v2/appearance/$resource")) {
            contentType(ContentType.Application.Json)
            setBody(AppearanceSelection(id))
        }
        requireNoContent(response.status, response.body())
    }

    private fun buildUrl(baseUrl: String, path: String, query: List<Pair<String, String>> = emptyList()) = URLBuilder(baseUrl).apply {
        appendPathSegments(path.split('/').filter { it.isNotBlank() })
        query.forEach { (name, value) -> parameters.append(name, value) }
    }.build()

    private fun <T> decodeDeviceResponse(
        status: HttpStatusCode,
        body: String,
        serializer: kotlinx.serialization.KSerializer<T>,
    ): T {
        if (!status.isSuccess())
            throwDeviceError(status, body)
        return runCatching { json.decodeFromString(serializer, body) }
            .getOrElse { cause ->
                throw NanoClientError(
                    message = "Device returned an invalid API response.",
                    status = status.value,
                    cause = cause,
                )
            }
    }

    private fun <T> decodeCatalog(body: String, serializer: KSerializer<T>): List<T> =
        json.parseToJsonElement(body).jsonArray.mapNotNull { item ->
            runCatching { json.decodeFromJsonElement(serializer, item) }.getOrNull()
        }

    private fun throwDeviceError(status: HttpStatusCode, body: String): Nothing {
        val error = runCatching { json.decodeFromString(DeviceError.serializer(), body) }.getOrNull()
        throw NanoClientError(
            message = error?.message?.takeIf(String::isNotBlank)
                ?: body.takeIf(String::isNotBlank)
                ?: "Device rejected request with HTTP $status",
            status = status.value,
            code = error?.code,
            field = error?.field,
        )
    }

    @Serializable
    private data class DeviceError(
        val code: String,
        val message: String,
        val field: String? = null,
    )

    @Serializable
    private data class AppearanceSelection(val id: String)

    @Serializable
    private data class BookPositionUpdate(
        val wordIndex: Int,
    )

    @Serializable
    private data class BookLanguageFontsUpdate(
        val languageFonts: List<NanoLanguageFont>,
    )

    @Serializable
    private data class GithubRelease(
        @SerialName("tag_name") val tagName: String,
        @SerialName("target_commitish") val targetCommitish: String,
        val assets: List<GithubAsset> = emptyList(),
    )

    @Serializable
    private data class GithubAsset(val name: String)
}
