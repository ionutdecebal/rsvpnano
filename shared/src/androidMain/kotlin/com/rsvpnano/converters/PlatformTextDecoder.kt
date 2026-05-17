package com.rsvpnano.converters

import java.nio.charset.Charset

internal actual object PlatformTextDecoder {
    override fun decode(data: ByteArray, charsetName: String): String? {
        return try {
            String(data, Charset.forName(charsetName))
        } catch (_: Exception) {
            null
        }
    }
}
