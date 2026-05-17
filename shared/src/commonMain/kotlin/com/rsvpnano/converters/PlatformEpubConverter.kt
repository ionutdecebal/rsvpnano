package com.rsvpnano.converters

internal expect object PlatformEpubConverter {
    fun convert(data: ByteArray, filename: String): RsvpBookFile
}
