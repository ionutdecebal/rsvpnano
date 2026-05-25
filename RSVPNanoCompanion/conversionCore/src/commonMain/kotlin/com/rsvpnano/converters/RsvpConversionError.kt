package com.rsvpnano.converters

class RsvpConversionError private constructor(message: String) : IllegalArgumentException(message) {
    companion object {
        val emptyText = RsvpConversionError("There is no readable text to convert.")
        val unreadableText = RsvpConversionError("This file is not readable as text yet.")
        val unsupportedEpub = RsvpConversionError("This EPUB could not be converted locally.")
    }
}
