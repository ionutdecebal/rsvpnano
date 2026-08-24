package com.rsvpnano

import com.rsvpnano.api.NanoKtorClient
import com.rsvpnano.api.NanoClientError
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoFocusTimer
import com.rsvpnano.models.NanoFocusTimers
import com.rsvpnano.models.NanoFontSummary
import com.rsvpnano.models.NanoLanguageFont
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoLocaleSummary
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoStorageRepair
import com.rsvpnano.models.NanoThemeSummary
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.NanoWifiUpdate
import io.ktor.client.HttpClient
import io.ktor.client.engine.mock.MockEngine
import io.ktor.client.engine.mock.respond
import io.ktor.client.plugins.contentnegotiation.ContentNegotiation
import io.ktor.client.request.HttpRequestData
import io.ktor.http.ContentType
import io.ktor.http.HttpHeaders
import io.ktor.http.HttpMethod
import io.ktor.http.HttpStatusCode
import io.ktor.http.headersOf
import io.ktor.http.content.TextContent
import io.ktor.serialization.kotlinx.json.json
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.builtins.serializer
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertIs

class NanoKtorClientAndroidTest {
    @Test
    fun resolvesPublishedBuildVersionFromReleaseTarget() = runBlocking {
        val seen = mutableListOf<String>()
        val client = NanoKtorClient(mockHttpClient { request ->
            seen += request.url.encodedPath
            when (request.url.encodedPath) {
                "/repos/reader/rsvpnano/releases/tags/preview-v0.0.9" ->
                    """{"tag_name":"preview-v0.0.9","target_commitish":"0123456789abcdef0123456789abcdef01234567","assets":[{"name":"reader-ota.bin"}]}"""
                else -> error("Unexpected request: ${request.url}")
            }
        })

        val release = client.fetchFirmwareRelease("reader", "rsvpnano", "preview-v0.0.9")

        assertEquals("preview-v0.0.9+0123456789ab", release.version)
        assertEquals(listOf("reader-ota.bin"), release.assets)
        assertEquals(
            listOf("/repos/reader/rsvpnano/releases/tags/preview-v0.0.9"),
            seen,
        )
    }

    @Test
    fun fetchesIndependentDeviceAndCollectionResources() = runBlocking {
        val seen = mutableListOf<String>()
        val device = NanoInfo("RSVP-Nano-123456", "preview-v0.0.9+abc", "reader-ota.bin")
        val books = listOf(sampleBook("b12345678", "Book", 1000))
        val client = NanoKtorClient(mockHttpClient { request ->
            seen += "${request.method.value} ${request.url.encodedPath}"
            when (request.url.encodedPath) {
                "/api/v2/device" -> testJson.encodeToString(NanoInfo.serializer(), device)
                "/api/v2/storage/repair" -> testJson.encodeToString(
                    NanoStorageRepair(
                        healthy = true,
                        checked = 12,
                        moved = 2,
                        removed = 1,
                        diagnosticSummary = "Storage OK",
                        diagnosticDetail = "FAT32",
                    ),
                )
                "/api/v2/library" -> libraryJson(books)
                "/api/v2/themes" -> """[{"id":"default","name":"Default"}]"""
                "/api/v2/fonts" -> "[]"
                "/api/v2/locales" -> "[]"
                else -> error("Unexpected request: ${request.url}")
            }
        })

        val info = client.fetchDevice("http://device.local")
        val repair = client.repairStorage("http://device.local")
        val book = client.listLibrary("http://device.local").single()
        assertEquals("RSVP-Nano-123456", info.ssid)
        assertEquals("preview-v0.0.9+abc", info.firmwareVersion)
        assertEquals(2, repair.moved)
        assertEquals("Default", client.listThemes("http://device.local").single().name)
        assertEquals(emptyList(), client.listFonts("http://device.local"))
        assertEquals(emptyList(), client.listLocales("http://device.local"))
        assertEquals("b12345678", book.id)
        assertEquals("Book", book.metadata.title)
        assertEquals(1000, book.metadata.wordCount)
        assertEquals(
            listOf(
                "GET /api/v2/device",
                "POST /api/v2/storage/repair",
                "GET /api/v2/library",
                "GET /api/v2/themes",
                "GET /api/v2/fonts",
                "GET /api/v2/locales",
            ),
            seen,
        )
    }

    @Test
    fun settingsRoundTripUsesDomainResources() = runBlocking {
        val seen = mutableListOf<String>()
        var deviceSettings = sampleSettings()
        val client = NanoKtorClient(mockHttpClient(
            status = { request ->
                if (request.method == HttpMethod.Patch) HttpStatusCode.NoContent else HttpStatusCode.OK
            },
        ) { request ->
            seen += "${request.method.value} ${request.url.encodedPath}"
            if (request.method == HttpMethod.Patch) {
                assertEquals(ContentType.Application.Json, request.body.contentType)
                return@mockHttpClient when (request.url.encodedPath) {
                    "/api/v2/settings/reading" -> {
                        val value = testJson.decodeFromString(NanoSettings.Reading.serializer(), requestBodyText(request))
                        deviceSettings = deviceSettings.copy(reading = value)
                        ""
                    }
                    "/api/v2/settings/display" -> {
                        val value = testJson.decodeFromString(NanoSettings.Interface.serializer(), requestBodyText(request))
                        deviceSettings = deviceSettings.copy(`interface` = value)
                        ""
                    }
                    "/api/v2/settings/updates" -> {
                        val value = testJson.decodeFromString(NanoSettings.Updates.serializer(), requestBodyText(request))
                        deviceSettings = deviceSettings.copy(updates = value)
                        ""
                    }
                    else -> error("Unexpected request: ${request.url}")
                }
            }
            testJson.encodeToString(NanoSettings.serializer(), deviceSettings)
        })

        val fetched = client.fetchSettings("http://device.local")
        assertEquals(sampleSettings(), fetched)

        val requested = fetched
            .withWpm(450)
            .withBrightnessPercent(55)
            .withLocale("es")
            .withTypeface("atkinson")
            .withThemeId("default")
        client.updateReadingSettings("http://device.local", requested.reading)
        client.updateDisplaySettings("http://device.local", requested.`interface`)
        client.updateUpdateSettings("http://device.local", requested.updates)
        val fetchedAgain = client.fetchSettings("http://device.local")

        assertEquals(requested, fetchedAgain)
        assertEquals(
            listOf(
                "GET /api/v2/settings",
                "PATCH /api/v2/settings/reading",
                "PATCH /api/v2/settings/display",
                "PATCH /api/v2/settings/updates",
                "GET /api/v2/settings",
            ),
            seen,
        )
    }

    @Test
    fun appearanceSelectionsUseFocusedIdempotentResources() = runBlocking {
        val seen = mutableListOf<String>()
        val client = NanoKtorClient(mockHttpClient(
            status = { HttpStatusCode.NoContent },
        ) { request ->
            seen += "${request.method.value} ${request.url.encodedPath}"
            assertEquals(HttpMethod.Put, request.method)
            val id = testJson.parseToJsonElement(requestBodyText(request)).jsonObject.getValue("id").jsonPrimitive.content
            if (request.url.encodedPath !in setOf(
                    "/api/v2/appearance/theme",
                    "/api/v2/appearance/font",
                    "/api/v2/appearance/locale",
                )
            ) {
                error("Unexpected request: ${request.url}")
            }
            ""
        })

        client.selectTheme("http://device.local", "night")
        client.selectFont("http://device.local", "andika")
        client.selectLocale("http://device.local", "he")
        assertEquals(
            listOf(
                "PUT /api/v2/appearance/theme",
                "PUT /api/v2/appearance/font",
                "PUT /api/v2/appearance/locale",
            ),
            seen,
        )
    }

    @Test
    fun configurationApisRoundTripTheirPayloads() = runBlocking {
        val seen = mutableListOf<String>()
        var ssid = ""
        var feeds = NanoRssFeeds(emptyList())
        var focus = NanoFocusTimers(emptyList())
        val client = NanoKtorClient(mockHttpClient(
            status = { request ->
                if (request.method == HttpMethod.Get) HttpStatusCode.OK else HttpStatusCode.NoContent
            },
        ) { request ->
            seen += "${request.method.value} ${request.url.encodedPath}"
            when (request.url.encodedPath) {
                "/api/v2/network" -> {
                    when (request.method) {
                        HttpMethod.Put -> {
                            val update = testJson.decodeFromString(NanoWifiUpdate.serializer(), requestBodyText(request))
                            assertEquals(NanoWifiUpdate("Home", "secret"), update)
                            ssid = update.ssid
                        }
                        HttpMethod.Delete -> ssid = ""
                    }
                    if (request.method == HttpMethod.Get) """{"ssid":"$ssid"}""" else ""
                }
                "/api/v2/feeds" -> {
                    if (request.method == HttpMethod.Put) {
                        feeds = testJson.decodeFromString(NanoRssFeeds.serializer(), requestBodyText(request))
                    }
                    if (request.method == HttpMethod.Get) {
                        testJson.encodeToString(ListSerializer(String.serializer()), feeds.feeds)
                    } else ""
                }
                "/api/v2/focus-timers" -> {
                    if (request.method == HttpMethod.Put) {
                        focus = testJson.decodeFromString(NanoFocusTimers.serializer(), requestBodyText(request))
                    }
                    if (request.method == HttpMethod.Get) {
                        testJson.encodeToString(ListSerializer(NanoFocusTimer.serializer()), focus.timers)
                    } else ""
                }
                else -> error("Unexpected request: ${request.url}")
            }
        })

        assertEquals("", client.fetchWifiSettings("http://device.local").ssid)
        client.updateWifi("http://device.local", "Home", "secret")
        client.forgetWifi("http://device.local")
        val requestedFeeds = NanoRssFeeds(listOf("https://example.com/feed.xml"))
        val requestedFocus = NanoFocusTimers(listOf(NanoFocusTimer("Deep work", 50, 10, 3)))
        client.updateRssFeeds("http://device.local", requestedFeeds)
        assertEquals(requestedFeeds, client.fetchRssFeeds("http://device.local"))
        client.updateFocusTimers("http://device.local", requestedFocus)
        assertEquals(requestedFocus, client.fetchFocusTimers("http://device.local"))
        assertEquals(
            listOf(
                "GET /api/v2/network",
                "PUT /api/v2/network",
                "DELETE /api/v2/network",
                "PUT /api/v2/feeds",
                "GET /api/v2/feeds",
                "PUT /api/v2/focus-timers",
                "GET /api/v2/focus-timers",
            ),
            seen,
        )
    }

    @Test
    fun bookMutationsUseDeviceContract() = runBlocking {
        val seen = mutableListOf<String>()
        val story = sampleBook("b12345678", "Story", 1000)
        val client = NanoKtorClient(mockHttpClient(
            status = { request ->
                if (request.method == HttpMethod.Post) HttpStatusCode.OK else HttpStatusCode.NoContent
            },
        ) { request ->
            seen += "${request.method.value} ${request.url.encodedPath}?${request.url.encodedQuery}"
            when (request.method) {
                HttpMethod.Post -> {
                    assertEquals("article", request.url.parameters["category"])
                    assertEquals("Story.rsvp", request.url.parameters["name"])
                    assertEquals(ContentType.Application.OctetStream, request.body.contentType)
                    testJson.encodeToString(NanoBook.serializer(), story)
                }
                HttpMethod.Delete -> {
                    assertEquals("/api/v2/library/b12345678", request.url.encodedPath)
                    ""
                }
                HttpMethod.Put -> {
                    val body = testJson.parseToJsonElement(requestBodyText(request)).jsonObject
                    if (request.url.encodedPath.endsWith("/position")) {
                        assertEquals(250, body.getValue("wordIndex").jsonPrimitive.int)
                    } else {
                        val fonts = body.getValue("languageFonts").jsonArray
                        assertEquals("ar", fonts[0].jsonObject.getValue("locale").jsonPrimitive.content)
                        assertEquals("noto-sans-arabic", fonts[0].jsonObject.getValue("fontId").jsonPrimitive.content)
                        assertEquals("math", fonts[1].jsonObject.getValue("locale").jsonPrimitive.content)
                        assertEquals("stix-two-math", fonts[1].jsonObject.getValue("fontId").jsonPrimitive.content)
                    }
                    ""
                }
                else -> error("Unexpected method: ${request.method}")
            }
        })

        val uploaded = client.uploadBook(
            baseUrl = "http://device.local",
            name = "Story.rsvp",
            data = byteArrayOf(1, 2, 3),
            category = "article",
        )
        client.deleteBook("http://device.local", "b12345678")
        client.setBookPosition(
            baseUrl = "http://device.local",
            id = "b12345678",
            wordIndex = 250,
        )
        client.setBookLanguageFonts(
            baseUrl = "http://device.local",
            id = "b12345678",
            languageFonts = listOf(
                NanoLanguageFont(locale = "ar", fontId = "noto-sans-arabic"),
                NanoLanguageFont(locale = "math", fontId = "stix-two-math"),
            ),
        )

        assertEquals("b12345678", uploaded.id)
        assertEquals(
            listOf(
                "POST /api/v2/library?name=Story.rsvp&category=article",
                "DELETE /api/v2/library/b12345678?",
                "PUT /api/v2/library/b12345678/position?",
                "PUT /api/v2/library/b12345678/language-fonts?",
            ),
            seen,
        )
    }

    @Test
    fun rejectsPartialSuccessfulDeviceResponses() = runBlocking {
        val client = NanoKtorClient(mockHttpClient { """{"firmwareVersion":"preview-v0.0.9+abc"}""" })

        val error = assertFailsWith<NanoClientError> {
            client.fetchDevice("http://device.local")
        }

        assertEquals("Device returned an invalid API response.", error.message)
    }

    @Test
    fun appearanceCatalogsDownloadsAndUploadsUseTheirContracts() = runBlocking {
        val seen = mutableListOf<String>()
        val themeResource = NanoThemeSummary("night", "Night")
        val fontResource = NanoFontSummary("atkinson", "Atkinson Hyperlegible")
        val localeResource = NanoLocaleSummary("ja", "日本語", "ja")
        val client = NanoKtorClient(mockHttpClient(
            status = { request ->
                when (request.method) {
                    HttpMethod.Post -> HttpStatusCode.Created
                    HttpMethod.Delete -> HttpStatusCode.NoContent
                    else -> HttpStatusCode.OK
                }
            },
        ) { request ->
            seen += "${request.method.value} ${request.url.encodedPath}?${request.url.encodedQuery}"
            when (request.url.encodedPath) {
                "/themes/catalog.json" ->
                    """[{"id":"night","name":"Night","file":"night.toml"},{"id":"broken"}]"""
                "/themes/night.toml" -> "theme-data"
                "/fonts/catalog.json" ->
                    """[{"id":"atkinson","name":"Atkinson Hyperlegible","file":"atkinson/font.rfont4"},{"name":"broken"}]"""
                "/fonts/atkinson/font.rfont4" -> "font-data"
                "/locale-packs/index.json" ->
                    """[{"id":"ja","name":"日本語","englishName":"Japanese","version":"1.0.0","locale":"ja","direction":"ltr","scripts":["Hani","Hira","Kana"],"translationStatus":"preview","file":"ja.zip"},{"id":"broken"}]"""
                "/locale-packs/ja.zip" -> "locale-pack-data"
                "/api/v2/themes" -> {
                    assertEquals(HttpMethod.Post, request.method)
                    assertEquals("night.toml", request.url.parameters["name"])
                    assertEquals(ContentType.Application.OctetStream, request.body.contentType)
                    testJson.encodeToString(NanoThemeSummary.serializer(), themeResource)
                }
                "/api/v2/themes/night", "/api/v2/fonts/atkinson", "/api/v2/locales/ja" -> ""
                "/api/v2/fonts" -> {
                    assertEquals(null, request.url.parameters["name"])
                    assertEquals(ContentType.Application.OctetStream, request.body.contentType)
                    testJson.encodeToString(NanoFontSummary.serializer(), fontResource)
                }
                "/api/v2/locales" -> {
                    assertEquals(null, request.url.parameters["name"])
                    assertEquals(ContentType.Application.OctetStream, request.body.contentType)
                    testJson.encodeToString(NanoLocaleSummary.serializer(), localeResource)
                }
                else -> error("Unexpected request: ${request.url}")
            }
        })

        val theme = client.fetchThemeCatalog("https://catalog.example/themes/catalog.json").single()
        val font = client.fetchFontCatalog("https://catalog.example/fonts/catalog.json").single()
        val locale = client.fetchLocaleCatalog("https://catalog.example/locale-packs/index.json").single()
        assertEquals("night", theme.id)
        assertEquals("atkinson/font.rfont4", font.file)
        assertEquals("ja.zip", locale.file)
        val downloadProgress = mutableMapOf<String, Long>()
        assertContentEquals(
            "theme-data".encodeToByteArray(),
            client.downloadTheme("https://catalog.example/themes/night.toml") { received, _ ->
                downloadProgress["theme"] = received
            },
        )
        assertContentEquals(
            "font-data".encodeToByteArray(),
            client.downloadFont("https://catalog.example/fonts/atkinson/font.rfont4") { received, _ ->
                downloadProgress["font"] = received
            },
        )
        assertContentEquals(
            "locale-pack-data".encodeToByteArray(),
            client.downloadLocalePack("https://catalog.example/locale-packs/ja.zip") { received, _ ->
                downloadProgress["locale"] = received
            },
        )
        assertEquals(10, downloadProgress["theme"])
        assertEquals(9, downloadProgress["font"])
        assertEquals(16, downloadProgress["locale"])
        assertEquals(themeResource, client.uploadTheme("http://device.local", "night.toml", "theme-data".encodeToByteArray()))
        client.deleteTheme("http://device.local", "night")
        assertEquals(
            fontResource,
            client.uploadFont(
                "http://device.local",
                "font.rfont4",
                "font-data".encodeToByteArray(),
            ),
        )
        client.deleteFont("http://device.local", "atkinson")
        assertEquals(localeResource, client.uploadLocalePack("http://device.local", "ja.zip", byteArrayOf(1)))
        client.deleteLocalePack("http://device.local", "ja")
        assertEquals(
            listOf(
                "GET /themes/catalog.json?",
                "GET /fonts/catalog.json?",
                "GET /locale-packs/index.json?",
                "GET /themes/night.toml?",
                "GET /fonts/atkinson/font.rfont4?",
                "GET /locale-packs/ja.zip?",
                "POST /api/v2/themes?name=night.toml",
                "DELETE /api/v2/themes/night?",
                "POST /api/v2/fonts?",
                "DELETE /api/v2/fonts/atkinson?",
                "POST /api/v2/locales?",
                "DELETE /api/v2/locales/ja?",
            ),
            seen,
        )
    }

    @Test
    fun exposesPlainDeviceErrors() = runBlocking {
        val client = NanoKtorClient(
            HttpClient(MockEngine) {
                engine {
                    addHandler {
                        respond(
                            content = "wpm is out of range",
                            status = HttpStatusCode.UnprocessableEntity,
                            headers = headersOf(HttpHeaders.ContentType, ContentType.Text.Plain.toString()),
                        )
                    }
                }
            }
        )

        val error = assertFailsWith<NanoClientError> {
            client.fetchSettings("http://device.local")
        }

        assertEquals("wpm is out of range", error.message)
        assertEquals(422, error.status)
    }

    @Test
    fun exposesStructuredDeviceErrors() = runBlocking {
        val client = NanoKtorClient(mockHttpClient(
            status = { HttpStatusCode.NotFound },
        ) { """{"code":"font_not_found","message":"Font not found","field":"id"}""" })

        val error = assertFailsWith<NanoClientError> {
            client.listFonts("http://device.local")
        }

        assertEquals("Font not found", error.message)
        assertEquals(404, error.status)
        assertEquals("font_not_found", error.code)
        assertEquals("id", error.field)
    }

    private fun requestBodyText(request: HttpRequestData): String =
        assertIs<TextContent>(request.body).text

    private fun libraryJson(books: List<NanoBook>): String =
        testJson.encodeToString(ListSerializer(NanoBook.serializer()), books)

    private fun mockHttpClient(
        status: (HttpRequestData) -> HttpStatusCode = { HttpStatusCode.OK },
        handler: (HttpRequestData) -> String,
    ): HttpClient {
        return HttpClient(MockEngine) {
            engine {
                addHandler { request ->
                    respond(
                        content = handler(request),
                        status = status(request),
                        headers = headersOf(HttpHeaders.ContentType, ContentType.Application.Json.toString()),
                    )
                }
            }
            install(ContentNegotiation) {
                json(
                    testJson
                )
            }
        }
    }

    private companion object {
        val testJson = Json {
            ignoreUnknownKeys = true
            encodeDefaults = true
            explicitNulls = false
        }
    }
}
