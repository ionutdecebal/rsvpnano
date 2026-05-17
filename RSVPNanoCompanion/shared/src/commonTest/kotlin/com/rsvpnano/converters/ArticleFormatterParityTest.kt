package com.rsvpnano.converters

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class ArticleFormatterParityTest {

    @Test
    fun extractsTitleFromHtmlTitleTag() {
        val source = "https://example.com/art"
        val html = """
            <html>
            <head><title>  My Awesome Article  </title></head>
            <body><p>Hello world</p></body>
            </html>
        """.trimIndent()
        val article = ArticleFormatter.article(source, source, html)
        assertEquals("My Awesome Article", article.title)
    }

    @Test
    fun extractsTitleFromH1IfNoTitleTag() {
        val source = "https://example.com/art"
        val html = """
            <html>
            <body>
            <h1> The Actual Title </h1>
            <p>Some content</p>
            </body>
            </html>
        """.trimIndent()
        // ArticleFormatter uses RsvpTextUtils.titleFromText as fallback which looks for <title> then first non-empty line.
        val article = ArticleFormatter.article(source, source, html)
        assertEquals("The Actual Title", article.title)
    }

    @Test
    fun removesAnnoyingBlocks() {
        val html = """
            <html>
            <body>
            <header>Site Header</header>
            <nav>Navigation Link</nav>
            <article>
                <script>console.log('bad')</script>
                <style>.bad { color: red; }</style>
                <p>Good content</p>
                <aside>Related stories</aside>
            </article>
            <footer>Copyright 2024</footer>
            </body>
            </html>
        """.trimIndent()
        val article = ArticleFormatter.article("Title", "https://example.com/art", html)
        // focusedHTML will find <article> and removingBlocks will remove header, nav, script, style, aside, footer.
        assertEquals("Good content", article.text)
    }

    @Test
    fun focusesOnMainIfAvailable() {
        val html = """
            <html>
            <body>
            <nav>Nav</nav>
            <main>
                <p>Main content</p>
            </main>
            <aside>Aside</aside>
            </body>
            </html>
        """.trimIndent()
        val article = ArticleFormatter.article("Title", "https://example.com/art", html)
        assertEquals("Main content", article.text)
    }

    @Test
    fun fallsBackToHostIfNoTitle() {
        val html = "<html><body></body></html>"
        val article = ArticleFormatter.article("https://example.com/path", "https://example.com/path", html)
        assertEquals("example.com", article.title)
    }

    @Test
    fun handlesPlainText() {
        val text = "This is just plain text.\nNo HTML here."
        val article = ArticleFormatter.article("My Text", "https://example.com/txt", text)
        assertEquals("My Text", article.title)
        assertEquals("This is just plain text.\n\nNo HTML here.", article.text) // RsvpTextUtils.readableText adds double newline for paragraphs
    }

    @Test
    fun recognizesPlaceholderTitles() {
        val source = "https://rsvpnano.com/help"
        // If no title and no text, it should fall back to host
        assertEquals("rsvpnano.com", ArticleFormatter.article(source, source, "<html><body></body></html>").title)
        
        // If title is placeholder, it prefers text from body if available
        assertEquals("Body Title", ArticleFormatter.article(source, source, "<body><p>Body Title</p></body>").title)
    }
}
