@file:OptIn(ExperimentalWasmJsInterop::class, kotlin.io.encoding.ExperimentalEncodingApi::class)

package com.rsvpnano.web

import com.rsvpnano.api.NanoApi
import com.rsvpnano.api.NanoClientError
import com.rsvpnano.app.NanoConnectionTransport
import com.rsvpnano.app.NanoEndpoint
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoFocusTimer
import com.rsvpnano.models.NanoFocusTimers
import com.rsvpnano.models.NanoFontSummary
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoLanguageFont
import com.rsvpnano.models.NanoLocaleSummary
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoStorageRepair
import com.rsvpnano.models.NanoThemeSummary
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.NanoWifiUpdate
import com.rsvpnano.models.RememberedNano
import com.rsvpnano.ui.CompanionPresenter
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeout
import kotlinx.serialization.KSerializer
import kotlinx.serialization.Serializable
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.builtins.serializer
import kotlinx.serialization.json.Json
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException
import kotlin.io.encoding.Base64

internal object BrowserSerial {
    val api = WebSerialNanoApi()
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)

    fun connect(presenter: CompanionPresenter) {
        scope.launch {
            runCatching { api.open() }
                .onSuccess {
                    presenter.connectEndpoint(
                        NanoEndpoint("usb://active", RememberedNano("USB reader")),
                        NanoConnectionTransport.Usb,
                    )
                }
                .onFailure { error ->
                    presenter.reportConnectionFailure(error.message ?: "USB connection failed.")
                }
        }
    }

    suspend fun connectAuthorized(presenter: CompanionPresenter): Boolean {
        return runCatching { api.open(authorizedOnly = true) }
            .map { opened ->
                if (opened) {
                    presenter.connectEndpoint(
                        NanoEndpoint("usb://active", RememberedNano("USB reader")),
                        NanoConnectionTransport.Usb,
                    )
                }
                opened
            }
            .getOrDefault(false)
    }

    suspend fun releaseForInstaller(presenter: CompanionPresenter) {
        api.release()
        presenter.cancelNanoSelection()
    }

    fun connectNetwork(presenter: CompanionPresenter, endpoint: NanoEndpoint) {
        scope.launch {
            api.release()
            presenter.connectEndpoint(endpoint)
        }
    }
}

internal fun requestUsbConnection(presenter: CompanionPresenter) = BrowserSerial.connect(presenter)
internal suspend fun reconnectAuthorizedUsb(presenter: CompanionPresenter): Boolean =
    BrowserSerial.connectAuthorized(presenter)

internal class WebSerialNanoApi : NanoApi {
    private val json = Json { ignoreUnknownKeys = true; encodeDefaults = true; explicitNulls = false }
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private val requestMutex = Mutex()
    private val sessionMutex = Mutex()
    private val writeMutex = Mutex()
    private var frames = Channel<SerialFrame>(Channel.UNLIMITED)
    private var readerJob: Job? = null
    private var heartbeatJob: Job? = null
    private var nextRequestId = 1u
    private var opened = false

    suspend fun open(authorizedOnly: Boolean = false): Boolean = sessionMutex.withLock {
        closeSession()
        if (authorizedOnly) {
            if (!bridgeOpenAuthorized()) return false
        } else {
            bridgeOpen()
        }
        try {
            bridgeWrite("RSVPNANO/COMPANION/1\n".encodeToByteArray())
            val greeting = withTimeout(10_000) {
                var text = ""
                while (true) {
                    text = (text + bridgeRead().decodeToString()).takeLast(4096)
                    when {
                        "RSVPNANO/COMPANION/1 READY" in text -> return@withTimeout text
                        "BUSY MSC" in text -> throw NanoClientError("Exit USB Sync on the Nano before connecting the web companion.")
                        "UNSUPPORTED" in text -> throw NanoClientError("This firmware does not support USB companion protocol 1.")
                    }
                }
                @Suppress("UNREACHABLE_CODE") text
            }
            check("READY" in greeting)
            opened = true
            frames = Channel(Channel.UNLIMITED)
            readerJob = scope.launch { readFrames() }
            heartbeatJob = scope.launch {
                while (isActive) {
                    delay(5_000)
                    runCatching { sendFrame(SerialFrame(SerialFrameType.Ping)) }
                }
            }
            true
        } catch (error: Throwable) {
            bridgeCloseIgnoringErrors()
            throw error
        }
    }

    suspend fun release() = sessionMutex.withLock {
        closeSession()
    }

    override fun close() {
        scope.launch { release() }
    }

    override suspend fun fetchDevice(baseUrl: String): NanoInfo = get("/api/v2/device", NanoInfo.serializer())
    override suspend fun repairStorage(baseUrl: String): NanoStorageRepair =
        decode(request("POST", "/api/v2/storage/repair"), NanoStorageRepair.serializer())
    override suspend fun listLibrary(baseUrl: String): List<NanoBook> = get("/api/v2/library", ListSerializer(NanoBook.serializer()))
    override suspend fun listThemes(baseUrl: String): List<NanoThemeSummary> = get("/api/v2/themes", ListSerializer(NanoThemeSummary.serializer()))
    override suspend fun listFonts(baseUrl: String): List<NanoFontSummary> = get("/api/v2/fonts", ListSerializer(NanoFontSummary.serializer()))
    override suspend fun listLocales(baseUrl: String): List<NanoLocaleSummary> = get("/api/v2/locales", ListSerializer(NanoLocaleSummary.serializer()))
    override suspend fun fetchSettings(baseUrl: String): NanoSettings = get("/api/v2/settings", NanoSettings.serializer())

    override suspend fun updateReadingSettings(baseUrl: String, settings: NanoSettings.Reading) =
        noContent("PATCH", "/api/v2/settings/reading", json.encodeToString(NanoSettings.Reading.serializer(), settings).encodeToByteArray())
    override suspend fun updateDisplaySettings(baseUrl: String, settings: NanoSettings.Interface) =
        noContent("PATCH", "/api/v2/settings/display", json.encodeToString(NanoSettings.Interface.serializer(), settings).encodeToByteArray())
    override suspend fun updateUpdateSettings(baseUrl: String, settings: NanoSettings.Updates) =
        noContent("PATCH", "/api/v2/settings/updates", json.encodeToString(NanoSettings.Updates.serializer(), settings).encodeToByteArray())

    override suspend fun selectTheme(baseUrl: String, id: String) = selectAppearance("theme", id)
    override suspend fun selectFont(baseUrl: String, id: String) = selectAppearance("font", id)
    override suspend fun selectLocale(baseUrl: String, id: String) = selectAppearance("locale", id)

    override suspend fun fetchWifiSettings(baseUrl: String): NanoWifiSettings = get("/api/v2/network", NanoWifiSettings.serializer())
    override suspend fun updateWifi(baseUrl: String, ssid: String, password: String) =
        noContent("PUT", "/api/v2/network", json.encodeToString(NanoWifiUpdate.serializer(), NanoWifiUpdate(ssid, password)).encodeToByteArray())
    override suspend fun forgetWifi(baseUrl: String) = noContent("DELETE", "/api/v2/network")

    override suspend fun fetchRssFeeds(baseUrl: String): NanoRssFeeds =
        NanoRssFeeds(get("/api/v2/feeds", ListSerializer(String.serializer())))
    override suspend fun updateRssFeeds(baseUrl: String, config: NanoRssFeeds) =
        noContent("PUT", "/api/v2/feeds", json.encodeToString(NanoRssFeeds.serializer(), config).encodeToByteArray())
    override suspend fun fetchFocusTimers(baseUrl: String): NanoFocusTimers =
        NanoFocusTimers(get("/api/v2/focus-timers", ListSerializer(NanoFocusTimer.serializer())))
    override suspend fun updateFocusTimers(baseUrl: String, timers: NanoFocusTimers) =
        noContent("PUT", "/api/v2/focus-timers", json.encodeToString(NanoFocusTimers.serializer(), timers).encodeToByteArray())

    override suspend fun uploadBook(baseUrl: String, name: String, data: ByteArray, category: String?, onProgress: ((Long, Long) -> Unit)?): NanoBook =
        upload("/api/v2/library", name, data, category, onProgress, NanoBook.serializer())
    override suspend fun deleteBook(baseUrl: String, id: String) = noContent("DELETE", "/api/v2/library/$id")
    override suspend fun setBookPosition(baseUrl: String, id: String, wordIndex: Int) =
        noContent("PUT", "/api/v2/library/$id/position", json.encodeToString(BookPositionUpdate.serializer(), BookPositionUpdate(wordIndex)).encodeToByteArray())
    override suspend fun setBookLanguageFonts(baseUrl: String, id: String, languageFonts: List<NanoLanguageFont>) =
        noContent("PUT", "/api/v2/library/$id/language-fonts", json.encodeToString(BookLanguageFontsUpdate.serializer(), BookLanguageFontsUpdate(languageFonts)).encodeToByteArray())

    override suspend fun uploadTheme(baseUrl: String, name: String, data: ByteArray, onProgress: ((Long, Long) -> Unit)?): NanoThemeSummary =
        upload("/api/v2/themes", name, data, null, onProgress, NanoThemeSummary.serializer())
    override suspend fun deleteTheme(baseUrl: String, id: String) = noContent("DELETE", "/api/v2/themes/$id")
    override suspend fun uploadFont(baseUrl: String, name: String, data: ByteArray, onProgress: ((Long, Long) -> Unit)?): NanoFontSummary =
        upload("/api/v2/fonts", name, data, null, onProgress, NanoFontSummary.serializer())
    override suspend fun deleteFont(baseUrl: String, id: String) = noContent("DELETE", "/api/v2/fonts/$id")
    override suspend fun uploadLocalePack(baseUrl: String, name: String, data: ByteArray, onProgress: ((Long, Long) -> Unit)?): NanoLocaleSummary =
        upload("/api/v2/locales", name, data, null, onProgress, NanoLocaleSummary.serializer())
    override suspend fun deleteLocalePack(baseUrl: String, id: String) = noContent("DELETE", "/api/v2/locales/$id")

    private suspend fun selectAppearance(resource: String, id: String) =
        noContent("PUT", "/api/v2/appearance/$resource", json.encodeToString(AppearanceSelection.serializer(), AppearanceSelection(id)).encodeToByteArray())

    private suspend fun <T> get(path: String, serializer: KSerializer<T>): T {
        val response = request("GET", path)
        return decode(response, serializer)
    }

    private suspend fun noContent(method: String, path: String, body: ByteArray = byteArrayOf()) {
        val response = request(method, path, body = body, contentType = if (body.isEmpty()) null else "application/json")
        requireSuccess(response)
        if (response.status != 204 || response.body.isNotEmpty()) throw NanoClientError("Device returned an invalid empty response.")
    }

    private suspend fun <T> upload(
        path: String,
        name: String,
        data: ByteArray,
        category: String?,
        onProgress: ((Long, Long) -> Unit)?,
        serializer: KSerializer<T>,
    ): T {
        val query = buildMap {
            put("name", name)
            category?.let { put("category", it) }
        }
        return decode(request("POST", path, query, "application/octet-stream", data, onProgress), serializer)
    }

    private suspend fun request(
        method: String,
        path: String,
        query: Map<String, String> = emptyMap(),
        contentType: String? = null,
        body: ByteArray = byteArrayOf(),
        onProgress: ((Long, Long) -> Unit)? = null,
    ): SerialResponse = requestMutex.withLock {
        check(opened) { "Choose a USB port before using the USB companion." }
        val requestId = nextRequestId++
        val metadata = SerialRequestMetadata(method, path, query, contentType, body.size.toLong())
        sendFrame(SerialFrame(SerialFrameType.Request, requestId, payload = json.encodeToString(SerialRequestMetadata.serializer(), metadata).encodeToByteArray()))

        var sequence = 0u
        body.asList().chunked(SerialChunkBytes).forEach { values ->
            val chunk = values.toByteArray()
            sendFrame(SerialFrame(SerialFrameType.Data, requestId, sequence, chunk))
            awaitFrame(requestId, SerialFrameType.Acknowledgement, sequence)
            sequence++
            onProgress?.invoke(minOf(sequence.toLong() * SerialChunkBytes, body.size.toLong()), body.size.toLong())
        }
        val end = SerialTransferEnd(body.size.toLong(), SerialFrameCodec.crc32(body))
        sendFrame(SerialFrame(SerialFrameType.End, requestId, sequence, json.encodeToString(SerialTransferEnd.serializer(), end).encodeToByteArray()))

        val responseFrame = awaitFrame(requestId, SerialFrameType.Response)
        val response = json.decodeFromString(SerialResponseMetadata.serializer(), responseFrame.payload.decodeToString())
        val responseBody = ArrayList<Byte>(response.totalBytes.coerceAtMost(Int.MAX_VALUE.toLong()).toInt())
        while (true) {
            val frame = awaitFrame(requestId)
            when (frame.type) {
                SerialFrameType.Data -> {
                    responseBody.addAll(frame.payload.asList())
                    sendFrame(SerialFrame(SerialFrameType.Acknowledgement, requestId, frame.sequence))
                }
                SerialFrameType.End -> break
                SerialFrameType.Error -> throw NanoClientError(frame.payload.decodeToString())
                else -> Unit
            }
        }
        val bytes = responseBody.toByteArray()
        if (bytes.size.toLong() != response.totalBytes) throw NanoClientError("USB response was interrupted.")
        SerialResponse(response.status, bytes)
    }

    private suspend fun awaitFrame(requestId: UInt, type: SerialFrameType? = null, sequence: UInt? = null): SerialFrame =
        withTimeout(20_000) {
            while (true) {
                val frame = frames.receive()
                if (frame.type == SerialFrameType.Error && (frame.requestId == 0u || frame.requestId == requestId)) {
                    throw NanoClientError(frame.payload.decodeToString())
                }
                if (frame.requestId == requestId && (type == null || frame.type == type) &&
                    (sequence == null || frame.sequence == sequence)) {
                    return@withTimeout frame
                }
            }
            @Suppress("UNREACHABLE_CODE") error("Serial frame wait ended unexpectedly.")
        }

    private suspend fun sendFrame(frame: SerialFrame) = writeMutex.withLock {
        bridgeWrite(SerialFrameCodec.encode(frame))
    }

    private suspend fun readFrames() {
        val decoder = SerialFrameCodec.Decoder()
        try {
            while (scope.isActive && opened) {
                decoder.feed(bridgeRead()).forEach { frame ->
                    if (frame.type == SerialFrameType.Ping) sendFrame(SerialFrame(SerialFrameType.Pong))
                    else frames.send(frame)
                }
            }
        } catch (_: CancellationException) {
            throw CancellationException()
        } catch (error: Throwable) {
            opened = false
            frames.close(error)
        }
    }

    private suspend fun closeSession() {
        if (opened) runCatching { sendFrame(SerialFrame(SerialFrameType.Close)) }
        opened = false
        heartbeatJob?.cancel()
        readerJob?.cancel()
        frames.close()
        bridgeCloseIgnoringErrors()
    }

    private fun <T> decode(response: SerialResponse, serializer: KSerializer<T>): T {
        requireSuccess(response)
        return runCatching { json.decodeFromString(serializer, response.body.decodeToString()) }
            .getOrElse { throw NanoClientError("Device returned an invalid USB response.", response.status, cause = it) }
    }

    private fun requireSuccess(response: SerialResponse) {
        if (response.status in 200..299) return
        val error = runCatching { json.decodeFromString(DeviceError.serializer(), response.body.decodeToString()) }.getOrNull()
        throw NanoClientError(error?.message ?: "Device rejected the USB request.", response.status, error?.code, error?.field)
    }

    private data class SerialResponse(val status: Int, val body: ByteArray)

    @Serializable private data class SerialRequestMetadata(
        val method: String,
        val path: String,
        val query: Map<String, String> = emptyMap(),
        val contentType: String? = null,
        val totalBytes: Long = 0,
    )
    @Serializable private data class SerialResponseMetadata(val status: Int, val contentType: String? = null, val totalBytes: Long = 0)
    @Serializable private data class SerialTransferEnd(val totalBytes: Long, val crc32: UInt)
    @Serializable private data class DeviceError(val code: String, val message: String, val field: String? = null)
    @Serializable private data class AppearanceSelection(val id: String)
    @Serializable private data class BookPositionUpdate(val wordIndex: Int)
    @Serializable private data class BookLanguageFontsUpdate(val languageFonts: List<NanoLanguageFont>)
}

private suspend fun bridgeOpen() = suspendCancellableCoroutine { continuation ->
    serialOpen(
        { if (continuation.isActive) continuation.resume(Unit) },
        { message -> if (continuation.isActive) continuation.resumeWithException(NanoClientError(message)) },
    )
}

private suspend fun bridgeOpenAuthorized(): Boolean = suspendCancellableCoroutine { continuation ->
    serialOpenAuthorized(
        { found -> if (continuation.isActive) continuation.resume(found) },
        { message -> if (continuation.isActive) continuation.resumeWithException(NanoClientError(message)) },
    )
}

private suspend fun bridgeRead(): ByteArray = suspendCancellableCoroutine { continuation ->
    serialRead(
        { encoded ->
            if (!continuation.isActive) return@serialRead
            if (encoded.isEmpty()) continuation.resumeWithException(NanoClientError("The USB device disconnected."))
            else continuation.resume(Base64.decode(encoded))
        },
        { message -> if (continuation.isActive) continuation.resumeWithException(NanoClientError(message)) },
    )
}

private suspend fun bridgeWrite(data: ByteArray) = suspendCancellableCoroutine { continuation ->
    serialWrite(
        Base64.encode(data),
        { if (continuation.isActive) continuation.resume(Unit) },
        { message -> if (continuation.isActive) continuation.resumeWithException(NanoClientError(message)) },
    )
}

private suspend fun bridgeCloseIgnoringErrors() = suspendCancellableCoroutine { continuation ->
    serialClose { if (continuation.isActive) continuation.resume(Unit) }
}

@JsFun("""(ok, fail) => { (async () => { try { const key = 'rsvpnano.web.usbDevice'; let saved = null; try { saved = JSON.parse(localStorage.getItem(key) || 'null'); } catch (_) { localStorage.removeItem(key); } const authorized = await navigator.serial.getPorts(); const matches = saved ? authorized.filter(port => { const info = port.getInfo(); return info.usbVendorId === saved.usbVendorId && info.usbProductId === saved.usbProductId; }) : []; const port = matches.length === 1 ? matches[0] : authorized.length === 1 ? authorized[0] : await navigator.serial.requestPort(); await port.open({ baudRate: 115200 }); const info = port.getInfo(); if (info.usbVendorId || info.usbProductId) localStorage.setItem(key, JSON.stringify({ usbVendorId: info.usbVendorId || 0, usbProductId: info.usbProductId || 0 })); globalThis.rsvpNanoSerial = { port, reader: port.readable.getReader(), writer: port.writable.getWriter() }; ok('ok'); } catch (error) { fail(error?.message || String(error)); } })(); }""")
private external fun serialOpen(ok: (String) -> Unit, fail: (String) -> Unit)

@JsFun("""(ok, fail) => { (async () => { try { const key = 'rsvpnano.web.usbDevice'; let saved = null; try { saved = JSON.parse(localStorage.getItem(key) || 'null'); } catch (_) { localStorage.removeItem(key); } const authorized = await navigator.serial.getPorts(); const matches = saved ? authorized.filter(port => { const info = port.getInfo(); return info.usbVendorId === saved.usbVendorId && info.usbProductId === saved.usbProductId; }) : authorized; if (matches.length !== 1) { ok(false); return; } const port = matches[0]; await port.open({ baudRate: 115200 }); const info = port.getInfo(); if (info.usbVendorId || info.usbProductId) localStorage.setItem(key, JSON.stringify({ usbVendorId: info.usbVendorId || 0, usbProductId: info.usbProductId || 0 })); globalThis.rsvpNanoSerial = { port, reader: port.readable.getReader(), writer: port.writable.getWriter() }; ok(true); } catch (error) { fail(error?.message || String(error)); } })(); }""")
private external fun serialOpenAuthorized(ok: (Boolean) -> Unit, fail: (String) -> Unit)

@JsFun("""(ok, fail) => { const serial = globalThis.rsvpNanoSerial; if (!serial) { fail('No USB port is open.'); return; } serial.reader.read().then(({ value, done }) => { if (done || !value) { ok(''); return; } let text = ''; for (let i = 0; i < value.length; i++) text += String.fromCharCode(value[i]); ok(btoa(text)); }).catch(error => fail(error?.message || String(error))); }""")
private external fun serialRead(ok: (String) -> Unit, fail: (String) -> Unit)

@JsFun("""(encoded, ok, fail) => { const serial = globalThis.rsvpNanoSerial; if (!serial) { fail('No USB port is open.'); return; } const text = atob(encoded); const bytes = new Uint8Array(text.length); for (let i = 0; i < text.length; i++) bytes[i] = text.charCodeAt(i); serial.writer.write(bytes).then(() => ok('ok')).catch(error => fail(error?.message || String(error))); }""")
private external fun serialWrite(encoded: String, ok: (String) -> Unit, fail: (String) -> Unit)

@JsFun("""(done) => { (async () => { const serial = globalThis.rsvpNanoSerial; globalThis.rsvpNanoSerial = null; if (!serial) { done('ok'); return; } try { await serial.reader.cancel(); } catch (_) {} try { serial.reader.releaseLock(); } catch (_) {} try { await serial.writer.close(); } catch (_) {} try { serial.writer.releaseLock(); } catch (_) {} try { await serial.port.close(); } catch (_) {} done('ok'); })(); }""")
private external fun serialClose(done: (String) -> Unit)
