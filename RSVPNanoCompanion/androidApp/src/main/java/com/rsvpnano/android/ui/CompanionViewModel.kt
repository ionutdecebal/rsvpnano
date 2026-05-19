package com.rsvpnano.android.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.rsvpnano.app.NanoDeviceSyncService
import com.rsvpnano.app.RsvpSharedApp
import com.rsvpnano.app.SharedAppUtils
import com.rsvpnano.converters.ImportPreparation
import com.rsvpnano.converters.RsvpConverter
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.PendingUpload
import java.net.URI
import java.util.UUID
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

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
    val isConnected: Boolean = false,
    val showAddressEntry: Boolean = false,
    val status: String = "Ready",
)

data class SharedImport(
    val title: String,
    val text: String,
    val source: String,
)

class CompanionViewModel(
    private val sharedApp: RsvpSharedApp,
) : ViewModel() {
    private val deviceSyncService: NanoDeviceSyncService = sharedApp.deviceSyncService
    private val companionController = sharedApp.companionController
    private val _uiState = MutableStateFlow(CompanionUiState(status = "Loading shared data..."))
    val uiState: StateFlow<CompanionUiState> = _uiState
    private var pendingSettingsSave: NanoSettings? = null
    private var settingsSaveJob: Job? = null
    private val current: CompanionUiState
        get() = _uiState.value

    init {
        refresh()
    }

    fun setAddress(value: String) = updateState { it.copy(address = value) }

    fun showAddressEntry() = updateState {
        it.copy(
            showAddressEntry = true,
            status = "If the default address is not working, enter the address shown by the reader.",
        )
    }

    fun connectDefault() {
        updateState { it.copy(address = SharedAppUtils.DEFAULT_DEVICE_ADDRESS) }
        connect()
    }

    fun setWifiSsidDraft(value: String) = updateState { it.copy(wifiSsidDraft = value) }

    fun setWifiPasswordDraft(value: String) = updateState { it.copy(wifiPasswordDraft = value) }

    fun setDraftTitle(value: String) = updateState { it.copy(draftTitle = value) }

    fun setDraftSourceUrl(value: String) = updateState { it.copy(draftSourceUrl = value) }

    fun setDraftBody(value: String) = updateState { it.copy(draftBody = value) }

    fun setRssFeedDraft(value: String) = updateState { it.copy(rssFeedDraft = value) }

    fun refresh() {
        viewModelScope.launch {
            setStatus("Refreshing...")
            val local = companionController.refreshLocal()
            updateState {
                it.copy(
                    drafts = local.drafts,
                    rssFeeds = local.rssFeeds,
                    status = "Loaded ${local.drafts.size} drafts.",
                )
            }
            if (!current.isConnected) {
                connectSilently()
            }
        }
    }

    fun connect() {
        connect(showBusyStatus = true)
    }

    private fun connectSilently() {
        connect(showBusyStatus = false)
    }

    private fun connect(showBusyStatus: Boolean) {
        viewModelScope.launch {
            if (showBusyStatus) {
                setStatus("Connecting... Give the phone a few seconds after joining the Nano Wi-Fi.")
            }
            val state = current
            val address = SharedAppUtils.normalizedAddress(state.address)
            runCatching { refreshConnection(address, state.rssFeeds) }
                .onFailure { error ->
                    markDisconnected(
                        if (address == SharedAppUtils.DEFAULT_DEVICE_ADDRESS) {
                            "Could not find RSVP Nano at ${SharedAppUtils.DEFAULT_DEVICE_ADDRESS}. Check the Nano Wi-Fi, or enter the address shown on the reader."
                        } else {
                            error.message ?: "Connection failed."
                        },
                        showAddressEntry = current.showAddressEntry || address == SharedAppUtils.DEFAULT_DEVICE_ADDRESS,
                    )
                }
        }
    }

    fun recheckConnectionAfterResume() {
        viewModelScope.launch {
            val state = current
            if (!state.isConnected) {
                connect(showBusyStatus = false)
                return@launch
            }

            runCatching {
                companionController.verifyReachableWithRetry(SharedAppUtils.normalizedAddress(state.address))
            }.onFailure {
                markDisconnected("Reader disconnected. Reconnect to RSVP Nano before continuing.")
            }
        }
    }

    fun updateSettings(transform: (NanoSettings) -> NanoSettings) {
        val state = current
        val currentSettings = state.settings
        if (!state.isConnected || currentSettings == null) {
            setStatus("Connect to the reader before saving settings.")
            return
        }

        val nextSettings = transform(currentSettings)
        updateState {
            it.copy(
                settings = nextSettings,
                status = "Saving reader settings...",
            )
        }
        enqueueSettingsSave(nextSettings)
    }

    private fun enqueueSettingsSave(settings: NanoSettings) {
        pendingSettingsSave = settings
        if (settingsSaveJob?.isActive == true) {
            return
        }

        settingsSaveJob = viewModelScope.launch {
            while (true) {
                val settingsToSave = pendingSettingsSave ?: break
                pendingSettingsSave = null
                val address = current.address

                val result = runCatching { companionController.saveSettings(address, settingsToSave) }
                if (result.isFailure) {
                    val error = result.exceptionOrNull()
                    pendingSettingsSave = null
                    markDisconnected(error?.message ?: "Reader disconnected before saving settings.")
                    break
                }

                val snapshot = result.getOrThrow()
                updateState { state ->
                    if (pendingSettingsSave == null && state.settings == settingsToSave) {
                        state.copy(settings = snapshot.settings, status = "Reader settings saved.")
                    } else {
                        state
                    }
                }
            }
        }
    }

    fun saveWifiSettings() {
        viewModelScope.launch {
            val state = current
            if (!state.isConnected) {
                setStatus("Connect to the reader before saving Wi-Fi.")
                return@launch
            }
            val ssid = state.wifiSsidDraft.trim()
            if (ssid.isEmpty()) {
                setStatus("Wi-Fi SSID is required.")
                return@launch
            }
            setStatus("Saving Wi-Fi settings...")
            runCatching { companionController.saveWifiSettings(state.address, ssid, state.wifiPasswordDraft) }
                .onSuccess { snapshot ->
                    val wifi = snapshot.wifiSettings
                    updateState {
                        it.copy(
                            wifiSettings = wifi,
                            wifiSsidDraft = wifi.ssid,
                            wifiPasswordDraft = "",
                            status = "Wi-Fi settings saved.",
                        )
                    }
                }
                .onFailure { error -> markDisconnected(error.message ?: "Reader disconnected before saving Wi-Fi.") }
        }
    }

    fun clearWifiSettings() {
        viewModelScope.launch {
            val state = current
            if (!state.isConnected) {
                setStatus("Connect to the reader before clearing Wi-Fi.")
                return@launch
            }
            setStatus("Clearing Wi-Fi settings...")
            runCatching { companionController.clearWifiSettings(state.address) }
                .onSuccess { snapshot ->
                    val wifi = snapshot.wifiSettings
                    updateState {
                        it.copy(
                            wifiSettings = wifi,
                            wifiSsidDraft = wifi.ssid,
                            wifiPasswordDraft = "",
                            status = "Wi-Fi settings cleared.",
                        )
                    }
                }
                .onFailure { error -> markDisconnected(error.message ?: "Reader disconnected before clearing Wi-Fi.") }
        }
    }

    fun addRssFeed() {
        viewModelScope.launch {
            val state = current
            val feed = state.rssFeedDraft.trim()
            if (!feed.startsWith("http://") && !feed.startsWith("https://")) {
                setStatus("RSS feed URLs must start with http:// or https://.")
                return@launch
            }
            val feeds = companionController.saveRssFeeds(
                baseUrl = state.address,
                feeds = state.rssFeeds + feed,
                syncToDevice = false,
            ).rssFeeds
            updateState {
                it.copy(
                    rssFeeds = feeds,
                    rssFeedDraft = "",
                    status = if (state.isConnected) {
                        "RSS feed saved locally. Sync to write it to the reader."
                    } else {
                        "RSS feed saved locally."
                    },
                )
            }
        }
    }

    fun saveTextDraft() {
        viewModelScope.launch {
            val state = current
            val title = state.draftTitle.trim()
            val body = state.draftBody.trim()
            if (title.isEmpty() || body.isEmpty()) {
                setStatus("Text drafts need a title and body.")
                return@launch
            }
            val existing = state.editingDraftId?.let { id -> state.drafts.firstOrNull { it.id == id } }
            val snapshot = companionController.saveDraft(
                ImportPreparation.pendingUploadForText(
                    id = existing?.id ?: UUID.randomUUID().toString(),
                    title = title,
                    source = state.draftSourceUrl,
                    text = body,
                    createdAt = existing?.createdAt ?: SharedAppUtils.nowIso8601(),
                    fallbackTitle = "Untitled",
                )
            )
            clearDraftEditor(
                drafts = snapshot.drafts,
                status = if (existing == null) "Text draft saved locally." else "Text draft updated.",
            )
        }
    }

    fun saveSharedImports(imports: List<SharedImport>) {
        viewModelScope.launch {
            val prepared = imports.mapNotNull {
                ImportPreparation.prepareSharedImport(
                    id = UUID.randomUUID().toString(),
                    title = it.title,
                    text = it.text,
                    source = it.source,
                    createdAt = SharedAppUtils.nowIso8601(),
                )
            }
            if (prepared.isEmpty()) {
                setStatus("Shared item is not readable text or a URL.")
                return@launch
            }

            var drafts = current.drafts
            var fetchedCount = 0
            prepared.forEach { item ->
                val snapshot = companionController.saveDraftFetchingArticleIfNeeded(item)
                drafts = snapshot.drafts
                if (snapshot.fetchedArticle) {
                    fetchedCount += 1
                }
            }
            updateState {
                it.copy(
                    drafts = drafts,
                    status = sharedImportStatus(savedCount = prepared.size, fetchedCount = fetchedCount),
                )
            }
        }
    }

    private fun sharedImportStatus(savedCount: Int, fetchedCount: Int): String {
        return when {
            fetchedCount > 0 && savedCount == 1 -> {
                "Shared article fetched and saved. Connect to the Nano Wi-Fi when you are ready to sync it."
            }
            fetchedCount > 0 -> {
                "Saved $savedCount shared items and fetched $fetchedCount articles. Connect to the Nano Wi-Fi to sync."
            }
            savedCount == 1 -> {
                "Shared item saved locally. If it is a URL-only draft, edit it while online before syncing to the Nano."
            }
            else -> {
                "Saved $savedCount shared items locally. URL-only drafts need article text before syncing to the Nano."
            }
        }
    }

    fun saveLinkDraft() {
        viewModelScope.launch {
            val state = current
            val title = state.draftTitle.trim()
            val sourceUrl = state.draftSourceUrl.trim()
            if (title.isEmpty() || !sourceUrl.startsWith("http://") && !sourceUrl.startsWith("https://")) {
                setStatus("Saved links need a title and http:// or https:// URL.")
                return@launch
            }
            val existing = state.editingDraftId?.let { id -> state.drafts.firstOrNull { it.id == id } }
            val pending = ImportPreparation.pendingUploadForUrl(
                id = existing?.id ?: UUID.randomUUID().toString(),
                title = title,
                source = sourceUrl,
                host = hostName(sourceUrl),
                createdAt = existing?.createdAt ?: SharedAppUtils.nowIso8601(),
            )
            val snapshot = companionController.saveDraftFetchingArticleIfNeeded(pending)
            clearDraftEditor(
                drafts = snapshot.drafts,
                status = when {
                    snapshot.fetchedArticle -> {
                        "Fetched and saved ${snapshot.item.title}. Connect to the Nano Wi-Fi to sync it."
                    }
                    existing == null -> {
                        "Link saved locally. If article text was not fetched, edit it while online before syncing."
                    }
                    else -> {
                        "Link updated. If article text was not fetched, edit it while online before syncing."
                    }
                },
            )
        }
    }

    fun editDraft(draft: PendingUpload) {
        updateState {
            it.copy(
                draftTitle = draft.title,
                draftSourceUrl = draft.sourceUrl.orEmpty(),
                draftBody = draft.body,
                editingDraftId = draft.id,
                status = "Editing ${draft.title}.",
            )
        }
    }

    fun cancelDraftEdit() {
        clearDraftEditor(status = "Edit cancelled.")
    }

    fun deleteDraft(draft: PendingUpload) {
        viewModelScope.launch {
            val drafts = companionController.deleteDraft(draft).drafts
            if (current.editingDraftId == draft.id) {
                clearDraftEditor(drafts = drafts, status = "Draft deleted.")
            } else {
                updateState { it.copy(drafts = drafts, status = "Draft deleted.") }
            }
        }
    }

    fun syncRssFeeds() {
        viewModelScope.launch {
            val state = current
            if (!state.isConnected) {
                setStatus("Connect to the reader before syncing RSS feeds.")
                return@launch
            }
            setStatus("Syncing RSS feeds...")
            runCatching {
                companionController.saveRssFeeds(
                    baseUrl = state.address,
                    feeds = state.rssFeeds,
                    syncToDevice = true,
                )
            }.onSuccess { rss ->
                updateState { it.copy(rssFeeds = rss.rssFeeds, status = "RSS feeds synced to the reader.") }
            }.onFailure { error ->
                markDisconnected(error.message ?: "Reader disconnected before syncing RSS feeds.")
            }
        }
    }

    fun syncSavedArticles() {
        viewModelScope.launch {
            val state = current
            if (!state.isConnected) {
                setStatus("Connect to the reader before syncing saved articles.")
                return@launch
            }
            val readyDrafts = state.drafts.filterNot(companionController::needsArticleFetch)
            if (readyDrafts.isEmpty()) {
                setStatus("No fetched articles are ready. Share links while online, or paste article text before syncing.")
                return@launch
            }
            setStatus("Syncing saved articles...")
            runCatching {
                companionController.syncPendingUploads(
                    baseUrl = state.address,
                    items = readyDrafts,
                )
            }.onSuccess { synced ->
                updateState {
                    it.copy(
                        drafts = synced.drafts,
                        books = synced.books,
                        status = "Synced ${synced.syncedCount} saved articles.",
                    )
                }
            }.onFailure { error ->
                markDisconnected(error.message ?: "Reader disconnected before syncing saved articles.")
            }
        }
    }

    fun deleteDeviceBook(book: NanoBook) {
        viewModelScope.launch {
            val state = current
            if (!state.isConnected) {
                setStatus("Connect to the reader before deleting books.")
                return@launch
            }
            val title = book.displayTitle
            setStatus("Deleting $title...")
            runCatching {
                companionController.deleteBooks(state.address, listOf(book.id))
            }.onSuccess { snapshot ->
                updateState { it.copy(books = snapshot.books, status = "Deleted $title.") }
            }.onFailure { error -> markDisconnected(error.message ?: "Reader disconnected before deleting books.") }
        }
    }

    fun uploadSelectedFile(displayName: String, data: ByteArray) {
        viewModelScope.launch {
            val state = current
            if (!state.isConnected) {
                setStatus("Connect to the reader before uploading files.")
                return@launch
            }
            setStatus("Uploading $displayName...")
            runCatching {
                val file = RsvpConverter.bookFile(data = data, filename = displayName)
                companionController.uploadBook(
                    baseUrl = state.address,
                    file = file,
                    category = "book",
                )
            }.onSuccess { snapshot ->
                updateState { it.copy(books = snapshot.books, status = "Uploaded $displayName.") }
            }.onFailure { error -> markDisconnected(error.message ?: "Reader disconnected before uploading files.") }
        }
    }

    fun needsArticleFetch(draft: PendingUpload): Boolean = companionController.needsArticleFetch(draft)

    private fun clearDraftEditor(
        drafts: List<PendingUpload> = current.drafts,
        status: String,
    ) {
        updateState {
            it.copy(
                drafts = drafts,
                draftTitle = "",
                draftSourceUrl = "",
                draftBody = "",
                editingDraftId = null,
                status = status,
            )
        }
    }

    private fun setStatus(status: String) = updateState { it.copy(status = status) }

    private suspend fun refreshConnection(address: String, localRssFeeds: List<String>) {
        val snapshot = companionController.connectWithRetry(address, localRssFeeds)
        val device = snapshot.device
        val deviceName = device.info?.name ?: "RSVP Nano"
        updateState {
            it.copy(
                books = device.books,
                settings = device.settings,
                wifiSettings = device.wifiSettings,
                wifiSsidDraft = device.wifiSettings?.ssid.orEmpty(),
                wifiPasswordDraft = "",
                address = address,
                rssFeeds = snapshot.rssFeeds,
                drafts = snapshot.drafts,
                isConnected = device.info != null,
                showAddressEntry = false,
                status = "Connected to $deviceName. ${device.summaryText}",
            )
        }
    }

    private fun markDisconnected(
        status: String,
        showAddressEntry: Boolean = false,
    ) {
        updateState {
            it.copy(
                books = emptyList(),
                settings = null,
                wifiSettings = null,
                isConnected = false,
                showAddressEntry = showAddressEntry,
                status = status,
            )
        }
    }

    private fun updateState(transform: (CompanionUiState) -> CompanionUiState) {
        _uiState.update(transform)
    }

    private fun hostName(url: String): String {
        return runCatching {
            URI(url).host.orEmpty().ifEmpty { url.substringAfter("://").substringBefore("/") }
        }.getOrDefault(url.substringAfter("://").substringBefore("/"))
    }

    class Factory(
        private val sharedApp: RsvpSharedApp,
    ) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T {
            return CompanionViewModel(sharedApp) as T
        }
    }

    private companion object {
        const val DEFAULT_DEVICE_ADDRESS = "http://192.168.4.1"
    }
}
