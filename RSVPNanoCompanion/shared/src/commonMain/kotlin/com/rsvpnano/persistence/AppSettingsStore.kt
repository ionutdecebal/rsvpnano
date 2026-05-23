package com.rsvpnano.persistence

import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import com.rsvpnano.models.CompanionAppSettings
import kotlinx.coroutines.flow.first
import kotlinx.serialization.json.Json

/**
 * Shared storage for companion app settings (default address, remembered Nano).
 */
class AppSettingsStore(
    private val dataStore: DataStore<Preferences>,
    private val json: Json = Json {
        ignoreUnknownKeys = true
        encodeDefaults = true
        explicitNulls = false
    },
) {
    suspend fun load(): CompanionAppSettings {
        val text = dataStore.data.first()[AppSettingsKey]
            ?: return CompanionAppSettings()
        return runCatching { json.decodeFromString(CompanionAppSettings.serializer(), text) }
            .getOrDefault(CompanionAppSettings())
    }

    suspend fun save(settings: CompanionAppSettings) {
        val text = json.encodeToString(CompanionAppSettings.serializer(), settings)
        dataStore.edit { preferences ->
            preferences[AppSettingsKey] = text
        }
    }

    private companion object {
        val AppSettingsKey = stringPreferencesKey("companion_settings_json")
    }
}
