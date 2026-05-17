package com.rsvpnano.persistence

import java.io.File

class FilePendingUploadStorage(
    private val file: File,
) : PendingUploadStorage {
    override suspend fun readText(): String? {
        if (!file.exists()) return null
        return file.readText()
    }

    override suspend fun writeText(value: String) {
        file.parentFile?.mkdirs()
        file.writeText(value)
    }
}
