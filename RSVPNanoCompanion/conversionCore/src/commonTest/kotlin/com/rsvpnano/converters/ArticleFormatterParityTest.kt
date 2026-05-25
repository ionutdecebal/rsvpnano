package com.rsvpnano.converters

import kotlin.test.Test
import kotlin.test.assertEquals

class ArticleFormatterParityTest {

    @Test
    fun formatsBasicHtmlArticle() {
        val html = """
            <html>
                <head><title>Test Article</title></head>
                <body>
                    <nav>Navigation</nav>
                    <article>
                        <p>This is the main content.</p>
                        <p>Second paragraph.</p>
                    </article>
                    <footer>Footer info</footer>
                </body>
            </html>
        """.trimIndent()

        val article = ArticleFormatter.article(title = "Test Article", source = "https://example.com", htmlOrText = html)
        
        assertEquals("Test Article", article.title)
        assertEquals("This is the main content.\n\nSecond paragraph.", article.text)
    }

    @Test
    fun removesNoiseBlocks() {
        val html = """
            <html>
                <body>
                    <header>Header</header>
                    <aside>Sidebar</aside>
                    <main>
                        <p>Real content here.</p>
                        <script>console.log('noise');</script>
                        <style>.noise { color: red; }</style>
                        <form><input type="text"/></form>
                    </main>
                    <noscript>No script message</noscript>
                </body>
            </html>
        """.trimIndent()

        val article = ArticleFormatter.article(title = "", source = "https://example.com/noisy", htmlOrText = html)
        
        // title should be inferred from header or source if title tag is missing
        // In this case, it might use the host of the source.
        assertEquals("example.com/noisy", article.title)
        assertEquals("Real content here.", article.text)
    }

    @Test
    fun handlesComplexHtmlEntities() {
        val html = """
            <p>Hello&nbsp;world! &ldquo;Smart quotes&rdquo; &amp; ampersands.</p>
        """.trimIndent()

        val article = ArticleFormatter.article(title = "Entities", source = "https://example.com", htmlOrText = html)
        
        assertEquals("Hello world! \"Smart quotes\" & ampersands.", article.text)
    }

    @Test
    fun prefersArticleTagOverMain() {
        val html = """
            <body>
                <main>
                    Main content
                    <article>Article content</article>
                </main>
            </body>
        """.trimIndent()

        val article = ArticleFormatter.article(title = "Priority", source = "https://example.com", htmlOrText = html)
        assertEquals("Article content", article.text)
    }
}
