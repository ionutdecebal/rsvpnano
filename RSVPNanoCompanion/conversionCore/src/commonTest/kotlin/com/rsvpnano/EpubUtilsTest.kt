package com.rsvpnano

import com.rsvpnano.converters.EpubUtils
import kotlin.test.Test
import kotlin.test.assertEquals

class EpubUtilsTest {
    @Test
    fun zipJoinNormalizesRelativePaths() {
        assertEquals("OPS/chapter1.xhtml", EpubUtils.zipJoin("OPS/content.opf", "chapter1.xhtml"))
        assertEquals("OPS/chapters/chapter2.xhtml", EpubUtils.zipJoin("OPS/content.opf", "chapters/chapter2.xhtml"))
    }

    @Test
    fun fallbackChapterTitleUsesReadableFilename() {
        assertEquals("Chapter 4", EpubUtils.fallbackChapterTitle("OPS/chapter-4.xhtml", 4))
    }
}
