package com.rsvpnano.persistence

import androidx.datastore.core.DataStore
import androidx.datastore.core.DataStoreFactory
import androidx.datastore.core.okio.OkioStorage
import com.rsvpnano.models.CompanionAppSettings
import java.io.File
import kotlinx.coroutines.flow.first
import kotlinx.serialization.SerializationException
import kotlinx.serialization.json.Json
import okio.BufferedSink
import okio.BufferedSource
import okio.FileSystem
import okio.Path.Companion.toPath

class DataStoreAppSettingsStore(
    private val dataStore: DataStore<CompanionAppSettings>,
) : AppSettingsStore {
    override suspend fun load(): CompanionAppSettings {
        return dataStore.data.first()
    }

    override suspend fun save(settings: CompanionAppSettings) {
        dataStore.updateData { settings }
    }
}

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

private object AppSettingsSerializer : androidx.datastore.core.okio.OkioSerializer<CompanionAppSettings> {
    private val json = Json {
        ignoreUnknownKeys = true
        encodeDefaults = true
        explicitNulls = false
    }

    override val defaultValue: CompanionAppSettings = CompanionAppSettings()

    override suspend fun readFrom(source: BufferedSource): CompanionAppSettings {
        return runCatching {
            val text = source.readUtf8()
            if (text.isBlank()) defaultValue else json.decodeFromString(CompanionAppSettings.serializer(), text)
        }.recoverCatching { error ->
            if (error is SerializationException || error is IllegalArgumentException) defaultValue else throw error
        }.getOrThrow()
    }

    override suspend fun writeTo(t: CompanionAppSettings, sink: BufferedSink) {
        sink.writeUtf8(json.encodeToString(CompanionAppSettings.serializer(), t))
    }
}
