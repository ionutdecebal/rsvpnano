package com.rsvpnano.converters

import com.fleeksoft.ksoup.Ksoup
import com.fleeksoft.ksoup.nodes.Document
import com.fleeksoft.ksoup.nodes.Element
import com.fleeksoft.ksoup.nodes.Node
import com.fleeksoft.ksoup.nodes.TextNode
import com.fleeksoft.ksoup.parser.Parser

internal object EpubBookConverter {
    private val blockTags = setOf(
        "address", "article", "aside", "blockquote", "body", "br", "dd", "div", "dl", "dt",
        "figcaption", "figure", "footer", "header", "hr", "li", "main", "ol", "p", "pre",
        "section", "table", "tbody", "td", "tfoot", "th", "thead", "tr", "ul",
    )

    private val skipTags = setOf("head", "math", "nav", "script", "style", "svg")

    fun convert(entries: Map<String, ByteArray>, filename: String): RsvpBookFile {
        val normalizedEntries = entries.mapKeys { (path, _) -> EpubUtils.normalizeZipPath(path).lowercase() }
        if (normalizedEntries.containsKey("meta-inf/encryption.xml")) {
            throw RsvpConversionError.unsupportedEpub
        }
        val containerXml = normalizedEntries["meta-inf/container.xml"]
            ?.let(RsvpTextUtils::decodeText)
            ?: throw RsvpConversionError.unsupportedEpub
        val opfPath = containerRootfile(containerXml) ?: throw RsvpConversionError.unsupportedEpub
        val packageXml = normalizedEntries[EpubUtils.normalizeZipPath(opfPath).lowercase()]
            ?.let(RsvpTextUtils::decodeText)
            ?: throw RsvpConversionError.unsupportedEpub

        val packageInfo = parsePackage(packageXml, opfPath, normalizedEntries)
        val paths = packageInfo.spinePaths.ifEmpty { packageInfo.manifestContentPaths }
        if (paths.isEmpty()) {
            throw RsvpConversionError.unsupportedEpub
        }

        val events = mutableListOf<RsvpEvent>()
        paths.forEachIndexed { index, spinePath ->
            val chapterData = normalizedEntries[EpubUtils.normalizeZipPath(spinePath).lowercase()] ?: return@forEachIndexed
            val rawMarkup = RsvpTextUtils.decodeText(chapterData) ?: return@forEachIndexed
            val tocEntries = packageInfo.tocTitlesByPath[EpubUtils.normalizeZipPath(spinePath).lowercase()].orEmpty()
            val chapterEvents = htmlEvents(rawMarkup, tocEntries).toMutableList()
            if (tocEntries.isNotEmpty()) {
                chapterEvents.applyTocTitles(tocEntries.map { it.title }, packageInfo.title)
            } else if (packageInfo.tocTitlesByPath.isNotEmpty()) {
                if (chapterEvents.isGeneratedTableOfContents() || chapterEvents.isBookTitlePage(packageInfo.title)) {
                    return@forEachIndexed
                }
            } else if (packageInfo.tocTitlesByPath.isEmpty() && chapterEvents.none { it is RsvpEvent.Chapter }) {
                chapterEvents.add(0, RsvpEvent.Chapter(EpubUtils.fallbackChapterTitle(spinePath, index + 1)))
            }
            if (chapterEvents.any { it is RsvpEvent.Text }) {
                events += chapterEvents
            }
        }

        if (events.isEmpty()) {
            throw RsvpConversionError.unsupportedEpub
        }

        return RsvpConverter.rsvpFile(
            title = packageInfo.title.ifBlank { RsvpConverter.filenameWithoutExtension(filename) },
            author = packageInfo.author,
            source = filename,
            events = events,
        )
    }

    private fun containerRootfile(xml: String): String? {
        return parseXml(xml).allElements()
            .firstOrNull { it.localName() == "rootfile" }
            ?.attr("full-path")
            ?.takeIf { it.isNotBlank() }
    }

    private fun parsePackage(xml: String, opfPath: String, entries: Map<String, ByteArray>): EpubPackage {
        val document = parseXml(xml)
        val manifest = linkedMapOf<String, EpubManifestItem>()
        val manifestContentPaths = mutableListOf<String>()
        val spinePaths = mutableListOf<String>()
        val navPaths = mutableListOf<String>()
        val ncxPaths = mutableListOf<String>()

        document.allElements().filter { it.localName() == "item" }.forEach { itemElement ->
            val id = itemElement.attr("id")
            val href = itemElement.attr("href")
            if (id.isBlank() || href.isBlank()) return@forEach

            val mediaType = itemElement.attr("media-type")
            val path = EpubUtils.zipJoin(opfPath, href)
            val item = EpubManifestItem(path = path, mediaType = mediaType)
            manifest[id] = item
            if (isContentDocument(path, mediaType)) {
                manifestContentPaths += path
            }
            if (mediaType.equals("application/x-dtbncx+xml", ignoreCase = true)) {
                ncxPaths += path
            }
            if (itemElement.attr("properties").split(Regex("\\s+")).any { it == "nav" }) {
                navPaths += path
            }
        }

        document.allElements().filter { it.localName() == "itemref" }.forEach { itemRef ->
            val idref = itemRef.attr("idref")
            val item = manifest[idref] ?: return@forEach
            if (isContentDocument(item.path, item.mediaType)) {
                spinePaths += item.path
            }
        }

        val metadata = document.allElements().firstOrNull { it.localName() == "metadata" }
        val title = metadata?.firstTextByLocalName("title").orEmpty()
        val packageVersion = document.allElements()
            .firstOrNull { it.localName() == "package" }
            ?.attr("version")
            .orEmpty()

        return EpubPackage(
            title = title,
            author = metadata?.firstTextByLocalName("creator").orEmpty(),
            spinePaths = spinePaths,
            manifestContentPaths = manifestContentPaths,
            tocTitlesByPath = tocTitlesByPath(
                entries = entries,
                bookTitle = title,
                packageVersion = packageVersion,
                navPaths = navPaths,
                ncxPaths = ncxPaths,
            ),
        )
    }

    private fun tocTitlesByPath(
        entries: Map<String, ByteArray>,
        bookTitle: String,
        packageVersion: String,
        navPaths: List<String>,
        ncxPaths: List<String>,
    ): Map<String, List<EpubTocEntry>> {
        val primaryPaths = if (packageVersion.startsWith("3")) navPaths else ncxPaths
        val fallbackPaths = if (packageVersion.startsWith("3")) ncxPaths else navPaths
        val primaryParser = if (packageVersion.startsWith("3")) ::htmlNavTocTitles else ::ncxTocTitles
        val fallbackParser = if (packageVersion.startsWith("3")) ::ncxTocTitles else ::htmlNavTocTitles

        primaryPaths.firstNotNullOfOrNull { path ->
            entries[EpubUtils.normalizeZipPath(path).lowercase()]
                ?.let(RsvpTextUtils::decodeText)
                ?.let { primaryParser(it, path, bookTitle) }
                ?.takeIf { it.isNotEmpty() }
        }?.let { return it }

        fallbackPaths.firstNotNullOfOrNull { path ->
            entries[EpubUtils.normalizeZipPath(path).lowercase()]
                ?.let(RsvpTextUtils::decodeText)
                ?.let { fallbackParser(it, path, bookTitle) }
                ?.takeIf { it.isNotEmpty() }
        }?.let { return it }

        return emptyMap()
    }

    private fun ncxTocTitles(xml: String, tocPath: String, bookTitle: String): Map<String, List<EpubTocEntry>> {
        return parseXml(xml).allElements()
            .filter { it.localName() == "navpoint" }
            .mapNotNull { navPoint ->
            val label = navPoint.firstTextByLocalName("text")
                .takeIf { isContentTocTitle(it, bookTitle) }
                ?: return@mapNotNull null
            val src = navPoint.allElements().firstOrNull { it.localName() == "content" }?.attr("src").orEmpty()
            tocPathKey(tocPath, src) to EpubTocEntry(label, tocPathFragment(src))
        }.groupBy({ it.first }, { it.second })
    }

    private fun htmlNavTocTitles(markup: String, tocPath: String, bookTitle: String): Map<String, List<EpubTocEntry>> {
        val document = parseHtml(markup)
        val root = document.allElements().firstOrNull { element ->
            element.localName() == "nav" &&
                element.attr("epub:type").split(Regex("\\s+")).contains("toc") ||
                element.localName() == "nav" &&
                element.attr("type").split(Regex("\\s+")).contains("toc")
        } ?: document

        return root.allElements().filter { it.localName() == "a" }.mapNotNull { anchor ->
            val href = anchor.attr("href")
            val label = RsvpTextUtils.cleanedLine(anchor.text())
                .takeIf { isContentTocTitle(it, bookTitle) } ?: return@mapNotNull null
            tocPathKey(tocPath, href) to EpubTocEntry(label, tocPathFragment(href))
        }.groupBy({ it.first }, { it.second })
    }

    private fun tocPathKey(tocPath: String, href: String): String {
        val withoutAnchor = href.substringBefore('#').substringBefore('?')
        return EpubUtils.normalizeZipPath(EpubUtils.zipJoin(tocPath, withoutAnchor)).lowercase()
    }

    private fun tocPathFragment(href: String): String {
        val fragment = href.substringAfter('#', missingDelimiterValue = "").substringBefore('?')
        return EpubUtils.percentDecode(fragment).trim()
    }

    private fun isContentTocTitle(value: String, bookTitle: String): Boolean {
        val cleaned = RsvpTextUtils.cleanedLine(value)
        val lowered = cleaned.lowercase()
        val normalized = normalizedTocLabel(cleaned)
        val normalizedBookTitle = normalizedTocLabel(bookTitle)
        return cleaned.isNotEmpty() &&
            lowered != "contents" &&
            lowered != "cover" &&
            lowered != "title page" &&
            normalized != "tableofcontents" &&
            (normalizedBookTitle.isEmpty() || normalized != normalizedBookTitle) &&
            (normalizedBookTitle.isEmpty() || !normalizedBookTitle.startsWith(normalized)) &&
            cleaned.any(Char::isLetterOrDigit)
    }

    private fun normalizedTocLabel(value: String): String {
        return RsvpTextUtils.cleanedLine(value)
            .filter(Char::isLetterOrDigit)
            .lowercase()
    }

    private fun isContentDocument(path: String, mediaType: String): Boolean {
        val loweredPath = path.lowercase()
        val loweredType = mediaType.lowercase()
        return loweredType == "application/xhtml+xml" ||
            loweredType == "text/html" ||
            loweredPath.endsWith(".xhtml") ||
            loweredPath.endsWith(".html") ||
            loweredPath.endsWith(".htm")
    }

    private fun htmlEvents(markup: String, tocEntries: List<EpubTocEntry>): List<RsvpEvent> {
        val document = parseHtml(markup)
        val events = mutableListOf<RsvpEvent>()
        val paragraph = mutableListOf<String>()
        val emittedFragments = mutableSetOf<String>()

        fun flushText() {
            val text = RsvpTextUtils.cleanedLine(paragraph.joinToString(" "))
            paragraph.clear()
            if (text.isNotEmpty()) {
                events += RsvpEvent.Text(text)
            }
        }

        fun emitChapter(title: String) {
            val cleaned = RsvpTextUtils.cleanedLine(title)
            if (cleaned.isNotEmpty()) {
                flushText()
                events += RsvpEvent.Chapter(cleaned)
            }
        }

        fun tocEntryFor(element: Element): EpubTocEntry? {
            val ids = listOf(element.attr("id"), element.attr("name"))
                .map { RsvpTextUtils.cleanedLine(it) }
                .filter { it.isNotEmpty() }
            if (ids.isEmpty()) {
                return null
            }
            return tocEntries.firstOrNull { entry ->
                entry.fragment.isNotBlank() &&
                    entry.fragment !in emittedFragments &&
                    ids.any { it == entry.fragment }
            }
        }

        fun visit(node: Node) {
            when (node) {
                is TextNode -> {
                    val text = RsvpTextUtils.cleanedLine(node.getWholeText())
                    if (text.isNotEmpty()) {
                        paragraph += text
                    }
                }
                is Element -> {
                    val tag = node.localName()
                    if (tag in skipTags) {
                        return
                    }
                    if (tag == "br") {
                        flushText()
                        return
                    }

                    val isBlock = tag in blockTags
                    if (isBlock) {
                        flushText()
                    }

                    val tocEntry = tocEntryFor(node)
                    if (tocEntry != null) {
                        emittedFragments += tocEntry.fragment
                        emitChapter(tocEntry.title)
                    }

                    if (tag.matches(Regex("h[1-6]"))) {
                        if (tocEntry == null) {
                            emitChapter(node.text())
                        }
                        return
                    }

                    node.childNodes().forEach(::visit)
                    if (isBlock) {
                        flushText()
                    }
                }
                else -> node.childNodes().forEach(::visit)
            }
        }

        visit(document.body())
        flushText()
        return events
    }

    private fun parseXml(markup: String): Document = Ksoup.parse(markup, Parser.xmlParser(), "")

    private fun parseHtml(markup: String): Document = Ksoup.parse(markup, Parser.htmlParser(), "")

    private fun Element.allElements(): List<Element> = getAllElements().toList()

    private fun Element.localName(): String = normalName().substringAfter(':').lowercase()

    private fun Element.firstTextByLocalName(name: String): String {
        return allElements()
            .firstOrNull { it.localName() == name }
            ?.text()
            ?.let(RsvpTextUtils::cleanedLine)
            .orEmpty()
    }

    private data class EpubManifestItem(val path: String, val mediaType: String)

    private data class EpubTocEntry(val title: String, val fragment: String)

    private data class EpubPackage(
        val title: String,
        val author: String,
        val spinePaths: List<String>,
        val manifestContentPaths: List<String>,
        val tocTitlesByPath: Map<String, List<EpubTocEntry>>,
    )

    private fun MutableList<RsvpEvent>.applyTocTitles(tocTitles: List<String>, bookTitle: String) {
        removeChapterMatching(bookTitle)

        if (tocTitles.size == 1) {
            removeChapterMatching(tocTitles.first())
            removeFirstChapterPrefixOf(tocTitles.first())
            removeFirstChapter()
            add(0, RsvpEvent.Chapter(tocTitles.first()))
            removeChaptersAfterFirst()
            return
        }

        var tocIndex = 0
        val iterator = listIterator()
        while (iterator.hasNext()) {
            val event = iterator.next()
            if (event !is RsvpEvent.Chapter) {
                continue
            }
            if (tocIndex < tocTitles.size) {
                iterator.set(RsvpEvent.Chapter(tocTitles[tocIndex]))
                tocIndex += 1
            } else {
                iterator.remove()
            }
        }
        while (tocIndex < tocTitles.size) {
            add(RsvpEvent.Chapter(tocTitles[tocIndex]))
            tocIndex += 1
        }
    }

    private fun MutableList<RsvpEvent>.removeFirstChapter() {
        val index = indexOfFirst { it is RsvpEvent.Chapter }
        if (index >= 0) {
            removeAt(index)
        }
    }

    private fun MutableList<RsvpEvent>.removeFirstChapterMatching(title: String) {
        val normalizedTitle = normalizedChapterTitle(title)
        val index = indexOfFirst { event ->
            event is RsvpEvent.Chapter && normalizedChapterTitle(event.title) == normalizedTitle
        }
        if (index >= 0) {
            removeAt(index)
        }
    }

    private fun MutableList<RsvpEvent>.removeChapterMatching(title: String) {
        val normalizedTitle = normalizedChapterTitle(title)
        val normalizedTocTitle = normalizedTocLabel(title)
        if (normalizedTitle.isEmpty()) {
            return
        }
        removeAll { event ->
            event is RsvpEvent.Chapter &&
                (normalizedChapterTitle(event.title) == normalizedTitle ||
                    normalizedTocLabel(event.title) == normalizedTocTitle)
        }
    }

    private fun MutableList<RsvpEvent>.removeChaptersAfterFirst() {
        var seenFirst = false
        removeAll { event ->
            if (event !is RsvpEvent.Chapter) {
                false
            } else if (!seenFirst) {
                seenFirst = true
                false
            } else {
                true
            }
        }
    }

    private fun List<RsvpEvent>.isGeneratedTableOfContents(): Boolean {
        val firstChapter = firstOrNull { it is RsvpEvent.Chapter } as? RsvpEvent.Chapter ?: return false
        val linkLikeTextCount = filterIsInstance<RsvpEvent.Text>()
            .take(300)
            .count { event -> normalizedChapterTitle(event.text).matches(Regex("[ivxlcdm]+|[ivxlcdm]+\\s+.+")) }
        val textCount = count { it is RsvpEvent.Text }
        return normalizedChapterTitle(firstChapter.title) in setOf("contents", "table of contents") &&
            (linkLikeTextCount >= 20 || textCount >= 4)
    }

    private fun List<RsvpEvent>.isBookTitlePage(bookTitle: String): Boolean {
        val normalizedTitle = normalizedTocLabel(bookTitle)
        val normalizedChapter = filterIsInstance<RsvpEvent.Chapter>()
            .map { normalizedTocLabel(it.title) }
            .firstOrNull { it.isNotEmpty() && normalizedTitle.startsWith(it) }
            .orEmpty()
        val textCount = count { it is RsvpEvent.Text }
        return normalizedTitle.isNotEmpty() &&
            normalizedChapter.isNotEmpty() &&
            normalizedTitle.startsWith(normalizedChapter) &&
            (normalizedTitle == normalizedChapter || normalizedChapter.length >= 8) &&
            textCount <= 25
    }

    private fun MutableList<RsvpEvent>.removeFirstChapterPrefixOf(title: String) {
        val normalizedTitle = normalizedChapterTitle(title)
        val index = indexOfFirst { event ->
            event is RsvpEvent.Chapter && normalizedTitle.startsWith(normalizedChapterTitle(event.title) + " ")
        }
        if (index >= 0) {
            removeAt(index)
        }
    }

    private fun normalizedChapterTitle(value: String): String {
        return RsvpTextUtils.cleanedLine(value).lowercase()
    }
}
