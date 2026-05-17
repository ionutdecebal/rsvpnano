package com.rsvpnano.converters

internal actual object PlatformTextDecoder {
    override fun decode(data: ByteArray, charsetName: String): String? {
        return when (charsetName.lowercase()) {
            "utf-8", "utf8" -> String(data, Charsets.UTF_8)
            "utf-16", "utf16" -> String(data, Charsets.UTF_16)
            "utf-16le" -> String(data, Charsets.UTF_16LE)
            "utf-16be" -> String(data, Charsets.UTF_16BE)
            "windows-1252", "cp1252", "iso-8859-1", "latin1" -> null
            else -> null
        }
    }
}
