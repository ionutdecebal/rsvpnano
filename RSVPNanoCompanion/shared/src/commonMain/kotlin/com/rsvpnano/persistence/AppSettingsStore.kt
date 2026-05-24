package com.rsvpnano.persistence

import androidx.datastore.core.DataStore
import com.rsvpnano.models.CompanionAppSettings
import kotlinx.coroutines.flow.first

/**
 * Shared storage for companion app settings (default address, remembered Nano).
 */
class AppSettingsStore(
    private val dataStore: DataStore<CompanionAppSettings>,
) {
    suspend fun load(): CompanionAppSettings {
        return dataStore.data.first()
    }

    suspend fun save(settings: CompanionAppSettings) {
        dataStore.updateData { settings }
    }
}
