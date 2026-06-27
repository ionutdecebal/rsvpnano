package com.rsvpnano.app

import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoWifiSettings

/**
 * One snapshot of the device state, suitable for populating a SwiftUI or Compose view model.
 */
data class NanoDeviceSnapshot(
    val info: NanoInfo? = null,
    val books: List<NanoBook> = emptyList(),
    val settings: NanoSettings? = null,
    val wifiSettings: NanoWifiSettings? = null,
    val rssFeeds: NanoRssFeeds? = null,
) {
    val summaryText: String
        get() {
            val articleCount = books.count { it.category == "article" || it.id.lowercase().startsWith("articles/") }
            val bookCount = books.count() - articleCount
            val bookLabel = if (bookCount == 1) "book" else "books"
            val articleLabel = if (articleCount == 1) "article" else "articles"
            val knownProgressCount = books.count { it.progressPercent != null }
            val base = "$bookCount $bookLabel · $articleCount $articleLabel"
            return if (knownProgressCount > 0) {
                "$base · $knownProgressCount with saved progress"
            } else {
                base
            }
        }
}
