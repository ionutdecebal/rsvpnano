package com.rsvpnano.converters

internal actual object PlatformTextDecoder {
    actual fun decode(data: ByteArray, charsetName: String): String? {
        return when (charsetName.lowercase()) {
            "utf-8", "utf8" -> String(data, Charsets.UTF_8)
            "utf-16", "utf16" -> String(data, Charsets.UTF_16)
            "utf-16le" -> String(data, Charsets.UTF_16LE)
            "utf-16be" -> String(data, Charsets.UTF_16BE)
            "iso-8859-1", "latin1" -> decodeLatin1(data)
            "windows-1252", "cp1252" -> decodeWindows1252(data)
            else -> null
        }
    }

    private fun decodeLatin1(data: ByteArray): String = buildString(data.size) {
        data.forEach { byte -> append((byte.toInt() and 0xff).toChar()) }
    }

    private fun decodeWindows1252(data: ByteArray): String = buildString(data.size) {
        data.forEach { byte ->
            append(windows1252Controls[byte.toInt() and 0xff] ?: (byte.toInt() and 0xff).toChar())
        }
    }

    private val windows1252Controls: Map<Int, Char> = mapOf(
        0x80 to '\u20ac',
        0x82 to '\u201a',
        0x83 to '\u0192',
        0x84 to '\u201e',
        0x85 to '\u2026',
        0x86 to '\u2020',
        0x87 to '\u2021',
        0x88 to '\u02c6',
        0x89 to '\u2030',
        0x8a to '\u0160',
        0x8b to '\u2039',
        0x8c to '\u0152',
        0x8e to '\u017d',
        0x91 to '\u2018',
        0x92 to '\u2019',
        0x93 to '\u201c',
        0x94 to '\u201d',
        0x95 to '\u2022',
        0x96 to '\u2013',
        0x97 to '\u2014',
        0x98 to '\u02dc',
        0x99 to '\u2122',
        0x9a to '\u0161',
        0x9b to '\u203a',
        0x9c to '\u0153',
        0x9e to '\u017e',
        0x9f to '\u0178',
    )
}
