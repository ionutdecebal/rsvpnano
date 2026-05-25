package com.rsvpnano.converters

object RsvpConverter {
    const val wrapWidth = 96

    fun bookFile(data: ByteArray, filename: String): RsvpBookFile {
        if (RsvpSupportedFileTypes.isRsvp(filename)) {
            return RsvpBookFile(filename = filename, data = data, title = filenameWithoutExtension(filename))
        }

        if (RsvpSupportedFileTypes.isEpub(filename)) {
            return EpubBookConverter.convert(
                entries = EpubZipReader.readEntries(data),
                filename = filename,
            )
        }

        val rawText = RsvpTextUtils.decodeText(data) ?: throw RsvpConversionError.unreadableText
        val (title, events) = if (RsvpTextUtils.looksLikeHTML(rawText)) {
            RsvpTextUtils.titleFromText(rawText, fallback = filenameWithoutExtension(filename)) to RsvpTextUtils.htmlEvents(rawText)
        } else {
            filenameWithoutExtension(filename) to RsvpTextUtils.textEvents(rawText)
        }
        return rsvpFile(title = title, author = "", source = filename, events = events)
    }

    fun rsvpFile(title: String, source: String, text: String): RsvpBookFile {
        val events = if (RsvpTextUtils.looksLikeHTML(text)) {
            RsvpTextUtils.htmlEvents(text)
        } else {
            RsvpTextUtils.textEvents(text)
        }
        return rsvpFile(title = title, author = "", source = source, events = events)
    }

    fun rsvpFile(title: String, author: String, source: String, events: List<RsvpEvent>): RsvpBookFile {
        val writer = RsvpWriter(title = title, author = author, source = source)
        events.forEach { event ->
            when (event) {
                is RsvpEvent.Chapter -> writer.addChapter(event.title)
                is RsvpEvent.Text -> {
                    writer.beginParagraph()
                    writer.addText(event.text)
                }
            }
        }
        return writer.finalize(fallbackChapterTitle = title)
    }

    fun readableText(value: String): String = RsvpTextUtils.readableText(value)

    fun titleFromText(text: String, fallback: String): String = RsvpTextUtils.titleFromText(text, fallback)

    fun htmlEvents(markup: String): List<RsvpEvent> = RsvpTextUtils.htmlEvents(markup)

    fun textEvents(text: String): List<RsvpEvent> = RsvpTextUtils.textEvents(text)

    fun filenameSafe(value: String): String = RsvpTextUtils.filenameSafe(value)

    fun decodeText(data: ByteArray): String? = RsvpTextUtils.decodeText(data)

    fun filenameWithoutExtension(filename: String): String {
        val index = filename.lastIndexOf('.')
        return if (index > 0) filename.substring(0, index) else filename
    }
}
