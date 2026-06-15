package com.rsvpnano

import com.rsvpnano.converters.ImportPreparation
import kotlin.test.Test
import kotlin.test.assertEquals

class ImportPreparationTest {
    @Test
    fun titleForTextUsesPreferredTitleWhenPresent() {
        assertEquals(
            "Manual title",
            ImportPreparation.titleForText(
                preferredTitle = "  Manual title  ",
                text = "Document title\n\nBody",
                fallback = "Untitled",
            ),
        )
    }

    @Test
    fun titleForTextExtractsTitleWhenPreferredTitleIsBlank() {
        assertEquals(
            "Document title",
            ImportPreparation.titleForText(
                preferredTitle = "",
                text = "Document title\n\nBody",
                fallback = "Untitled",
            ),
        )
    }

    @Test
    fun titleForTextUsesFallbackWhenPreferredTitleAndTextTitleAreBlank() {
        assertEquals(
            "Untitled",
            ImportPreparation.titleForText(
                preferredTitle = "   ",
                text = " \n \t ",
                fallback = "Untitled",
            ),
        )
    }

    @Test
    fun titleForSharedUrlFiltersBrowserPlaceholderTitles() {
        assertEquals(
            "https://example.com/article",
            ImportPreparation.titleForSharedUrl(
                preferredTitle = "example.com",
                source = "https://example.com/article",
                host = "example.com",
            ),
        )
        assertEquals(
            "Useful Article",
            ImportPreparation.titleForSharedUrl(
                preferredTitle = "Useful Article",
                source = "https://example.com/article",
                host = "example.com",
            ),
        )
    }

    @Test
    fun titleForSharedUrlFiltersWwwHostAndSourceContainingTitle() {
        assertEquals(
            "https://example.com/article",
            ImportPreparation.titleForSharedUrl(
                preferredTitle = "www.example.com",
                source = "https://example.com/article",
                host = "example.com",
            ),
        )
        assertEquals(
            "https://example.com/articles/useful-article",
            ImportPreparation.titleForSharedUrl(
                preferredTitle = "useful-article",
                source = "https://example.com/articles/useful-article",
                host = "example.com",
            ),
        )
    }

    @Test
    fun rsvpFileForTextUsesSharedTitleRules() {
        val file = ImportPreparation.rsvpFileForText(
            title = "",
            source = "manual",
            text = "Document title\n\nBody text",
            fallbackTitle = "Untitled",
        )

        assertEquals("Document title", file.title)
        assertEquals("Document title.rsvp", file.filename)
    }

    @Test
    fun pendingUploadForTextNormalizesTitleSourceAndBody() {
        val item = ImportPreparation.pendingUploadForText(
            id = "1",
            title = "",
            source = " Shared text ",
            text = " Document title\n\nBody text ",
            createdAt = "2026-05-18T10:00:00Z",
            fallbackTitle = "Untitled",
        )

        assertEquals("Document title", item.title)
        assertEquals("Shared text", item.sourceUrl)
        assertEquals("Document title\n\nBody text", item.body)
        assertEquals("2026-05-18T10:00:00Z", item.createdAt)
    }

    @Test
    fun pendingUploadForTextStoresNullSourceWhenSourceIsBlank() {
        val item = ImportPreparation.pendingUploadForText(
            id = "1",
            title = "Manual title",
            source = "  ",
            text = " Body text ",
            createdAt = "2026-05-18T10:00:00Z",
            fallbackTitle = "Untitled",
        )

        assertEquals("Manual title", item.title)
        assertEquals(null, item.sourceUrl)
        assertEquals("Body text", item.body)
    }

    @Test
    fun pendingUploadForTextInfersTitleFromFirstReadableLine() {
        val item = ImportPreparation.pendingUploadForText(
            id = "1",
            title = "",
            source = "",
            text = "\n\nInferred shared title\n\nShared body text.",
            createdAt = "2026-05-18T10:00:00Z",
            fallbackTitle = "Untitled",
        )

        assertEquals("Inferred shared title", item.title)
        assertEquals("Inferred shared title\n\nShared body text.", item.body)
    }

    @Test
    fun pendingUploadForUrlUsesSharedUrlTitleRules() {
        val item = ImportPreparation.pendingUploadForUrl(
            id = "1",
            title = "example.com",
            source = "https://example.com/article",
            host = "example.com",
            createdAt = "2026-05-18T10:00:00Z",
        )

        assertEquals("https://example.com/article", item.title)
        assertEquals("https://example.com/article", item.sourceUrl)
        assertEquals("https://example.com/article", item.body)
    }

    @Test
    fun pendingUploadForUrlTrimsSourceAndAllowsEmptySource() {
        val trimmed = ImportPreparation.pendingUploadForUrl(
            id = "1",
            title = "",
            source = " https://example.com/article ",
            host = "example.com",
            createdAt = "2026-05-18T10:00:00Z",
        )
        val empty = ImportPreparation.pendingUploadForUrl(
            id = "2",
            title = "",
            source = " ",
            host = "",
            createdAt = "2026-05-18T10:00:00Z",
        )

        assertEquals("https://example.com/article", trimmed.title)
        assertEquals("https://example.com/article", trimmed.sourceUrl)
        assertEquals("https://example.com/article", trimmed.body)
        assertEquals(null, empty.sourceUrl)
        assertEquals("", empty.body)
    }
}
