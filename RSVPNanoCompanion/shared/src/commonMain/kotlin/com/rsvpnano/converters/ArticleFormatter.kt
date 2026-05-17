package com.rsvpnano.converters

object ArticleFormatter {
    fun article(title: String, source: String, htmlOrText: String): SharedArticle {
        val resolvedTitle = articleTitle(title = title, source = source, htmlOrText = htmlOrText)

        return if (RsvpTextUtils.looksLikeHTML(htmlOrText)) {
            val focused = focusedHTML(from = htmlOrText)
            SharedArticle(
                title = resolvedTitle,
                source = source,
                text = RsvpTextUtils.readableText(focused),
            )
        } else {
            SharedArticle(
                title = resolvedTitle,
                source = source,
                text = RsvpTextUtils.readableText(htmlOrText),
            )
        }
    }

    fun events(article: SharedArticle): List<RsvpEvent> {
        return if (RsvpTextUtils.looksLikeHTML(article.text)) {
            RsvpTextUtils.htmlEvents(article.text)
        } else {
            RsvpTextUtils.textEvents(article.text)
        }
    }

    private fun focusedHTML(from: String): String {
        val cleaned = removingBlocks(
            from = from,
            tags = listOf("script", "style", "svg", "nav", "header", "footer", "aside", "form", "noscript"),
        )

        for (tag in listOf("article", "main", "body")) {
            firstElementContent(value = cleaned, tag = tag)?.let { return it }
        }
        return cleaned
    }

    private fun fallbackTitle(from: String): String {
        val host = from.substringAfter("//", missingDelimiterValue = "")
            .substringBefore("/")
            .substringBefore(":")
        return if (host.isBlank()) "Shared Article" else host
    }

    private fun articleTitle(title: String, source: String, htmlOrText: String): String {
        val cleanedTitle = RsvpTextUtils.cleanedLine(title)
        if (cleanedTitle.isNotEmpty() && !isPlaceholderTitle(cleanedTitle, source = source)) {
            return cleanedTitle
        }

        if (RsvpTextUtils.looksLikeHTML(htmlOrText)) {
            htmlTitle(from = htmlOrText)?.let { return it }
        }

        return RsvpTextUtils.titleFromText(htmlOrText, fallback = fallbackTitle(from = source))
    }

    private fun isPlaceholderTitle(title: String, source: String): Boolean {
        val host = source.substringAfter("//", missingDelimiterValue = "")
            .substringBefore("/")
            .substringBefore(":")
        val wwwHost = if (host.isBlank()) "" else "www.$host"
        return title == source || title == host || title == wwwHost
    }

    private fun htmlTitle(from: String): String? {
        val title = firstElementContent(value = from, tag = "title")
        val content = title ?: return null
        val cleaned = RsvpTextUtils.cleanedLine(RsvpTextUtils.readableText(content))
        return cleaned.takeIf { it.isNotEmpty() }?.take(120)
    }

    private fun firstElementContent(value: String, tag: String): String? {
        val regex = Regex("<$tag\\b[^>]*>(.*?)</$tag>", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
        return regex.find(value)?.groupValues?.getOrNull(1)
    }

    private fun removingBlocks(from: String, tags: List<String>): String {
        var result = from
        for (tag in tags) {
            var searchStart = 0
            var removedCount = 0
            while (removedCount < 80) {
                val open = result.indexOf("<$tag", startIndex = searchStart, ignoreCase = true)
                if (open < 0) break
                val close = result.indexOf("</$tag>", startIndex = open, ignoreCase = true)
                if (close < 0) break
                val closeEnd = result.indexOf('>', startIndex = close)
                if (closeEnd < 0) break

                result = result.replaceRange(open, closeEnd + 1, " ")
                searchStart = open
                removedCount += 1
            }
        }
        return result
    }
}
