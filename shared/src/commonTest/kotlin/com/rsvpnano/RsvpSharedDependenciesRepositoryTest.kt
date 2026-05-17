package com.rsvpnano

import com.rsvpnano.app.RsvpSharedDependencies
import com.rsvpnano.persistence.PendingUploadStorage
import com.rsvpnano.persistence.RssFeedStorage
import kotlin.test.Test
import kotlin.test.assertNotNull

class RsvpSharedDependenciesRepositoryTest {
    @Test
    fun createsPendingUploadRepository() {
        val dependencies = RsvpSharedDependencies(
            pendingUploadStorage = object : PendingUploadStorage {
                override suspend fun readText(): String? = null
                override suspend fun writeText(value: String) = Unit
            },
            rssFeedStorage = object : RssFeedStorage {
                override suspend fun readText(): String? = null
                override suspend fun writeText(value: String) = Unit
            },
        )

        assertNotNull(dependencies.createPendingUploadRepository())
        assertNotNull(dependencies.createApp().pendingUploadRepository)
    }
}
