package com.rsvpnano.ui

import com.rsvpnano.app.CompanionNotice
import com.rsvpnano.app.NanoConnectionState
import com.rsvpnano.app.isCheckingReader
import com.rsvpnano.app.isConnected
import com.rsvpnano.app.isRequesting
import com.rsvpnano.app.isWifiAttached
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoCalibreSettings
import com.rsvpnano.models.NanoCalibreSchema
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.models.RememberedNano

data class CompanionUiState(
    val drafts: List<PendingUpload> = emptyList(),
    val rssFeeds: List<String> = emptyList(),
    val books: List<NanoBook> = emptyList(),
    val settings: NanoSettings? = null,
    val wifiSettings: NanoWifiSettings? = null,
    val address: String = "http://192.168.4.1",
    val wifiSsidDraft: String = "",
    val wifiPasswordDraft: String = "",
    val draftTitle: String = "",
    val draftSourceUrl: String = "",
    val draftBody: String = "",
    val editingDraftId: String? = null,
    val rssFeedDraft: String = "",
    // Calibre library sync draft fields (edited in the Settings tab, pushed to device)
    val calibreSettings: NanoCalibreSettings? = null,
    val calibreBaseUrlDraft: String = "",
    val calibreSearchQueryDraft: String = "",
    val calibreUsernameDraft: String = "",
    val calibrePasswordDraft: String = "",
    val calibreLibraryIdDraft: String = "",
    val calibreDeletionPolicyDraft: String = NanoCalibreSchema.DELETION_POLICY_KEEP,
    val calibreEnabledDraft: Boolean = false,
    val connectionState: NanoConnectionState = NanoConnectionState.Disconnected,
    val rememberedNano: RememberedNano? = null,
    val canRememberCurrentNano: Boolean = false,
    val showAddressEntry: Boolean = false,
    val isRefreshing: Boolean = false,
    val isSavingSettings: Boolean = false,
    val settingsSaveStatus: String? = null,
    val bookJob: BookJob? = null,
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

    val showSettingsSaveStatus: Boolean
        get() = isSavingSettings || settingsSaveStatus != null

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
