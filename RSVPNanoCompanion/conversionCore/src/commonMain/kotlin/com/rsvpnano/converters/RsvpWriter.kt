package com.rsvpnano.converters

internal class RsvpWriter(
    title: String,
    author: String,
    source: String,
) {
    private val title = RsvpTextUtils.directiveValue(title).ifEmpty { "Untitled" }
    private val author = RsvpTextUtils.directiveValue(author)
    private val lines = mutableListOf("@rsvp 1", "@title $title")
    private var wordCount = 0
    private var chapterCount = 0
    private var lineWords = mutableListOf<String>()
    private var lineLength = 0
    private var lastChapter = ""

    init {
        if (this.author.isNotEmpty()) {
            lines += "@author ${this.author}"
        }
        val cleanedSource = RsvpTextUtils.directiveValue(source)
        if (cleanedSource.isNotEmpty()) {
            lines += "@source $cleanedSource"
        }
        lines += ""
    }

    fun addChapter(value: String) {
        val chapter = RsvpTextUtils.directiveValue(value)
        if (chapter.isEmpty() || chapter == lastChapter) {
            return
        }
        flushLine()
        if (lines.lastOrNull() != "") {
            lines += ""
        }
        lines += "@chapter $chapter"
        chapterCount += 1
        lastChapter = chapter
    }

    fun beginParagraph() {
        flushLine()
        if (wordCount > 0) {
            if (lines.lastOrNull() != "") {
                lines += ""
            }
            lines += "@para"
        }
    }

    fun addText(text: String) {
        val readableTokens = RsvpTextUtils.cleanWordTokens(text)
        var readableIndex = 0
        RsvpTextUtils.outputTokens(text).forEach { word ->
            val projected = if (lineWords.isEmpty()) word.length else lineLength + 1 + word.length
            if (lineWords.isNotEmpty() && projected > RsvpConverter.wrapWidth) {
                flushLine()
            }
            lineWords += word
            lineLength = if (lineWords.size == 1) word.length else lineLength + 1 + word.length
            if (readableIndex < readableTokens.size && word == readableTokens[readableIndex]) {
                wordCount += 1
                readableIndex += 1
            }
        }
    }

    fun finalize(fallbackChapterTitle: String): RsvpBookFile {
        flushLine()
        if (wordCount == 0) {
            throw RsvpConversionError.emptyText
        }
        if (chapterCount == 0) {
            val chapter = "@chapter ${RsvpTextUtils.directiveValue(fallbackChapterTitle)}"
            val index = lines.indexOfFirst { it.isEmpty() }
            if (index >= 0) {
                lines.add(index, chapter)
            } else {
                lines += chapter
            }
        }

        val body = lines.joinToString("\n").trim() + "\n"
        return RsvpBookFile(
            filename = "${RsvpTextUtils.filenameSafe(title)}.rsvp",
            data = body.encodeToByteArray(),
            title = title,
            wordCount = wordCount,
            chapterCount = maxOf(chapterCount, 1),
        )
    }

    private fun flushLine() {
        if (lineWords.isEmpty()) {
            return
        }
        var line = lineWords.joinToString(" ")
        if (line.startsWith("@")) {
            line = "@$line"
        }
        lines += line
        lineWords = mutableListOf()
        lineLength = 0
    }
}
