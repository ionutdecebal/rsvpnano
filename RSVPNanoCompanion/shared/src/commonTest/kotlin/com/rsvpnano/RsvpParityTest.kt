package com.rsvpnano

import com.rsvpnano.converters.RsvpConversionError
import com.rsvpnano.converters.RsvpConverter
import com.rsvpnano.converters.RsvpEvent
import com.rsvpnano.converters.ArticleFormatter
import com.rsvpnano.converters.RsvpSupportedFileTypes
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
    fun textToRsvpMatchesReferenceVector() {
        val file = RsvpConverter.rsvpFile(
            title = "Basic Text Vector",
            source = "basic-text-input.txt",
            text = BASIC_TEXT_INPUT,
        )

        assertEquals("Basic Text Vector.rsvp", file.filename)
        assertEquals(6, file.wordCount)
        assertEquals(2, file.chapterCount)
        assertEquals(BASIC_TEXT_EXPECTED_RSVP, file.data.decodeToString())
    }

    @Test
    fun htmlToRsvpMatchesReferenceVector() {
        val file = RsvpConverter.rsvpFile(
            title = "Basic HTML Vector",
            source = "basic-html-input.html",
            text = BASIC_HTML_INPUT,
        )

        assertEquals("Basic HTML Vector.rsvp", file.filename)
        assertEquals(11, file.wordCount)
        assertEquals(2, file.chapterCount)
        assertEquals(BASIC_HTML_EXPECTED_RSVP, file.data.decodeToString())
    }

    @Test
    fun supportedFileTypesMatchConverterCoverage() {
        val expectedConvertible = setOf(".epub", ".txt", ".md", ".markdown", ".html", ".htm", ".xhtml")
        expectedConvertible.forEach { extension ->
            assertEquals(true, RsvpSupportedFileTypes.isConvertible("book$extension"), extension)
        }
        assertEquals(true, RsvpSupportedFileTypes.isUploadPassthrough("book.rsvp"))
    }

    @Test
    fun bookFilePassesRsvpThroughUnchanged() {
        val data = "@rsvp 1\n@title Ready\n@source ready.rsvp\n\nHello.\n".encodeToByteArray()
        val file = RsvpConverter.bookFile(data, "ready.rsvp")

        assertEquals("ready.rsvp", file.filename)
        assertEquals("ready", file.title)
        assertEquals(data.decodeToString(), file.data.decodeToString())
    }

    @Test
    fun bookFileSupportsTextAndMarkdownExtensions() {
        val cases = listOf(
            Triple("book.txt", "Chapter Plain Text\n\nHello reader.", "Chapter Plain Text"),
            Triple("book.md", "# Markdown Chapter\n\nHello reader.", "Markdown Chapter"),
            Triple("book.markdown", "Chapter Markdown Extension\n\nHello reader.", "Chapter Markdown Extension"),
        )

        cases.forEach { (filename, input, chapter) ->
            val converted = RsvpConverter.bookFile(input.encodeToByteArray(), filename)
            val body = converted.data.decodeToString()
            assertEquals(true, body.contains("@source $filename"), filename)
            assertEquals(true, body.contains("@chapter $chapter"), filename)
            assertEquals(true, body.contains("Hello reader."), filename)
        }
    }

    @Test
    fun bookFileSupportsHtmlExtensions() {
        val cases = listOf(
            "book.html" to "HTML Chapter",
            "book.htm" to "HTM Chapter",
            "book.xhtml" to "XHTML Chapter",
        )

        cases.forEach { (filename, chapter) ->
            val converted = RsvpConverter.bookFile(
                "<html><body><h1>$chapter</h1><p>Hello reader.</p></body></html>".encodeToByteArray(),
                filename,
            )
            val body = converted.data.decodeToString()
            assertEquals(true, body.contains("@source $filename"), filename)
            assertEquals(true, body.contains("@chapter $chapter"), filename)
            assertEquals(true, body.contains("Hello reader."), filename)
        }
    }

    @Test
    fun invalidEpubFailsWithConversionError() {
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

    @Test
    fun articleFormatterNormalizesEscapedLineBreaks() {
        val article = ArticleFormatter.article(
            title = "GitHub",
            source = "https://github.com/example/repo",
            htmlOrText = "First paragraph.\\n\\nSecond paragraph.\\r\\nThird line.",
        )

        assertEquals("First paragraph.\n\nSecond paragraph.\n\nThird line.", article.text)
    }

    private companion object {
        // Mirrors testdata/conversion/basic-text-input.txt. Keep commonTest free of JVM-only file APIs.
        private val BASIC_TEXT_INPUT = """
            Chapter 1

            Hello reader.

            # Chapter 2
            @directive-looking text stays readable.
        """.trimIndent()

        // Mirrors testdata/conversion/basic-text-expected.rsvp.
        private val BASIC_TEXT_EXPECTED_RSVP = """
            @rsvp 1
            @title Basic Text Vector
            @source basic-text-input.txt

            @chapter Chapter 1
            Hello reader.

            @chapter Chapter 2

            @para
            @@directive-looking text stays readable.
        """.trimIndent() + "\n"

        // Mirrors testdata/conversion/basic-html-input.html. Keep commonTest free of JVM-only file APIs.
        private val BASIC_HTML_INPUT = """
            <!doctype html>
            <html>
            <head>
              <title>Ignored Browser Title</title>
              <style>.hidden { display: none; }</style>
            </head>
            <body>
              <nav>Ignore navigation</nav>
              <article>
                <h1>Chapter One</h1>
                <p>Hello <span>reader</span> &amp; friend.</p>
                <p><span>Inline</span> <span>punctuation</span><span>!</span></p>
                <h2>Chapter Two</h2>
                <p>@directive-looking text stays readable.</p>
              </article>
              <footer>Footer note.</footer>
            </body>
            </html>
        """.trimIndent()

        // Mirrors testdata/conversion/basic-html-expected.rsvp.
        private val BASIC_HTML_EXPECTED_RSVP = """
            @rsvp 1
            @title Basic HTML Vector
            @source basic-html-input.html

            @chapter Chapter One
            Hello reader & friend.

            @para
            Inline punctuation !

            @chapter Chapter Two

            @para
            @@directive-looking text stays readable.

            @para
            Footer note.
        """.trimIndent() + "\n"
    }
}
