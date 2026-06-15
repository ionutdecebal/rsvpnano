package com.rsvpnano.persistence

import okio.FileSystem
import okio.Path

class OkioTextStorage(
    private val path: Path,
    private val fileSystem: FileSystem = FileSystem.SYSTEM,
) : PendingUploadStorage, AppSettingsStorage {
    override suspend fun readText(): String? {
        if (!fileSystem.exists(path)) return null
        return fileSystem.read(path) {
            readUtf8()
        }
    }

    override suspend fun writeText(value: String) {
        path.parent?.let(fileSystem::createDirectories)
        fileSystem.write(path) {
            writeUtf8(value)
        }
    }
}
