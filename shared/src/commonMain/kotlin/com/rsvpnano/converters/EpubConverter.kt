package com.rsvpnano.converters

object EpubConverter {
    fun convert(data: ByteArray, filename: String): RsvpBookFile {
        return PlatformEpubConverter.convert(data, filename)
    }
}
