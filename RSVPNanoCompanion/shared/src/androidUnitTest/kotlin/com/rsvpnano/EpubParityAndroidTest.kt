package com.rsvpnano

import com.rsvpnano.converters.RsvpConverter
import java.io.File
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class EpubParityAndroidTest {

    @Test
    fun convertsRealEpubToRsvp() {
        val epub = testVectorFile("sample.epub")
        val data = epub.readBytes()
        val converted = RsvpConverter.bookFile(data, epub.name)

        // The sample-1.epub from getsamplefiles.com has title "Sample title"
        assertEquals("Sample title", converted.title)
        assertTrue(converted.filename.endsWith(".rsvp"))
        
        // Verify basic conversion results
        assertTrue(converted.wordCount > 0)
        assertTrue(converted.chapterCount >= 1)
    }

    private fun testVectorFile(name: String): File {
        val candidates = listOf(
            File("RSVPNanoCompanion/docs/test-vectors", name),
            File("../docs/test-vectors", name),
            File("docs/test-vectors", name),
        )
        return candidates.firstOrNull { it.isFile }
            ?: error("Test vector not found. Checked: ${candidates.joinToString { it.path }}")
    }
}
