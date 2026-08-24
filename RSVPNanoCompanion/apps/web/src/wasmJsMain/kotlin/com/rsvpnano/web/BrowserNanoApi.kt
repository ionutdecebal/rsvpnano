package com.rsvpnano.web

import com.rsvpnano.api.NanoApi
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoFocusTimers
import com.rsvpnano.models.NanoFontSummary
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoLanguageFont
import com.rsvpnano.models.NanoLocaleSummary
import com.rsvpnano.models.NanoRssFeeds
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoThemeSummary
import com.rsvpnano.models.NanoWifiSettings

internal class BrowserNanoApi(
    private val http: NanoApi,
    private val serial: NanoApi,
) : NanoApi {
    private fun transport(baseUrl: String): NanoApi = if (baseUrl.startsWith("usb://")) serial else http

    override fun close() {
        http.close()
        serial.close()
    }

    override suspend fun fetchDevice(baseUrl: String) = transport(baseUrl).fetchDevice(baseUrl)
    override suspend fun repairStorage(baseUrl: String) = transport(baseUrl).repairStorage(baseUrl)
    override suspend fun listLibrary(baseUrl: String) = transport(baseUrl).listLibrary(baseUrl)
    override suspend fun listThemes(baseUrl: String) = transport(baseUrl).listThemes(baseUrl)
    override suspend fun listFonts(baseUrl: String) = transport(baseUrl).listFonts(baseUrl)
    override suspend fun listLocales(baseUrl: String) = transport(baseUrl).listLocales(baseUrl)
    override suspend fun fetchSettings(baseUrl: String) = transport(baseUrl).fetchSettings(baseUrl)
    override suspend fun updateReadingSettings(baseUrl: String, settings: NanoSettings.Reading) =
        transport(baseUrl).updateReadingSettings(baseUrl, settings)
    override suspend fun updateDisplaySettings(baseUrl: String, settings: NanoSettings.Interface) =
        transport(baseUrl).updateDisplaySettings(baseUrl, settings)
    override suspend fun updateUpdateSettings(baseUrl: String, settings: NanoSettings.Updates) =
        transport(baseUrl).updateUpdateSettings(baseUrl, settings)
    override suspend fun selectTheme(baseUrl: String, id: String) = transport(baseUrl).selectTheme(baseUrl, id)
    override suspend fun selectFont(baseUrl: String, id: String) = transport(baseUrl).selectFont(baseUrl, id)
    override suspend fun selectLocale(baseUrl: String, id: String) = transport(baseUrl).selectLocale(baseUrl, id)
    override suspend fun fetchWifiSettings(baseUrl: String) = transport(baseUrl).fetchWifiSettings(baseUrl)
    override suspend fun updateWifi(baseUrl: String, ssid: String, password: String) =
        transport(baseUrl).updateWifi(baseUrl, ssid, password)
    override suspend fun forgetWifi(baseUrl: String) = transport(baseUrl).forgetWifi(baseUrl)
    override suspend fun fetchRssFeeds(baseUrl: String) = transport(baseUrl).fetchRssFeeds(baseUrl)
    override suspend fun updateRssFeeds(baseUrl: String, config: NanoRssFeeds) =
        transport(baseUrl).updateRssFeeds(baseUrl, config)
    override suspend fun fetchFocusTimers(baseUrl: String) = transport(baseUrl).fetchFocusTimers(baseUrl)
    override suspend fun updateFocusTimers(baseUrl: String, timers: NanoFocusTimers) =
        transport(baseUrl).updateFocusTimers(baseUrl, timers)
    override suspend fun uploadBook(baseUrl: String, name: String, data: ByteArray, category: String?, onProgress: ((Long, Long) -> Unit)?): NanoBook =
        transport(baseUrl).uploadBook(baseUrl, name, data, category, onProgress)
    override suspend fun deleteBook(baseUrl: String, id: String) = transport(baseUrl).deleteBook(baseUrl, id)
    override suspend fun setBookPosition(baseUrl: String, id: String, wordIndex: Int) =
        transport(baseUrl).setBookPosition(baseUrl, id, wordIndex)
    override suspend fun setBookLanguageFonts(baseUrl: String, id: String, languageFonts: List<NanoLanguageFont>) =
        transport(baseUrl).setBookLanguageFonts(baseUrl, id, languageFonts)
    override suspend fun uploadTheme(baseUrl: String, name: String, data: ByteArray, onProgress: ((Long, Long) -> Unit)?): NanoThemeSummary =
        transport(baseUrl).uploadTheme(baseUrl, name, data, onProgress)
    override suspend fun deleteTheme(baseUrl: String, id: String) = transport(baseUrl).deleteTheme(baseUrl, id)
    override suspend fun uploadFont(baseUrl: String, name: String, data: ByteArray, onProgress: ((Long, Long) -> Unit)?): NanoFontSummary =
        transport(baseUrl).uploadFont(baseUrl, name, data, onProgress)
    override suspend fun deleteFont(baseUrl: String, id: String) = transport(baseUrl).deleteFont(baseUrl, id)
    override suspend fun uploadLocalePack(baseUrl: String, name: String, data: ByteArray, onProgress: ((Long, Long) -> Unit)?): NanoLocaleSummary =
        transport(baseUrl).uploadLocalePack(baseUrl, name, data, onProgress)
    override suspend fun deleteLocalePack(baseUrl: String, id: String) = transport(baseUrl).deleteLocalePack(baseUrl, id)
}
