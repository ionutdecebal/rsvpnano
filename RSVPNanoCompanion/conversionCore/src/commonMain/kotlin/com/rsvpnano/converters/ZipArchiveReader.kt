package com.rsvpnano.converters

internal object ZipArchiveReader {
    private const val LOCAL_FILE_HEADER = 0x04034b50
    private const val CENTRAL_DIRECTORY_HEADER = 0x02014b50
    private const val END_OF_CENTRAL_DIRECTORY = 0x06054b50
    private const val METHOD_STORED = 0
    private const val METHOD_DEFLATED = 8

    fun readEntries(data: ByteArray): Map<String, ByteArray> {
        val directoryOffset = centralDirectoryOffset(data)
        val entries = linkedMapOf<String, ByteArray>()
        var offset = directoryOffset

        while (offset + 46 <= data.size && data.readIntLe(offset) == CENTRAL_DIRECTORY_HEADER) {
            val method = data.readShortLe(offset + 10)
            val compressedSize = data.readUIntLe(offset + 20)
            val uncompressedSize = data.readUIntLe(offset + 24)
            val nameLength = data.readShortLe(offset + 28)
            val extraLength = data.readShortLe(offset + 30)
            val commentLength = data.readShortLe(offset + 32)
            val localHeaderOffset = data.readUIntLe(offset + 42)
            val nameStart = offset + 46
            val nameEnd = nameStart + nameLength

            if (nameEnd > data.size) {
                throw RsvpConversionError.unsupportedEpub
            }

            val name = data.decodeUtf8Slice(nameStart, nameEnd)
            if (!name.endsWith('/')) {
                entries[EpubUtils.normalizeZipPath(name).lowercase()] = readEntry(
                    data = data,
                    localHeaderOffset = localHeaderOffset,
                    method = method,
                    compressedSize = compressedSize,
                    uncompressedSize = uncompressedSize,
                )
            }

            offset = nameEnd + extraLength + commentLength
        }

        if (entries.isEmpty()) {
            throw RsvpConversionError.unsupportedEpub
        }
        return entries
    }

    private fun readEntry(
        data: ByteArray,
        localHeaderOffset: Int,
        method: Int,
        compressedSize: Int,
        uncompressedSize: Int,
    ): ByteArray {
        if (localHeaderOffset + 30 > data.size || data.readIntLe(localHeaderOffset) != LOCAL_FILE_HEADER) {
            throw RsvpConversionError.unsupportedEpub
        }

        val nameLength = data.readShortLe(localHeaderOffset + 26)
        val extraLength = data.readShortLe(localHeaderOffset + 28)
        val contentStart = localHeaderOffset + 30 + nameLength + extraLength
        val contentEnd = contentStart + compressedSize
        if (contentStart < 0 || contentEnd < contentStart || contentEnd > data.size) {
            throw RsvpConversionError.unsupportedEpub
        }

        val compressed = data.copyOfRange(contentStart, contentEnd)
        return when (method) {
            METHOD_STORED -> compressed
            METHOD_DEFLATED -> RawDeflateInflater.inflate(compressed, uncompressedSize)
            else -> throw RsvpConversionError.unsupportedEpub
        }
    }

    private fun centralDirectoryOffset(data: ByteArray): Int {
        val minimumEocdSize = 22
        val maxCommentSize = 65_535
        val minOffset = maxOf(0, data.size - minimumEocdSize - maxCommentSize)
        var offset = data.size - minimumEocdSize

        while (offset >= minOffset) {
            if (data.readIntLe(offset) == END_OF_CENTRAL_DIRECTORY) {
                val commentLength = data.readShortLe(offset + 20)
                if (offset + minimumEocdSize + commentLength == data.size) {
                    return data.readUIntLe(offset + 16)
                }
            }
            offset -= 1
        }

        throw RsvpConversionError.unsupportedEpub
    }

    private fun ByteArray.readShortLe(offset: Int): Int {
        if (offset < 0 || offset + 2 > size) {
            throw RsvpConversionError.unsupportedEpub
        }
        return (this[offset].toInt() and 0xff) or
            ((this[offset + 1].toInt() and 0xff) shl 8)
    }

    private fun ByteArray.readIntLe(offset: Int): Int {
        if (offset < 0 || offset + 4 > size) {
            throw RsvpConversionError.unsupportedEpub
        }
        return (this[offset].toInt() and 0xff) or
            ((this[offset + 1].toInt() and 0xff) shl 8) or
            ((this[offset + 2].toInt() and 0xff) shl 16) or
            ((this[offset + 3].toInt() and 0xff) shl 24)
    }

    private fun ByteArray.readUIntLe(offset: Int): Int {
        val value = readIntLe(offset).toLong() and 0xffff_ffffL
        if (value > Int.MAX_VALUE) {
            throw RsvpConversionError.unsupportedEpub
        }
        return value.toInt()
    }

    private fun ByteArray.decodeUtf8Slice(start: Int, end: Int): String {
        if (start < 0 || end < start || end > size) {
            throw RsvpConversionError.unsupportedEpub
        }
        return copyOfRange(start, end).decodeToString()
    }
}
