package com.rsvpnano.converters

internal object EpubZipReader {
    fun readEntries(data: ByteArray): Map<String, ByteArray> = ZipArchiveReader.readEntries(data)
}
