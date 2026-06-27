package com.rsvpnano.converters

internal expect object PlatformTextDecoder {
    fun decode(data: ByteArray, charsetName: String): String?
}
