package com.rsvpnano.persistence

import androidx.datastore.core.DataStore
import androidx.datastore.core.DataStoreFactory
import androidx.datastore.core.okio.OkioStorage
import com.rsvpnano.models.CompanionAppSettings
import java.io.File
import okio.FileSystem
import okio.Path.Companion.toPath

fun createSettingsDataStore(file: File): DataStore<CompanionAppSettings> {
    return DataStoreFactory.create(
        storage = OkioStorage(
            fileSystem = FileSystem.SYSTEM,
            serializer = AppSettingsSerializer,
            producePath = {
                file.parentFile?.mkdirs()
                file.absolutePath.toPath()
            },
        ),
    )
}
