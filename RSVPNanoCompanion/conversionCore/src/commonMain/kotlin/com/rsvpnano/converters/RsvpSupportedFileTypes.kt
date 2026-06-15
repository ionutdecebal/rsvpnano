package com.rsvpnano.converters

object RsvpSupportedFileTypes {
    private val convertibleExtensions = setOf(
        ".epub",
        ".html",
        ".htm",
        ".xhtml",
        ".md",
        ".markdown",
        ".txt",
    )

    private val textLikeExtensions = setOf(
        ".html",
        ".htm",
        ".xhtml",
        ".md",
        ".markdown",
        ".txt",
    )

    private val uploadPassthroughExtensions = setOf(".rsvp")

    fun extensionFor(filename: String): String {
        val base = filename.substringAfterLast('/').substringAfterLast('\\')
        val index = base.lastIndexOf('.')
        return if (index >= 0) base.substring(index).lowercase() else ""
    }

    fun isRsvp(filename: String): Boolean = extensionFor(filename) == ".rsvp"

    fun isEpub(filename: String): Boolean = extensionFor(filename) == ".epub"

    fun isTextLike(filename: String): Boolean = extensionFor(filename) in textLikeExtensions

    fun isConvertible(filename: String): Boolean = extensionFor(filename) in convertibleExtensions

    fun isUploadPassthrough(filename: String): Boolean = extensionFor(filename) in uploadPassthroughExtensions
}
