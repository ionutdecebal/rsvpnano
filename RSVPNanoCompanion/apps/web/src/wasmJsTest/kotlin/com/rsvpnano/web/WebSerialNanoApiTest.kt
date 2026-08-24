@file:OptIn(ExperimentalWasmJsInterop::class, kotlin.io.encoding.ExperimentalEncodingApi::class)

package com.rsvpnano.web

import kotlin.io.encoding.Base64
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.withContext
import kotlin.coroutines.resume

class WebSerialNanoApiTest {
    @Test
    fun companionConnectionReleasesPortForInstaller() = runTest {
        withContext(Dispatchers.Default.limitedParallelism(1)) {
            val deviceBody = """{"ssid":"RSVP-Nano","firmwareVersion":"0.0.9","otaAsset":"nano-ota.bin"}""".encodeToByteArray()
            val responseMetadata = """{"status":200,"contentType":"application/json","totalBytes":${deviceBody.size}}"""
            val response =
                SerialFrameCodec.encode(SerialFrame(SerialFrameType.Response, 1u, payload = responseMetadata.encodeToByteArray())) +
                    SerialFrameCodec.encode(SerialFrame(SerialFrameType.Data, 1u, payload = deviceBody)) +
                    SerialFrameCodec.encode(SerialFrame(SerialFrameType.End, 1u))
            val reads = listOf("RSVPNANO/COMPANION/1 READY\n".encodeToByteArray(), response)
                .joinToString("|") { Base64.encode(it) }
            installFakeSerial(reads)
            val api = WebSerialNanoApi()

            assertTrue(api.open())
            assertFalse(installerCanOpenFakeSerial())

            val device = api.fetchDevice("usb://active")
            assertEquals("RSVP-Nano", device.ssid)
            assertEquals("0.0.9", device.firmwareVersion)

            api.release()

            assertTrue(installerCanOpenFakeSerial())
            assertEquals(2, fakeSerialOpenCount())
            assertEquals(2, fakeSerialCloseCount())
            assertEquals(SerialFrameType.Close, fakeSerialFrames().last().type)
        }
    }
}

@JsFun("""(encodedReads) => { const decode = encoded => { const text = atob(encoded); const bytes = new Uint8Array(text.length); for (let i = 0; i < text.length; i++) bytes[i] = text.charCodeAt(i); return bytes; }; const reads = encodedReads.split('|').map(decode); const state = { opened: false, opens: 0, closes: 0, reads, writes: [], pendingRead: null }; const port = { getInfo: () => ({ usbVendorId: 0x303a, usbProductId: 0x1001 }), open: async () => { if (state.opened) throw new Error('Port is already open'); state.opened = true; state.opens++; }, close: async () => { state.opened = false; state.closes++; }, readable: { getReader: () => ({ read: async () => state.reads.length ? { value: state.reads.shift(), done: false } : new Promise(resolve => { state.pendingRead = resolve; }), cancel: async () => { state.pendingRead?.({ done: true }); state.pendingRead = null; }, releaseLock: () => {} }) }, writable: { getWriter: () => ({ write: async data => { state.writes.push(new Uint8Array(data)); }, close: async () => {}, releaseLock: () => {} }) } }; state.port = port; globalThis.rsvpNanoFakeSerial = state; Object.defineProperty(navigator, 'serial', { configurable: true, value: { getPorts: async () => [port], requestPort: async () => port } }); localStorage.removeItem('rsvpnano.web.usbDevice'); }""")
private external fun installFakeSerial(encodedReads: String)

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
