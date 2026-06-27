package com.rsvpnano

import com.rsvpnano.models.CompanionAppSettings
import com.rsvpnano.persistence.AppSettingsStore

fun testAppSettingsStore(): AppSettingsStore {
    return TestAppSettingsStore()
}

private class TestAppSettingsStore : AppSettingsStore {
    private var state = CompanionAppSettings()

    override suspend fun load(): CompanionAppSettings = state

    override suspend fun save(settings: CompanionAppSettings) {
        state = settings
    }
}
