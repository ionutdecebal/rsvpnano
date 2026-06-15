package com.rsvpnano

import com.rsvpnano.converters.RsvpConverter
import com.rsvpnano.converters.RsvpConversionError
import java.io.File
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class EpubParityAndroidTest {

    @Test
    fun convertsRealEpubToRsvp() {
        val epub = testVectorFile("sample.epub")
        val data = epub.readBytes()
        val converted = RsvpConverter.bookFile(data, epub.name)

        // Mirrors docs/conversion-spec.md: the title comes from OPF metadata.
        assertEquals("Letter", converted.title)
        assertTrue(converted.filename.endsWith(".rsvp"))
        
        // Verify basic conversion results
        assertTrue(converted.wordCount > 0)
        assertTrue(converted.chapterCount >= 1)
        assertEquals(
            testVectorFile("sample-expected.rsvp").readText().replace("\r\n", "\n"),
            converted.data.decodeToString(),
        )
    }

    @Test
    fun draculaEpubsUseTocChapterTitles() {
        listOf("Dracula-epub.epub", "Dracula-epub3.epub").forEach { name ->
            val epub = testVectorFile(name)
            val converted = RsvpConverter.bookFile(epub.readBytes(), epub.name)
            val chapters = converted.data.decodeToString()
                .lineSequence()
                .filter { it.startsWith("@chapter ") }
                .toList()

            assertTrue(
                chapters.any { it.startsWith("@chapter CHAPTER I JONATHAN HARKER") },
                "Chapter I should come from the EPUB TOC in $name",
            )
            assertTrue(
                chapters.any { it.startsWith("@chapter CHAPTER II JONATHAN HARKER") },
                "Chapter II should come from the EPUB TOC in $name",
            )
            assertEquals(
                1,
                chapters.count { it.startsWith("@chapter CHAPTER I JONATHAN HARKER") },
                "Chapter I should not be duplicated in $name",
            )
            assertEquals(
                false,
                chapters.any { it == "@chapter CHAPTER I" },
                "Short body heading should not override the richer TOC chapter title in $name",
            )
            assertEquals(
                false,
                chapters.any { it.contains("7599939443149237915") },
                "Chapter titles should not fall back to generated XHTML filenames in $name",
            )
            assertEquals(
                false,
                chapters.any { it == "@chapter D R A C U L A" },
                "Book title pages should not become chapter titles in $name",
            )
        }
    }

    @Test
    fun singleDocumentEpubUsesOrderedTocLabels() {
        val chapters = chaptersFor("single-document-toc.epub")

        assertEquals(
            listOf(
                "@chapter I. The Arrival",
                "@chapter II. Father and Son",
            ),
            chapters,
        )
        val body = bodyFor("single-document-toc.epub")
        assertEquals(true, body.contains("The harbour was bright."))
        assertEquals(true, body.contains("The door opened."))
    }

    @Test
    fun nestedTocIsFlattenedInTocOrder() {
        assertEquals(
            listOf(
                "@chapter Part One",
                "@chapter Chapter One A",
                "@chapter Part Two",
            ),
            chaptersFor("nested-toc.epub"),
        )
    }

    @Test
    fun encodedTocPathsAndFragmentsResolveToContent() {
        val body = bodyFor("encoded-paths.epub")

        assertEquals(listOf("@chapter Encoded Chapter"), chaptersFor("encoded-paths.epub"))
        assertEquals(true, body.contains("Encoded path text."))
    }

    @Test
    fun epub3NavTocBeatsLegacyNcxWhenBothArePresent() {
        assertEquals(listOf("@chapter Nav Chapter Title"), chaptersFor("epub3-nav-priority.epub"))
    }

    @Test
    fun encryptedEpubFailsExplicitly() {
        val epub = testVectorFile("encrypted-content.epub")

        assertFailsWith<RsvpConversionError> {
            RsvpConverter.bookFile(epub.readBytes(), epub.name)
        }
    }

    @Test
    fun sparseNcxDoesNotHideBodyChapterHeadings() {
        val epub = optionalTestVectorFile("TCOMC.epub") ?: return
        val chapters = RsvpConverter.bookFile(epub.readBytes(), epub.name).data.decodeToString()
            .lineSequence()
            .filter { it.startsWith("@chapter ") }
            .toList()

        assertEquals(122, chapters.size)
        assertEquals(false, chapters.any { it == "@chapter Contents" })
        assertEquals(false, chapters.any { it == "@chapter The Count of Monte Cristo" })
        assertEquals(true, chapters.any { it.startsWith("@chapter I MARSEILLE") })
        assertEquals(true, chapters.contains("@chapter III LES CATALANS"))
        assertEquals(true, chapters.contains("@chapter XI THE CORSICAN OGRE"))
        assertEquals(true, chapters.contains("@chapter CXVII OCTOBER THE FIFTH"))
        assertEquals("@chapter Notes", chapters.last())
    }

    private fun testVectorFile(name: String): File {
        return optionalTestVectorFile(name)
            ?: error("Test vector not found. Checked: ${testVectorCandidates(name).joinToString { it.path }}")
    }

    private fun optionalTestVectorFile(name: String): File? {
        return testVectorCandidates(name).firstOrNull { it.isFile }
    }

    private fun testVectorCandidates(name: String): List<File> {
        val candidates = listOf(
            File("RSVPNanoCompanion/testdata/conversion", name),
            File("../testdata/conversion", name),
            File("testdata/conversion", name),
        )
        return candidates
    }

    private fun bodyFor(name: String): String {
        val epub = testVectorFile(name)
        return RsvpConverter.bookFile(epub.readBytes(), epub.name).data.decodeToString()
    }

    private fun chaptersFor(name: String): List<String> {
        return bodyFor(name)
            .lineSequence()
            .filter { it.startsWith("@chapter ") }
            .toList()
    }
}
