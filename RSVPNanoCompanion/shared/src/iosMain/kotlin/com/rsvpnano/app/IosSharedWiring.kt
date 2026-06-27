package com.rsvpnano.app

import com.rsvpnano.api.NanoClient
import com.rsvpnano.api.NanoKtorClient
import com.rsvpnano.api.ArticleFetchClient
import com.rsvpnano.persistence.JsonAppSettingsStore
import com.rsvpnano.persistence.OkioTextStorage
import io.ktor.client.HttpClient
import io.ktor.client.engine.darwin.Darwin
import io.ktor.client.plugins.contentnegotiation.ContentNegotiation
import io.ktor.serialization.kotlinx.json.json
import kotlinx.serialization.json.Json
import okio.Path
import okio.Path.Companion.toPath
import platform.Foundation.NSFileManager

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
    val root = appGroupRootPath(appGroupIdentifier)
    return RsvpSharedDependencies(
        pendingUploadStorage = OkioTextStorage(root.resolve("PendingUploads/drafts.json")),
        appSettingsStore = JsonAppSettingsStore(OkioTextStorage(root.resolve("Settings/companion_settings.json"))),
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

private fun appGroupRootPath(appGroupIdentifier: String): Path {
    val rootURL = NSFileManager.defaultManager()
        .containerURLForSecurityApplicationGroupIdentifier(appGroupIdentifier)
        ?: error("App group container is unavailable: $appGroupIdentifier")
    return requireNotNull(rootURL.path).toPath()
}
