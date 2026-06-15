package com.rsvpnano.converters

import java.nio.charset.Charset

internal actual object PlatformTextDecoder {
    actual fun decode(data: ByteArray, charsetName: String): String? {
        return try {
            String(data, Charset.forName(charsetName))
        } catch (_: Exception) {
            null
        }
    }
}
