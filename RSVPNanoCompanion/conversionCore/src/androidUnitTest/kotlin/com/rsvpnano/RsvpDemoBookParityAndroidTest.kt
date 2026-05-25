package com.rsvpnano

import com.rsvpnano.converters.RsvpConverter
import java.io.File
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class RsvpDemoBookParityAndroidTest {
    @Test
    fun existingRsvpDemoBookPassesThroughByteForByte() {
        val demo = demoBookFile("european-letter-demo.rsvp")
        val data = demo.readBytes()
        val converted = RsvpConverter.bookFile(data, demo.name)

        assertEquals(demo.name, converted.filename)
        assertContentEquals(data, converted.data)
        assertEquals("european-letter-demo", converted.title)
    }

    private fun demoBookFile(name: String): File {
        val candidates = listOf(
            File("RSVPNanoCompanion/docs/demo-books", name),
            File("../docs/demo-books", name),
            File("docs/demo-books", name),
        )
        return candidates.firstOrNull { it.isFile }
            ?: error("Demo book not found. Checked: ${candidates.joinToString { it.path }}")
    }
}
