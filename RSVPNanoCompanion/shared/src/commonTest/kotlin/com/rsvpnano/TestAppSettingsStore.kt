package com.rsvpnano

import androidx.datastore.core.DataStore
import com.rsvpnano.models.CompanionAppSettings
import com.rsvpnano.persistence.AppSettingsStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow

fun testAppSettingsStore(): AppSettingsStore {
    return AppSettingsStore(TestSettingsDataStore())
}

private class TestSettingsDataStore : DataStore<CompanionAppSettings> {
    private val state = MutableStateFlow(CompanionAppSettings())

    override val data: Flow<CompanionAppSettings> = state

    override suspend fun updateData(
        transform: suspend (t: CompanionAppSettings) -> CompanionAppSettings,
    ): CompanionAppSettings {
        val updated = transform(state.value)
        state.value = updated
        return updated
    }
}
