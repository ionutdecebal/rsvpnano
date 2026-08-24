package com.rsvpnano.api

import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoFontSummary
import com.rsvpnano.models.NanoFocusTimers
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoLanguageFont
import com.rsvpnano.models.NanoLocaleSummary
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoStorageRepair
import com.rsvpnano.models.NanoThemeSummary
import com.rsvpnano.models.NanoWifiSettings

interface NanoApi {
    fun close() = Unit

    suspend fun fetchDevice(baseUrl: String): NanoInfo
    suspend fun repairStorage(baseUrl: String): NanoStorageRepair
    suspend fun listLibrary(baseUrl: String): List<NanoBook>
    suspend fun listThemes(baseUrl: String): List<NanoThemeSummary>
    suspend fun listFonts(baseUrl: String): List<NanoFontSummary>
    suspend fun listLocales(baseUrl: String): List<NanoLocaleSummary>
    suspend fun fetchSettings(baseUrl: String): NanoSettings
    suspend fun updateReadingSettings(baseUrl: String, settings: NanoSettings.Reading)
    suspend fun updateDisplaySettings(baseUrl: String, settings: NanoSettings.Interface)
    suspend fun updateUpdateSettings(baseUrl: String, settings: NanoSettings.Updates)
    suspend fun selectTheme(baseUrl: String, id: String)
    suspend fun selectFont(baseUrl: String, id: String)
    suspend fun selectLocale(baseUrl: String, id: String)
    suspend fun fetchWifiSettings(baseUrl: String): NanoWifiSettings
    suspend fun updateWifi(baseUrl: String, ssid: String, password: String)
    suspend fun forgetWifi(baseUrl: String)
    suspend fun fetchRssFeeds(baseUrl: String): NanoRssFeeds
    suspend fun updateRssFeeds(baseUrl: String, config: NanoRssFeeds)
    suspend fun fetchFocusTimers(baseUrl: String): NanoFocusTimers
    suspend fun updateFocusTimers(baseUrl: String, timers: NanoFocusTimers)

    suspend fun uploadBook(
        baseUrl: String,
        name: String,
        data: ByteArray,
        category: String? = null,
        onProgress: ((sent: Long, total: Long) -> Unit)? = null,
    ): NanoBook

    suspend fun deleteBook(baseUrl: String, id: String)
    suspend fun setBookPosition(baseUrl: String, id: String, wordIndex: Int)
    suspend fun setBookLanguageFonts(
        baseUrl: String,
        id: String,
        languageFonts: List<NanoLanguageFont>,
    )

    suspend fun uploadTheme(
        baseUrl: String,
        name: String,
        data: ByteArray,
        onProgress: ((sent: Long, total: Long) -> Unit)? = null,
    ): NanoThemeSummary

    suspend fun deleteTheme(baseUrl: String, id: String)
    suspend fun uploadFont(
        baseUrl: String,
        name: String,
        data: ByteArray,
        onProgress: ((sent: Long, total: Long) -> Unit)? = null,
    ): NanoFontSummary

    suspend fun deleteFont(baseUrl: String, id: String)
    suspend fun uploadLocalePack(
        baseUrl: String,
        name: String,
        data: ByteArray,
        onProgress: ((sent: Long, total: Long) -> Unit)? = null,
    ): NanoLocaleSummary

    suspend fun deleteLocalePack(baseUrl: String, id: String)
}
