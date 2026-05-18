package com.rsvpnano.app

import com.rsvpnano.api.NanoClient
import com.rsvpnano.api.NanoKtorClient
import com.rsvpnano.api.ArticleFetchClient
import com.rsvpnano.persistence.FilePendingUploadStorage
import com.rsvpnano.persistence.FileRssFeedStorage
import io.ktor.client.HttpClient
import io.ktor.client.engine.okhttp.OkHttp
import io.ktor.client.plugins.contentnegotiation.ContentNegotiation
import io.ktor.serialization.kotlinx.json.json
import java.io.File
import kotlinx.serialization.json.Json

private const val PendingUploadRelativePath = "pending-uploads/drafts.json"
private const val RssFeedsRelativePath = "rss/feeds.json"

/**
 * Creates shared dependencies for Android using app-private storage paths.
 */
fun createAndroidSharedDependencies(
    appFilesDir: File,
): RsvpSharedDependencies {
    val httpClient = createAndroidHttpClient()
    val nanoClient = NanoKtorClient(httpClient = httpClient)
    return RsvpSharedDependencies(
        pendingUploadStorage = FilePendingUploadStorage(File(appFilesDir, PendingUploadRelativePath)),
        rssFeedStorage = FileRssFeedStorage(File(appFilesDir, RssFeedsRelativePath)),
        articleFetchClient = ArticleFetchClient(httpClient = httpClient),
        nanoClient = nanoClient,
    )
}

fun createAndroidSharedApp(
    appFilesDir: File,
): RsvpSharedApp {
    return createAndroidSharedDependencies(appFilesDir).createApp()
}

fun createAndroidNanoClient(): NanoClient {
    return NanoKtorClient(httpClient = createAndroidHttpClient())
}

fun createAndroidDeviceSyncService(): NanoDeviceSyncService {
    return NanoDeviceSyncService(createAndroidNanoClient())
}

fun createAndroidCompanionController(
    appFilesDir: File,
): NanoCompanionController {
    return createAndroidSharedDependencies(appFilesDir).createCompanionController()
}

fun createAndroidArticleFetchClient(): ArticleFetchClient {
    return ArticleFetchClient(createAndroidHttpClient())
}

private fun createAndroidHttpClient(): HttpClient {
    return HttpClient(OkHttp) {
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
