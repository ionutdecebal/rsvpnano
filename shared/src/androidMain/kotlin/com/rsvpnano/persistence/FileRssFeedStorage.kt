package com.rsvpnano.persistence

import java.io.File

class FileRssFeedStorage(
    private val file: File,
) : RssFeedStorage {
    override suspend fun readText(): String? {
        if (!file.exists()) return null
        return file.readText()
    }

    override suspend fun writeText(value: String) {
        file.parentFile?.mkdirs()
        file.writeText(value)
    }
}
