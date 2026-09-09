@file:OptIn(ExperimentalWasmJsInterop::class)

package com.rsvpnano.web

import com.rsvpnano.api.NanoClientError
import com.rsvpnano.api.NanoKtorClient
import kotlinx.coroutines.test.runTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class BrowserHttpClientTest {
    @Test
    fun deviceMutationsUseLocalNetworkPermissionAndKeepStructuredErrors() = runTest {
        installFetchRecorder()
        val device = NanoKtorClient(browserHttpClient(localNetwork = true))
        val internet = NanoKtorClient(browserHttpClient())
        try {
            device.deleteBook("http://reader.lan", "old-book")
            assertEquals("DELETE /api/v2/library/old-book local", recordedFetch())
            val error = assertFailsWith<NanoClientError> {
                device.deleteBook("http://reader.lan", "active-book")
            }
            assertEquals(409, error.status)
            assertEquals("resource_in_use", error.code)
            device.deleteBook("http://reader.lan", "active-book", force = true)
            assertEquals("DELETE /api/v2/library/active-book?force=true local", recordedFetch())
            device.deleteTheme("http://reader.lan", "theme")
            device.deleteFont("http://reader.lan", "font")
            device.deleteLocalePack("http://reader.lan", "locale")
            assertEquals("DELETE /api/v2/locales/locale local", recordedFetch())
            assertEquals("theme", device.uploadTheme("http://reader.lan", "theme.toml", byteArrayOf(1)).id)
            assertEquals("POST /api/v2/themes?name=theme.toml local", recordedFetch())
            internet.downloadTheme("https://example.com/theme.toml")
            assertEquals("GET /theme.toml unset", recordedFetch())
        } finally {
            device.close()
            internet.close()
            restoreFetch()
        }
    }
}

@JsFun("""() => {
    globalThis.rsvpSavedFetch = globalThis.fetch;
    globalThis.fetch = async (url, options) => {
        const parsed = new URL(url);
        const path = parsed.pathname + parsed.search;
        globalThis.rsvpRecordedFetch = options.method + ' ' + path + ' ' + (options.targetAddressSpace || 'unset');
        if (path.endsWith('/active-book')) return new Response(JSON.stringify({code: 'resource_in_use', message: 'Close the active book before removing it', field: 'id'}), {status: 409});
        if (options.method === 'DELETE') return new Response(null, {status: 204});
        return new Response(JSON.stringify({id: 'theme', name: 'Theme'}), {status: 200});
    };
}""")
private external fun installFetchRecorder()

@JsFun("() => globalThis.rsvpRecordedFetch")
private external fun recordedFetch(): String

@JsFun("() => { globalThis.fetch = globalThis.rsvpSavedFetch; delete globalThis.rsvpSavedFetch; delete globalThis.rsvpRecordedFetch; }")
private external fun restoreFetch()
