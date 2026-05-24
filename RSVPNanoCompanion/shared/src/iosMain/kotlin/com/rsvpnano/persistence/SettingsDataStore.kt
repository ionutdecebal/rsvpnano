package com.rsvpnano.persistence

import androidx.datastore.core.DataStore
import androidx.datastore.core.DataStoreFactory
import androidx.datastore.core.okio.OkioStorage
import com.rsvpnano.models.CompanionAppSettings
import kotlinx.cinterop.ExperimentalForeignApi
import okio.FileSystem
import okio.Path.Companion.toPath
import platform.Foundation.NSFileManager
import platform.Foundation.NSURL

@OptIn(ExperimentalForeignApi::class)
fun createSettingsDataStore(
    fileManager: NSFileManager = NSFileManager.defaultManager(),
    appGroupIdentifier: String = "group.com.rsvpnano.companion",
): DataStore<CompanionAppSettings> {
    return DataStoreFactory.create(
        storage = OkioStorage(
            fileSystem = FileSystem.SYSTEM,
            serializer = AppSettingsSerializer,
            producePath = {
                val rootURL = fileManager.containerURLForSecurityApplicationGroupIdentifier(appGroupIdentifier)
                    ?: error("App group container unavailable: $appGroupIdentifier")
                val folder = rootURL.URLByAppendingPathComponent("Settings", isDirectory = true)
                    ?: error("Settings folder URL unavailable")
                fileManager.createDirectoryAtURL(folder, withIntermediateDirectories = true, attributes = null, error = null)
                val fileURL: NSURL = folder.URLByAppendingPathComponent("companion_settings.json")
                    ?: error("Settings file URL unavailable")
                fileURL.path!!.toPath()
            },
        ),
    )
}
