package com.rsvpnano

import com.rsvpnano.app.RsvpSharedDependencies
import com.rsvpnano.persistence.PendingUploadStorage
import kotlin.test.Test
import kotlin.test.assertNotNull

class RsvpSharedDependenciesTest {
    @Test
    fun canCreateDependenciesFromStorageBackends() {
        val dependencies = RsvpSharedDependencies(
            pendingUploadStorage = object : PendingUploadStorage {
                override suspend fun readText(): String? = null
                override suspend fun writeText(value: String) = Unit
            },
            appSettingsStore = testAppSettingsStore(),
        )

        assertNotNull(dependencies.createPendingDraftService())
        assertNotNull(dependencies.createApp().pendingDraftService)
    }
}
