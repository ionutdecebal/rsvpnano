@file:OptIn(ExperimentalWasmJsInterop::class, kotlin.io.encoding.ExperimentalEncodingApi::class, kotlinx.coroutines.ExperimentalCoroutinesApi::class)

package com.rsvpnano.web.connection

import com.rsvpnano.web.connection.SerialFrame
import com.rsvpnano.web.connection.SerialFrameCodec
import com.rsvpnano.web.connection.SerialFrameType
import com.rsvpnano.web.connection.WebSerialNanoApi
import com.rsvpnano.web.setup.restartNanoInBootloader
import kotlin.io.encoding.Base64
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import kotlin.test.assertFailsWith
import com.rsvpnano.api.NanoClientError
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class WebSerialNanoApiTest {
    @Test
    fun deviceCloseReleasesIdleSessionAndAllowsReconnect() = runTest {
        withContext(Dispatchers.Default.limitedParallelism(1)) {
            val greeting = Base64.encode("RSVPNANO/COMPANION/1 READY\n".encodeToByteArray())
            installFakeSerial(greeting)
            val api = WebSerialNanoApi()
            val disconnected = CompletableDeferred<Unit>()
            var disconnects = 0
            api.open(onDisconnect = { disconnects++; disconnected.complete(Unit) })
            queueFakeSerialRead(Base64.encode(SerialFrameCodec.encode(SerialFrame(SerialFrameType.Close))))
            withTimeout(1_000) { disconnected.await() }
            assertEquals(1, fakeSerialCloseCount())
            assertEquals(1, disconnects)

            queueFakeSerialRead(greeting)
            assertTrue(api.open())
            api.release()
            assertEquals(2, fakeSerialCloseCount())
            assertEquals(1, disconnects)
        }
    }

    @Test
    fun uploadDisconnectStopsHeartbeatReleasesPortAndCanReconnect() = runTest {
        withContext(Dispatchers.Default.limitedParallelism(1)) {
            val greeting = "RSVPNANO/COMPANION/1 READY\n".encodeToByteArray()
            val ack = SerialFrameCodec.encode(SerialFrame(SerialFrameType.Acknowledgement, 1u))
            installFakeSerial(listOf(greeting, ack).joinToString("|") { Base64.encode(it) })
            val api = WebSerialNanoApi(backgroundScope)
            var disconnects = 0
            assertTrue(api.open(onDisconnect = { disconnects++ }))
            disconnectFakeSerialAfterEnd()

            assertFailsWith<NanoClientError> {
                api.uploadTheme("usb://active", "nord.toml", ByteArray(371))
            }
            assertEquals(1, disconnects)
            assertEquals(1, fakeSerialCloseCount())
            val sent = fakeSerialFrames()
            assertEquals(371, sent.single { it.type == SerialFrameType.Data }.payload.size)
            assertEquals(SerialFrameType.End, sent.last().type)
            testScheduler.advanceTimeBy(5_100)
            testScheduler.runCurrent()
            assertEquals(sent.size, fakeSerialFrames().size, "Disconnected sessions must stop their heartbeat")

            queueFakeSerialRead(Base64.encode(greeting))
            assertTrue(api.open(onDisconnect = { disconnects++ }))
            val body = """{"ssid":"RSVP-Nano","firmwareVersion":"0.0.9","otaAsset":"nano-ota.bin"}""".encodeToByteArray()
            val metadata = """{"status":200,"totalBytes":${body.size}}""".encodeToByteArray()
            queueFakeSerialRead(Base64.encode(
                SerialFrameCodec.encode(SerialFrame(SerialFrameType.Response, 2u, payload = metadata)) +
                    SerialFrameCodec.encode(SerialFrame(SerialFrameType.Data, 2u, payload = body)) +
                    SerialFrameCodec.encode(SerialFrame(SerialFrameType.End, 2u)),
            ))
            assertEquals("RSVP-Nano", api.fetchDevice("usb://active").ssid)
            api.release()
            assertEquals(1, disconnects, "Intentional release must not report a disconnect")
        }
    }

    @Test
    fun writeFailureAndRequestCancellationReleaseTheirSession() = runTest {
        withContext(Dispatchers.Default.limitedParallelism(1)) {
            val greeting = Base64.encode("RSVPNANO/COMPANION/1 READY\n".encodeToByteArray())
            installFakeSerial(greeting)
            val api = WebSerialNanoApi()
            var disconnects = 0
            api.open(onDisconnect = { disconnects++ })
            failFakeSerialWrites()
            assertFailsWith<NanoClientError> { api.fetchDevice("usb://active") }
            assertEquals(1, fakeSerialCloseCount())
            assertEquals(1, disconnects)

            queueFakeSerialRead(greeting)
            api.open(onDisconnect = { disconnects++ })
            assertFailsWith<CancellationException> {
                withTimeout(50) { api.fetchDevice("usb://active") }
            }
            assertEquals(2, fakeSerialCloseCount())
            assertEquals(2, disconnects)
        }
    }

    @Test
    fun largeUploadKeepsChunkBoundariesAndApplicationErrorsKeepSessionOpen() = runTest {
        withContext(Dispatchers.Default.limitedParallelism(1)) {
            val body = ByteArray(2 * SerialChunkBytes + 17) { it.toByte() }
            val errorBody = """{"code":"already_exists","message":"Theme already exists"}""".encodeToByteArray()
            val metadata = """{"status":409,"totalBytes":${errorBody.size}}""".encodeToByteArray()
            val response = (0u..2u).flatMap { sequence ->
                SerialFrameCodec.encode(SerialFrame(SerialFrameType.Acknowledgement, 1u, sequence)).asList()
            }.toByteArray() +
                SerialFrameCodec.encode(SerialFrame(SerialFrameType.Response, 1u, payload = metadata)) +
                SerialFrameCodec.encode(SerialFrame(SerialFrameType.Data, 1u, payload = errorBody)) +
                SerialFrameCodec.encode(SerialFrame(SerialFrameType.End, 1u))
            installFakeSerial(Base64.encode("RSVPNANO/COMPANION/1 READY\n".encodeToByteArray()) + "|" + Base64.encode(response))
            val api = WebSerialNanoApi()
            var disconnects = 0
            api.open(onDisconnect = { disconnects++ })
            val progress = mutableListOf<Long>()
            val error = assertFailsWith<NanoClientError> {
                api.uploadTheme("usb://active", "existing.toml", body) { sent, _ -> progress += sent }
            }
            assertEquals(409, error.status)
            assertEquals(listOf(4096L, 8192L, body.size.toLong()), progress)
            val chunks = fakeSerialFrames().filter { it.type == SerialFrameType.Data }
            assertEquals(listOf(4096, 4096, 17), chunks.map { it.payload.size })
            assertTrue(chunks.flatMap { it.payload.asList() }.toByteArray().contentEquals(body))
            assertEquals(0, disconnects)
            assertEquals(0, fakeSerialCloseCount())
            api.release()
        }
    }

    @Test
    fun cancellingStalledWriteAbortsStreamBeforeClosingPort() = runTest {
        withContext(Dispatchers.Default.limitedParallelism(1)) {
            installFakeSerial(Base64.encode("RSVPNANO/COMPANION/1 READY\n".encodeToByteArray()))
            val api = WebSerialNanoApi()
            api.open()
            stallFakeSerialWrites()
            assertFailsWith<CancellationException> {
                withTimeout(50) { api.fetchDevice("usb://active") }
            }
            assertEquals(1, fakeSerialAbortCount())
            assertEquals(1, fakeSerialCloseCount())
        }
    }

    @Test
    fun bootloaderRestartUsesNative1200BaudTouch() = runTest {
        installBootloaderResetSerial(openFails = false)
        restartInBootloader()

        assertEquals("1200|1|0", bootloaderResetState())
    }

    @Test
    fun bootloaderRestartAcceptsDisconnectDuringOpen() = runTest {
        installBootloaderResetSerial(openFails = true)
        restartInBootloader()

        assertEquals("1200|0|0", bootloaderResetState())
    }

    @Test
    fun companionConnectionReleasesPortForInstaller() = runTest {
        withContext(Dispatchers.Default.limitedParallelism(1)) {
            val deviceBody = """{"ssid":"RSVP-Nano","firmwareVersion":"0.0.9","otaAsset":"nano-ota.bin"}""".encodeToByteArray()
            val responseMetadata = """{"status":200,"contentType":"application/json","totalBytes":${deviceBody.size}}"""
            val response =
                SerialFrameCodec.encode(SerialFrame(SerialFrameType.Response, 1u, payload = responseMetadata.encodeToByteArray())) +
                    SerialFrameCodec.encode(SerialFrame(SerialFrameType.Data, 1u, payload = deviceBody)) +
                    SerialFrameCodec.encode(SerialFrame(SerialFrameType.End, 1u))
            val repairBody = """{"healthy":true,"checked":8,"moved":1,"removed":0,"diagnosticSummary":"Storage OK","diagnosticDetail":"FAT32","actions":[],"issues":[]}""".encodeToByteArray()
            val repairMetadata = """{"status":200,"contentType":"application/json","totalBytes":${repairBody.size}}"""
            val repairResponse =
                SerialFrameCodec.encode(SerialFrame(SerialFrameType.Response, 2u, payload = repairMetadata.encodeToByteArray())) +
                    SerialFrameCodec.encode(SerialFrame(SerialFrameType.Data, 2u, payload = repairBody)) +
                    SerialFrameCodec.encode(SerialFrame(SerialFrameType.End, 2u))
            val reads = listOf("RSVPNANO/COMPANION/1 READY\n".encodeToByteArray(), response, repairResponse)
                .joinToString("|") { Base64.encode(it) }
            installFakeSerial(reads)
            val api = WebSerialNanoApi()

            assertTrue(api.open())
            assertFalse(installerCanOpenFakeSerial())

            val device = api.fetchDevice("usb://active")
            assertEquals("RSVP-Nano", device.ssid)
            assertEquals("0.0.9", device.firmwareVersion)
            assertEquals(1, api.repairStorage("usb://active").moved)

            api.release()

            assertTrue(installerCanOpenFakeSerial())
            assertEquals(2, fakeSerialOpenCount())
            assertEquals(2, fakeSerialCloseCount())
            assertEquals(SerialFrameType.Close, fakeSerialFrames().last().type)
        }
    }

    private suspend fun restartInBootloader() {
        suspendCancellableCoroutine { continuation ->
            restartNanoInBootloader(
                { if (continuation.isActive) continuation.resume(Unit) },
                { message -> if (continuation.isActive) continuation.resumeWithException(IllegalStateException(message)) },
            )
        }
    }
}

@JsFun("""(openFails) => { const state = { baudRate: 0, closes: 0, signals: 0 }; const listeners = new Set(); let port; const emitDisconnect = () => listeners.forEach(listener => listener({ target: port, port })); const serial = { requestPort: async () => port, addEventListener: (type, listener) => { if (type === 'disconnect') listeners.add(listener); }, removeEventListener: (type, listener) => { if (type === 'disconnect') listeners.delete(listener); } }; port = { getInfo: () => ({ usbVendorId: 0x303a, usbProductId: 0x1001 }), open: async options => { state.baudRate = options.baudRate; if (openFails) { setTimeout(emitDisconnect, 0); throw new Error('Failed to open port'); } }, close: async () => { state.closes++; emitDisconnect(); }, setSignals: async () => { state.signals++; } }; globalThis.rsvpNanoBootloaderReset = state; Object.defineProperty(navigator, 'serial', { configurable: true, value: serial }); }""")
private external fun installBootloaderResetSerial(openFails: Boolean)

@JsFun("""() => { const state = globalThis.rsvpNanoBootloaderReset; return state.baudRate + '|' + state.closes + '|' + state.signals; }""")
private external fun bootloaderResetState(): String

@JsFun("""(encodedReads) => { const decode = encoded => { const text = atob(encoded); const bytes = new Uint8Array(text.length); for (let i = 0; i < text.length; i++) bytes[i] = text.charCodeAt(i); return bytes; }; const reads = encodedReads.split('|').map(decode); const state = { opened: false, opens: 0, closes: 0, reads, writes: [], pendingRead: null }; const port = { getInfo: () => ({ usbVendorId: 0x303a, usbProductId: 0x1001 }), open: async () => { if (state.opened) throw new Error('Port is already open'); state.opened = true; state.opens++; }, close: async () => { state.opened = false; state.closes++; }, readable: { getReader: () => ({ read: async () => state.reads.length ? { value: state.reads.shift(), done: false } : new Promise(resolve => { state.pendingRead = resolve; }), cancel: async () => { state.pendingRead?.({ done: true }); state.pendingRead = null; }, releaseLock: () => {} }) }, writable: { getWriter: () => ({ write: async data => { state.writes.push(new Uint8Array(data)); }, close: async () => {}, releaseLock: () => {} }) } }; state.port = port; globalThis.rsvpNanoFakeSerial = state; Object.defineProperty(navigator, 'serial', { configurable: true, value: { getPorts: async () => [port], requestPort: async () => port } }); localStorage.removeItem('rsvpnano.web.usbDevice'); }""")
private external fun installFakeSerial(encodedReads: String)

@JsFun("""() => { const state = globalThis.rsvpNanoFakeSerial; const write = globalThis.rsvpNanoSerial.writer.write; globalThis.rsvpNanoSerial.writer.write = async data => { await write(data); if (data[5] === 3) { state.pendingRead?.({ done: true }); state.pendingRead = null; } }; }""")
private external fun disconnectFakeSerialAfterEnd()

@JsFun("""() => { globalThis.rsvpNanoSerial.writer.write = async () => { throw new Error('Port has been closed'); }; }""")
private external fun failFakeSerialWrites()

@JsFun("""() => { const state = globalThis.rsvpNanoFakeSerial; let rejectWrite; state.aborts = 0; const stream = new WritableStream({ start: controller => { controller.signal.addEventListener('abort', () => rejectWrite?.(new Error('Write aborted'))); }, write: () => new Promise((resolve, reject) => { rejectWrite = reject; }), abort: () => { state.aborts++; } }); globalThis.rsvpNanoSerial.writer.releaseLock(); globalThis.rsvpNanoSerial.writer = stream.getWriter(); }""")
private external fun stallFakeSerialWrites()

@JsFun("""() => globalThis.rsvpNanoFakeSerial.aborts""")
private external fun fakeSerialAbortCount(): Int

@JsFun("""(encoded) => { const state = globalThis.rsvpNanoFakeSerial; const text = atob(encoded); const bytes = Uint8Array.from(text, c => c.charCodeAt(0)); if (state.pendingRead) { const resolve = state.pendingRead; state.pendingRead = null; resolve({ value: bytes, done: false }); } else state.reads.push(bytes); }""")
private external fun queueFakeSerialRead(encoded: String)

private suspend fun installerCanOpenFakeSerial(): Boolean = suspendCancellableCoroutine { continuation ->
    tryOpenFakeSerial { opened ->
        if (continuation.isActive) continuation.resume(opened)
    }
}

@JsFun("""(done) => { (async () => { const state = globalThis.rsvpNanoFakeSerial; try { await state.port.open({ baudRate: 115200 }); await state.port.close(); done(true); } catch (_) { done(false); } })(); }""")
private external fun tryOpenFakeSerial(done: (Boolean) -> Unit)

@JsFun("""() => globalThis.rsvpNanoFakeSerial.opens""")
private external fun fakeSerialOpenCount(): Int

@JsFun("""() => globalThis.rsvpNanoFakeSerial.closes""")
private external fun fakeSerialCloseCount(): Int

private fun fakeSerialFrames(): List<SerialFrame> {
    val decoder = SerialFrameCodec.Decoder()
    return fakeSerialWrites()
        .split('|')
        .filter { it.isNotEmpty() }
        .flatMap { decoder.feed(Base64.decode(it)) }
}

@JsFun("""() => globalThis.rsvpNanoFakeSerial.writes.map(bytes => { let text = ''; for (let i = 0; i < bytes.length; i++) text += String.fromCharCode(bytes[i]); return btoa(text); }).join('|')""")
private external fun fakeSerialWrites(): String
