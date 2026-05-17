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
}
