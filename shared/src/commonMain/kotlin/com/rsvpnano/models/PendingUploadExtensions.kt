package com.rsvpnano.models

/**
 * Shared helper that mirrors the Swift `needsArticleFetch` behavior.
 */
fun PendingUpload.needsArticleFetch(): Boolean {
    val source = sourceUrl?.trim().orEmpty()
    if (!(source.startsWith("http://") || source.startsWith("https://"))) {
        return false
    }
    return body.trim() == source
}
