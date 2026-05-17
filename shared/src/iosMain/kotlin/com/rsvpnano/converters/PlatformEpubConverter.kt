package com.rsvpnano.converters

internal actual object PlatformEpubConverter {
    override fun convert(data: ByteArray, filename: String): RsvpBookFile {
        throw RsvpConversionError.unsupportedEpub
    }
}
