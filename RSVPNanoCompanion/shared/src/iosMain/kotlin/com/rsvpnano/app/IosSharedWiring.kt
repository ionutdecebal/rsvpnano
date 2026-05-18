package com.rsvpnano.app

import com.rsvpnano.api.NanoClient
import com.rsvpnano.api.NanoKtorClient
import com.rsvpnano.api.ArticleFetchClient
import com.rsvpnano.persistence.FilePendingUploadStorage
import com.rsvpnano.persistence.FileRssFeedStorage
import io.ktor.client.HttpClient
import io.ktor.client.engine.darwin.Darwin
import io.ktor.client.plugins.contentnegotiation.ContentNegotiation
import io.ktor.serialization.kotlinx.json.json
import kotlinx.serialization.json.Json

private const val DefaultAppGroupIdentifier = "group.com.rsvpnano.companion"

/**
 * Creates shared dependencies for the iOS app and share extension.
 *
 * Keeping this wiring in `iosMain` avoids duplicating storage setup in Swift adapters.
 */
fun createIosSharedDependencies(
    appGroupIdentifier: String = DefaultAppGroupIdentifier,
): RsvpSharedDependencies {
    val httpClient = createIosHttpClient()
    val nanoClient = NanoKtorClient(httpClient = httpClient)
    return RsvpSharedDependencies(
        pendingUploadStorage = FilePendingUploadStorage(appGroupIdentifier = appGroupIdentifier),
        rssFeedStorage = FileRssFeedStorage(appGroupIdentifier = appGroupIdentifier),
        articleFetchClient = ArticleFetchClient(httpClient = httpClient),
        nanoClient = nanoClient,
    )
}

fun createIosSharedApp(
    appGroupIdentifier: String = DefaultAppGroupIdentifier,
): RsvpSharedApp {
    return createIosSharedDependencies(appGroupIdentifier).createApp()
}

fun createIosNanoClient(): NanoClient {
    return NanoKtorClient(httpClient = createIosHttpClient())
}

fun createIosDeviceSyncService(): NanoDeviceSyncService {
    return NanoDeviceSyncService(createIosNanoClient())
}

fun createIosCompanionController(
    appGroupIdentifier: String = DefaultAppGroupIdentifier,
): NanoCompanionController {
    return createIosSharedDependencies(appGroupIdentifier).createCompanionController()
}

fun createIosArticleFetchClient(): ArticleFetchClient {
    return ArticleFetchClient(createIosHttpClient())
}

private fun createIosHttpClient(): HttpClient {
    return HttpClient(Darwin) {
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
