package com.rsvpnano.converters

internal actual object PlatformTextDecoder {
    actual fun decode(data: ByteArray, charsetName: String): String? {
        if (data.isEmpty()) {
            return ""
        }

        return when (charsetName.lowercase()) {
            "utf-8", "utf8" -> data.decodeToString()
            "utf-16", "utf16" -> decodeUtf16(data)
            "utf-16le" -> decodeUtf16LittleEndian(data)
            "utf-16be" -> decodeUtf16BigEndian(data)
            "iso-8859-1", "latin1" -> decodeLatin1(data)
            "windows-1252", "cp1252" -> decodeWindows1252(data)
            else -> null
        }
    }

    private fun decodeUtf16(data: ByteArray): String {
        if (data.size >= 2) {
            val first = data[0].toUnsignedInt()
            val second = data[1].toUnsignedInt()
            if (first == 0xfe && second == 0xff) {
                return decodeUtf16BigEndian(data, offset = 2)
            }
            if (first == 0xff && second == 0xfe) {
                return decodeUtf16LittleEndian(data, offset = 2)
            }
        }
        return decodeUtf16BigEndian(data)
    }

    private fun decodeUtf16LittleEndian(data: ByteArray, offset: Int = 0): String = decodeUtf16Units(data, offset) { index ->
        data[index].toUnsignedInt() or (data[index + 1].toUnsignedInt() shl 8)
    }

    private fun decodeUtf16BigEndian(data: ByteArray, offset: Int = 0): String = decodeUtf16Units(data, offset) { index ->
        (data[index].toUnsignedInt() shl 8) or data[index + 1].toUnsignedInt()
    }

    private fun decodeUtf16Units(
        data: ByteArray,
        offset: Int,
        unitAt: (Int) -> Int,
    ): String = buildString((data.size - offset) / 2) {
        var index = offset
        while (index + 1 < data.size) {
            append(unitAt(index).toChar())
            index += 2
        }
    }

    private fun decodeLatin1(data: ByteArray): String = buildString(data.size) {
        data.forEach { byte -> append(byte.toUnsignedInt().toChar()) }
    }

    private fun decodeWindows1252(data: ByteArray): String = buildString(data.size) {
        data.forEach { byte ->
            val value = byte.toUnsignedInt()
            append(windows1252Controls[value] ?: value.toChar())
        }
    }

    private fun Byte.toUnsignedInt(): Int = toInt() and 0xff

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
