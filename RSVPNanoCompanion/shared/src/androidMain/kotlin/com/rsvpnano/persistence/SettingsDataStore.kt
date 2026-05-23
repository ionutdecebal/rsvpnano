package com.rsvpnano.persistence

import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.PreferenceDataStoreFactory
import androidx.datastore.preferences.core.Preferences
import java.io.File

fun createSettingsDataStore(file: File): DataStore<Preferences> {
    return PreferenceDataStoreFactory.create {
        file.parentFile?.mkdirs()
        file
    }
}
