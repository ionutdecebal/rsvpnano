package com.rsvpnano.converters

data class RsvpBookFile(
    val filename: String,
    val data: ByteArray,
    val title: String,
    val wordCount: Int = 0,
    val chapterCount: Int = 0,
)
