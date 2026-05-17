package com.rsvpnano

import com.rsvpnano.converters.RsvpConversionError
import com.rsvpnano.converters.RsvpConverter
import com.rsvpnano.converters.RsvpEvent
import com.rsvpnano.converters.ArticleFormatter
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.Test

class RsvpParityTest {
    @Test
    fun textEventsKeepChaptersAndParagraphs() {
        val events = RsvpConverter.textEvents(
            """
            Chapter 1

            First paragraph.
            Second line.

            # Chapter 2
            Third paragraph.
            """.trimIndent()
        )

        assertEquals(
            listOf(
                RsvpEvent.Chapter("Chapter 1"),
                RsvpEvent.Text("First paragraph. Second line."),
                RsvpEvent.Chapter("Chapter 2"),
                RsvpEvent.Text("Third paragraph."),
            ),
            events,
        )
    }

    @Test
    fun readableTextCollapsesHtmlAndWhitespace() {
        val text = RsvpConverter.readableText(
            """
            <html><body><p>Hello&nbsp;world!</p><p>Line two.</p></body></html>
            """.trimIndent()
        )

        assertEquals("Hello world!\n\nLine two.", text)
    }

    @Test
    fun rsvpFileBuildsDeterministicBody() {
        val file = RsvpConverter.rsvpFile(
            title = "Demo Book",
            source = "demo.txt",
            text = "Chapter 1\n\nHello reader."
        )

        val body = file.data.decodeToString()
        assertEquals("Demo Book.rsvp", file.filename)
        assertEquals(2, file.wordCount)
        assertEquals(1, file.chapterCount)
        assertEquals(true, body.startsWith("@rsvp 1\n@title Demo Book\n@source demo.txt\n\n@chapter Chapter 1\n"))
        assertEquals(true, body.endsWith("\n"))
    }

    @Test
    fun epubConversionIsStillExplicitlyUnsupported() {
        assertFailsWith<RsvpConversionError> {
            RsvpConverter.bookFile(byteArrayOf(), "sample.epub")
        }
    }

    @Test
    fun articleFormatterPrefersVisibleContent() {
        val article = ArticleFormatter.article(
            title = "https://example.com/story",
            source = "https://example.com/story",
            htmlOrText = """
                <html>
                  <head><title>Story Title</title></head>
                  <body><nav>ignore me</nav><article><p>Hello reader.</p></article></body>
                </html>
            """.trimIndent(),
        )

        assertEquals("Story Title", article.title)
        assertEquals("Hello reader.", article.text)
    }
}
