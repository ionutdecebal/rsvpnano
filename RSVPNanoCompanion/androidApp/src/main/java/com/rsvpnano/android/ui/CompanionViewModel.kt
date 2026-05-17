package com.rsvpnano.android.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.rsvpnano.app.NanoDeviceSyncService
import com.rsvpnano.app.RsvpSharedApp
import com.rsvpnano.converters.RsvpConverter
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.PendingUpload
import java.time.Instant
import java.util.UUID
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
    val settingsWpmDraft: String = "",
    val settingsBrightnessDraft: String = "",
    val wifiSsidDraft: String = "",
    val wifiPasswordDraft: String = "",
    val draftTitle: String = "",
    val draftSourceUrl: String = "",
    val draftBody: String = "",
    val editingDraftId: String? = null,
    val rssFeedDraft: String = "",
    val isConnected: Boolean = false,
    val status: String = "Ready",
)

class CompanionViewModel(
    private val sharedApp: RsvpSharedApp,
) : ViewModel() {
    private val deviceSyncService: NanoDeviceSyncService = sharedApp.deviceSyncService
    private val companionController = sharedApp.companionController
    private val _uiState = MutableStateFlow(CompanionUiState(status = "Loading shared data..."))
    val uiState: StateFlow<CompanionUiState> = _uiState

    init {
        refresh()
    }

    fun setAddress(value: String) = updateState { it.copy(address = value) }

    fun setSettingsWpmDraft(value: String) = updateState { it.copy(settingsWpmDraft = value) }

    fun setSettingsBrightnessDraft(value: String) = updateState { it.copy(settingsBrightnessDraft = value) }

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
        }
    }

    fun connect() {
        viewModelScope.launch {
            setStatus("Connecting...")
            val state = current
            runCatching { companionController.connect(state.address, state.rssFeeds) }
                .onSuccess { snapshot ->
                    val device = snapshot.device
                    val deviceName = device.info?.name ?: "RSVP Nano"
                    updateState {
                        it.copy(
                            books = device.books,
                            settings = device.settings,
                            wifiSettings = device.wifiSettings,
                            settingsWpmDraft = device.settings?.reading?.wpm?.toString().orEmpty(),
                            settingsBrightnessDraft = device.settings?.display?.brightnessIndex?.toString().orEmpty(),
                            wifiSsidDraft = device.wifiSettings?.ssid.orEmpty(),
                            wifiPasswordDraft = "",
                            rssFeeds = snapshot.rssFeeds,
                            drafts = snapshot.drafts,
                            isConnected = device.info != null,
                            status = "Connected to $deviceName. Loaded ${device.books.size} books.",
                        )
                    }
                }
                .onFailure { error ->
                    updateState { it.copy(isConnected = false, status = error.message ?: "Connection failed.") }
                }
        }
    }

    fun saveDeviceSettings() {
        viewModelScope.launch {
            val state = current
            val currentSettings = state.settings
            if (!state.isConnected || currentSettings == null) {
                setStatus("Connect to the reader before saving settings.")
                return@launch
            }
            val wpm = state.settingsWpmDraft.toIntOrNull()
            val brightness = state.settingsBrightnessDraft.toIntOrNull()
            if (wpm == null || brightness == null) {
                setStatus("WPM and brightness must be whole numbers.")
                return@launch
            }
            val settings = currentSettings.copy(
                reading = currentSettings.reading.copy(wpm = wpm),
                display = currentSettings.display.copy(brightnessIndex = brightness),
            )
            setStatus("Saving reader settings...")
            runCatching { deviceSyncService.saveSettings(state.address, settings) }
                .onSuccess { saved ->
                    updateState {
                        it.copy(
                            settings = saved,
                            settingsWpmDraft = saved.reading.wpm.toString(),
                            settingsBrightnessDraft = saved.display.brightnessIndex.toString(),
                            status = "Reader settings saved.",
                        )
                    }
                }
                .onFailure { error -> setStatus(error.message ?: "Settings save failed.") }
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
            runCatching { deviceSyncService.saveWifiSettings(state.address, ssid, state.wifiPasswordDraft) }
                .onSuccess { wifi ->
                    updateState {
                        it.copy(
                            wifiSettings = wifi,
                            wifiSsidDraft = wifi.ssid,
                            wifiPasswordDraft = "",
                            status = "Wi-Fi settings saved.",
                        )
                    }
                }
                .onFailure { error -> setStatus(error.message ?: "Wi-Fi save failed.") }
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
            runCatching { deviceSyncService.clearWifiSettings(state.address) }
                .onSuccess { wifi ->
                    updateState {
                        it.copy(
                            wifiSettings = wifi,
                            wifiSsidDraft = wifi.ssid,
                            wifiPasswordDraft = "",
                            status = "Wi-Fi settings cleared.",
                        )
                    }
                }
                .onFailure { error -> setStatus(error.message ?: "Wi-Fi clear failed.") }
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
            sharedApp.facade.saveDraft(
                PendingUpload(
                    id = existing?.id ?: UUID.randomUUID().toString(),
                    title = title,
                    sourceUrl = state.draftSourceUrl.trim().ifEmpty { null },
                    body = body,
                    createdAt = existing?.createdAt ?: Instant.now().toString(),
                )
            )
            clearDraftEditor(
                drafts = sharedApp.facade.loadDrafts(),
                status = if (existing == null) "Text draft saved locally." else "Text draft updated.",
            )
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
            sharedApp.facade.saveDraft(
                PendingUpload(
                    id = existing?.id ?: UUID.randomUUID().toString(),
                    title = title,
                    sourceUrl = sourceUrl,
                    body = "",
                    createdAt = existing?.createdAt ?: Instant.now().toString(),
                )
            )
            clearDraftEditor(
                drafts = sharedApp.facade.loadDrafts(),
                status = if (existing == null) {
                    "Link draft saved locally. Fetch it before syncing."
                } else {
                    "Link draft updated. Fetch it before syncing."
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
            sharedApp.facade.deleteDraft(draft)
            val drafts = sharedApp.facade.loadDrafts()
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
                setStatus(error.message ?: "RSS sync failed.")
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
            val readyDrafts = state.drafts.filterNot(sharedApp.facade::needsArticleFetch)
            if (readyDrafts.isEmpty()) {
                setStatus("No text drafts are ready to sync.")
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
                setStatus(error.message ?: "Saved article sync failed.")
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
                deviceSyncService.deleteBook(state.address, book.id)
                deviceSyncService.refreshBooks(state.address)
            }.onSuccess { books ->
                updateState { it.copy(books = books, status = "Deleted $title.") }
            }.onFailure { error ->
                setStatus(error.message ?: "Book delete failed.")
            }
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
                deviceSyncService.uploadBook(
                    baseUrl = state.address,
                    filename = file.filename,
                    data = file.data,
                    category = "book",
                )
                deviceSyncService.refreshBooks(state.address)
            }.onSuccess { books ->
                updateState { it.copy(books = books, status = "Uploaded $displayName.") }
            }.onFailure { error ->
                setStatus(error.message ?: "File upload failed.")
            }
        }
    }

    fun fetchMissingArticles() {
        viewModelScope.launch {
            val missing = current.drafts.filter(sharedApp.facade::needsArticleFetch)
            if (missing.isEmpty()) {
                setStatus("No saved links need fetching.")
                return@launch
            }
            setStatus("Fetching ${missing.size} saved links...")
            runCatching {
                missing.forEach { draft ->
                    val article = sharedApp.facade.fetchArticle(title = draft.title, source = draft.sourceUrl.orEmpty())
                    sharedApp.facade.updateDraft(draft, article.title, article.text)
                }
                sharedApp.facade.loadDrafts()
            }.onSuccess { drafts ->
                updateState { it.copy(drafts = drafts, status = "Fetched ${missing.size} saved links.") }
            }.onFailure { error ->
                setStatus(error.message ?: "Article fetch failed.")
            }
        }
    }

    fun needsArticleFetch(draft: PendingUpload): Boolean = sharedApp.facade.needsArticleFetch(draft)

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

    private val current: CompanionUiState
        get() = _uiState.value

    private fun setStatus(status: String) = updateState { it.copy(status = status) }

    private fun updateState(transform: (CompanionUiState) -> CompanionUiState) {
        _uiState.update(transform)
    }

    class Factory(
        private val sharedApp: RsvpSharedApp,
    ) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T {
            return CompanionViewModel(sharedApp) as T
        }
    }
}
