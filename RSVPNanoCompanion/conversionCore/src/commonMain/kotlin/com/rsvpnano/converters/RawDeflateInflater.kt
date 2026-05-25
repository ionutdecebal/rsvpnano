package com.rsvpnano.converters

import korlibs.io.compression.deflate.Deflate
import korlibs.io.compression.uncompress

internal object RawDeflateInflater {
    fun inflate(data: ByteArray, expectedSize: Int): ByteArray {
        return try {
            val output = Deflate.uncompress(data, expectedSize)
            if (output.size != expectedSize) {
                throw RsvpConversionError.unsupportedEpub
            }
            output
        } catch (error: RsvpConversionError) {
            throw error
        } catch (_: Throwable) {
            throw RsvpConversionError.unsupportedEpub
        }
    }
}
