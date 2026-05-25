package com.rsvpnano.converters

import com.fleeksoft.ksoup.Ksoup
import com.fleeksoft.ksoup.nodes.Element
import com.fleeksoft.ksoup.parser.Parser

object ArticleFormatter {
    private val noiseTags = setOf("script", "style", "svg", "nav", "header", "footer", "aside", "form", "noscript")

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
        val document = parseHtml(from)
        document.allElements()
            .filter { it.localName() in noiseTags }
            .forEach { it.remove() }

        return listOf("article", "main", "body")
            .firstNotNullOfOrNull { tag -> document.allElements().firstOrNull { it.localName() == tag } }
            ?.html()
            ?: document.html()
    }

    private fun fallbackTitle(from: String): String {
        val withoutScheme = from.substringAfter("//", missingDelimiterValue = from)
        val sourceLabel = withoutScheme
            .substringBefore("?")
            .substringBefore("#")
            .trim('/')
        if (sourceLabel.isNotBlank()) {
            return sourceLabel
        }

        val host = withoutScheme
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
            return fallbackTitle(from = source)
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
        val cleaned = RsvpTextUtils.cleanedLine(parseHtml(from).selectFirst("title")?.text().orEmpty())
        return cleaned.takeIf { it.isNotEmpty() }?.take(120)
    }

    private fun parseHtml(markup: String) = Ksoup.parse(markup, Parser.htmlParser(), "")

    private fun Element.allElements(): List<Element> = getAllElements().toList()

    private fun Element.localName(): String = normalName().substringAfter(':').lowercase()
}
