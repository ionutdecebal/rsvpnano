package com.rsvpnano.ui

import com.rsvpnano.app.CompanionNotice
import com.rsvpnano.app.NanoConnectionState
import com.rsvpnano.app.NanoEndpoint
import com.rsvpnano.app.SharedAppUtils
import com.rsvpnano.app.isCheckingReader
import com.rsvpnano.app.isConnected
import com.rsvpnano.app.isRequesting
import com.rsvpnano.app.isWifiAttached
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoStorageRepair
import com.rsvpnano.models.NanoThemeCatalogItem
import com.rsvpnano.models.NanoThemeSummary
import com.rsvpnano.models.NanoFontCatalogItem
import com.rsvpnano.models.NanoFontSummary
import com.rsvpnano.models.NanoFocusTimers
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.NanoLocaleSummary
import com.rsvpnano.models.NanoLocaleCatalogItem
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.models.RememberedNano

data class CompanionUiState(
    val drafts: List<PendingUpload> = emptyList(),
    val rssFeeds: List<String> = emptyList(),
    val focusTimers: NanoFocusTimers = NanoFocusTimers(emptyList()),
    val books: List<NanoBook> = emptyList(),
    val settings: NanoSettings? = null,
    val wifiSettings: NanoWifiSettings? = null,
    val baseUrl: String = SharedAppUtils.ACCESS_POINT_BASE_URL,
    val wifiSsidDraft: String = "",
    val wifiPasswordDraft: String = "",
    val draftTitle: String = "",
    val draftSourceUrl: String = "",
    val draftBody: String = "",
    val editingDraftId: String? = null,
    val rssFeedDraft: String = "",
    val connectionState: NanoConnectionState = NanoConnectionState.Disconnected,
    val rememberedNano: RememberedNano? = null,
    val firmwareVersion: String = "",
    val otaAsset: String = "",
    val firmwareNotificationsEnabled: Boolean = false,
    val discoveredNanos: List<NanoEndpoint> = emptyList(),
    val canRememberCurrentNano: Boolean = false,
    val loadingResources: Set<CompanionResource> = emptySet(),
    val loadedResources: Set<CompanionResource> = emptySet(),
    val isSavingSettings: Boolean = false,
    val isRepairingStorage: Boolean = false,
    val storageRepair: NanoStorageRepair? = null,
    val bookJob: BookJob? = null,
    val themeCatalog: List<NanoThemeCatalogItem> = emptyList(),
    val availableThemes: List<NanoThemeSummary> = emptyList(),
    val availableFonts: List<NanoFontSummary> = emptyList(),
    val availableLocales: List<NanoLocaleSummary> = emptyList(),
    val themeCatalogUrl: String = "",
    val fontCatalog: List<NanoFontCatalogItem> = emptyList(),
    val fontCatalogUrl: String = "",
    val localeCatalog: List<NanoLocaleCatalogItem> = emptyList(),
    val localeCatalogUrl: String = "",
    val catalogInstall: CatalogInstall? = null,
    val notice: CompanionNotice = CompanionNotice.Neutral("Ready"),
) {
    val status: String
        get() = notice.message

    val isConnected: Boolean
        get() = connectionState.isConnected

    val isNanoWifiAttached: Boolean
        get() = connectionState.isWifiAttached

    val isCheckingReader: Boolean
        get() = connectionState.isCheckingReader

    val isRequestingNanoNetwork: Boolean
        get() = connectionState.isRequesting

    val currentNano: RememberedNano?
        get() = connectionState.currentNano

    val nanoSsid: String?
        get() = currentNano?.ssid
}

data class SharedImport(
    val title: String,
    val text: String,
    val source: String,
)

enum class CatalogAsset(
    val label: String,
    val selectionLabel: String,
    val plural: String,
    val extension: String,
    val catalogPath: String,
) {
    Theme("theme", "theme", "themes", ".toml", "themes/index.json"),
    Font("font", "font", "fonts", ".rfont4", "fonts/index.json"),
    Locale("locale pack", "interface language", "locale packs", ".zip", "locale-packs/index.json"),
}

enum class CompanionResource {
    Drafts,
    Library,
    Settings,
    Wifi,
    Themes,
    Fonts,
    Locales,
    RssFeeds,
    FocusTimers,
}

enum class CatalogInstallStage(val label: String) {
    Downloading("Downloading"),
    Sending("Sending to reader"),
    Installing("Installing"),
}

data class CatalogInstall(
    val asset: CatalogAsset,
    val id: String,
    val stage: CatalogInstallStage,
    val progress: Float? = null,
)
