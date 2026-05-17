package com.rsvpnano.sync

internal object RssFeedNormalizer {
    fun normalize(feeds: List<String>): List<String> {
        val seen = linkedSetOf<String>()
        feeds.asSequence()
            .map { it.trim() }
            .filter { it.isNotEmpty() }
            .filter { it.startsWith("http://") || it.startsWith("https://") }
            .forEach { seen += it }
        return seen.toList()
    }
}
