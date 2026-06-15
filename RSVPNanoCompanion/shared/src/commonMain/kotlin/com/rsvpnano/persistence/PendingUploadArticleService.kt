package com.rsvpnano.persistence

import com.rsvpnano.converters.ArticleFormatter
import com.rsvpnano.converters.RsvpBookFile
import com.rsvpnano.converters.RsvpConverter
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.models.needsArticleFetch

/**
 * Shared article-processing logic for pending uploads.
 *
 * This keeps the draft-to-article conversion in one place so Swift and Compose can both reuse it.
 */
class PendingUploadArticleService {
    fun needsArticleFetch(item: PendingUpload): Boolean {
        return item.needsArticleFetch()
    }

    fun articleFor(item: PendingUpload) =
        ArticleFormatter.article(
            title = item.title,
            source = item.sourceUrl.orEmpty(),
            htmlOrText = item.body,
        )

    fun bookFileFor(item: PendingUpload): RsvpBookFile {
        val article = articleFor(item)
        return RsvpConverter.rsvpFile(
            title = article.title,
            author = "",
            source = article.source,
            events = ArticleFormatter.events(article),
        )
    }
}
