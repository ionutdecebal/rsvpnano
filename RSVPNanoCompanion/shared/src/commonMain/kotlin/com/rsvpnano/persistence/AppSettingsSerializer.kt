package com.rsvpnano.persistence

import androidx.datastore.core.okio.OkioSerializer
import com.rsvpnano.models.CompanionAppSettings
import kotlinx.serialization.SerializationException
import kotlinx.serialization.json.Json
import okio.BufferedSink
import okio.BufferedSource

object AppSettingsSerializer : OkioSerializer<CompanionAppSettings> {
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
