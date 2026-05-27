package com.rsvpnano.android.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.rsvpnano.app.CompanionNotice
import com.rsvpnano.app.NanoConnectionState
import com.rsvpnano.app.NanoDeviceSyncService
import com.rsvpnano.app.NanoWifiConnector
import com.rsvpnano.app.NanoWifiEvent
import com.rsvpnano.app.NanoWifiIdentity
import com.rsvpnano.app.NanoWifiRequestResult
import com.rsvpnano.app.NanoWifiSnapshot
import com.rsvpnano.app.RsvpSharedApp
import com.rsvpnano.app.SharedAppUtils
import com.rsvpnano.converters.ImportPreparation
import com.rsvpnano.converters.RsvpConverter
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.models.RememberedNano
import com.rsvpnano.persistence.AppSettingsStore
import java.net.URI
import java.util.UUID
import kotlinx.coroutines.Job
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout

class CompanionViewModel(
    private val sharedApp: RsvpSharedApp,
    private val nanoNetworkController: NanoWifiConnector,
    private val settingsStore: AppSettingsStore,
) : ViewModel() {
    private val deviceSyncService: NanoDeviceSyncService = sharedApp.deviceSyncService
    private val companionController = sharedApp.companionController
    private val _uiState = MutableStateFlow(CompanionUiState(notice = CompanionNotice.Neutral("Loading shared data...")))
    val uiState: StateFlow<CompanionUiState> = _uiState
    private var pendingSettingsSave: NanoSettings? = null
    private var settingsSaveJob: Job? = null
    private var settingsSaveStatusJob: Job? = null
    private var recheckJob: Job? = null
    private var connectionCheckJob: Job? = null
    private var articleFetchJob: Job? = null
    private var suppressedRememberPrompt: RememberedNano? = null
    private val current: CompanionUiState
        get() = _uiState.value

    init {
        nanoNetworkController.start()
        viewModelScope.launch {
            val appSettings = withContext(Dispatchers.IO) { settingsStore.load() }
            updateState { 
                it.copy(
                    rememberedNano = appSettings.rememberedNano,
                    address = appSettings.defaultAddress,
                )
            }
        }
        observeNanoNetwork()
        observeNanoNetworkEvents()
        refresh()
    }

    fun setAddress(value: String) = updateState { it.copy(address = value) }

    fun showAddressEntry() = updateState {
        val shouldShowAddressEntry = !it.showAddressEntry
        it.copy(
            showAddressEntry = shouldShowAddressEntry,
            notice = if (shouldShowAddressEntry) {
                CompanionNotice.Neutral("If the default address is not working, enter the address shown by the reader.")
            } else {
                it.notice
            },
        )
    }

    fun connectDefault() {
        viewModelScope.launch {
            updateState { it.copy(address = SharedAppUtils.DEFAULT_DEVICE_ADDRESS) }
            connectCurrentAddress(showBusyStatus = true, markFailure = true)
        }
    }

    fun connectNanoScan() {
        connectNano(rememberedNano = current.rememberedNano)
    }

    fun scanPermissionDenied() {
        setNotice(CompanionNotice.Attention("Wi-Fi permission was not granted. Use the Wi-Fi panel to join your Nano manually."))
    }

    fun requestWifiPermissions() {
        setNotice(CompanionNotice.Attention("Grant Wi-Fi permission so the app can find your RSVP Nano."))
    }

    fun wifiPermissionsBlocked() {
        setNotice(CompanionNotice.Error("Wi-Fi permission is blocked. Enable it in app settings to let the app find your RSVP Nano."))
    }

    fun setWifiSsidDraft(value: String) = updateState { it.copy(wifiSsidDraft = value) }

    fun setWifiPasswordDraft(value: String) = updateState { it.copy(wifiPasswordDraft = value) }

    fun setDraftTitle(value: String) = updateState { it.copy(draftTitle = value) }

    fun setDraftSourceUrl(value: String) = updateState { it.copy(draftSourceUrl = value) }

    fun setDraftBody(value: String) = updateState { it.copy(draftBody = value) }

    fun setRssFeedDraft(value: String) = updateState { it.copy(rssFeedDraft = value) }

    fun refresh() {
        viewModelScope.launch {
            val startedAt = System.currentTimeMillis()
            updateState { it.copy(isRefreshing = true, notice = CompanionNotice.Neutral("Refreshing...")) }
            runCatching {
                val local = withContext(Dispatchers.IO) { companionController.refreshLocal() }
                updateState {
                    it.copy(
                        drafts = local.drafts,
                        rssFeeds = local.rssFeeds,
                        notice = CompanionNotice.Neutral("Loaded ${local.drafts.size} drafts."),
                    )
                }
                if (!current.isConnected) {
                    setNotice(CompanionNotice.Neutral("Ready. Connect to your Nano when you want to sync."))
                } else {
                    verifyCurrentConnection()
                }
            }.onFailure { error ->
                updateState {
                    it.copy(notice = CompanionNotice.Error(error.message ?: "Refresh failed."))
                }
            }.also {
                val elapsed = System.currentTimeMillis() - startedAt
                if (elapsed < MIN_REFRESH_INDICATOR_MS) {
                    delay(MIN_REFRESH_INDICATOR_MS - elapsed)
                }
                updateState { it.copy(isRefreshing = false) }
            }
        }
    }

    fun connect() {
        viewModelScope.launch {
            connectCurrentAddress(showBusyStatus = true, markFailure = true)
        }
    }

    private suspend fun connectCurrentAddress(
        showBusyStatus: Boolean,
        markFailure: Boolean,
    ): Boolean {
        if (showBusyStatus) {
            setNotice(CompanionNotice.Attention("Connecting... Give the phone a few seconds after joining the Nano Wi-Fi."))
        }
        val state = current
        val address = SharedAppUtils.normalizedAddress(state.address)
        return runCatching { withNanoApi { refreshConnection(address, state.rssFeeds) } }
            .isSuccess
            .also { success ->
                if (!success && markFailure) {
                    markConnectionFailure(address)
                }
            }
    }

    private fun markConnectionFailure(address: String) {
        markDisconnected(
            if (address == SharedAppUtils.DEFAULT_DEVICE_ADDRESS) {
                "Could not find RSVP Nano at ${SharedAppUtils.DEFAULT_DEVICE_ADDRESS}. Use Connect to search, or join the Nano Wi-Fi manually."
            } else {
                "Connection failed."
            },
            showAddressEntry = current.showAddressEntry || address == SharedAppUtils.DEFAULT_DEVICE_ADDRESS,
        )
    }

    fun recheckConnectionAfterResume() {
        recheckJob?.cancel()
        recheckJob = viewModelScope.launch {
            nanoNetworkController.refreshSnapshot()
            if (current.isConnected) {
                verifyCurrentConnection()
            }
        }
    }

    fun recheckConnectionAfterNetworkChange() {
        recheckJob?.cancel()
        recheckJob = viewModelScope.launch {
            if (current.isConnected) {
                verifyCurrentConnection()
            }
        }
    }

    private suspend fun verifyCurrentConnection() {
        val state = current
        if (!state.isConnected) return
        runCatching {
            withNanoApi {
                companionController.verifyReachableWithRetry(
                    baseUrl = SharedAppUtils.normalizedAddress(state.address),
                    attempts = 2,
                    retryDelayMillis = 300,
                )
            }
        }.onFailure {
            if (current.isNanoWifiAttached) {
                setNotice(CompanionNotice.Attention("Nano Wi-Fi is connected, but the reader is not responding."))
            } else {
                markDisconnected("Reader disconnected. Reconnect to your Nano before continuing.")
            }
        }
    }

    private suspend fun ensureReaderReachable(action: String): Boolean {
        val state = current
        if (!state.isConnected) {
            setNotice(CompanionNotice.Error("Connect to your Nano before $action."))
            return false
        }
        return runCatching {
            withNanoApi {
                companionController.verifyReachableWithRetry(
                    baseUrl = SharedAppUtils.normalizedAddress(state.address),
                    attempts = 1,
                    retryDelayMillis = 0,
                )
            }
        }.onFailure {
            markDisconnected("Reader disconnected. Reconnect to your Nano before $action.")
        }.isSuccess
    }

    fun updateSettings(transform: (NanoSettings) -> NanoSettings) {
        val state = current
        val currentSettings = state.settings
        if (!state.isConnected || currentSettings == null) {
            setNotice(CompanionNotice.Error("Connect to your Nano before saving settings."))
            return
        }

        val nextSettings = transform(currentSettings)
        settingsSaveStatusJob?.cancel()
        updateState {
            it.copy(
                settings = nextSettings,
                isSavingSettings = true,
                settingsSaveStatus = "Saving changes...",
                notice = CompanionNotice.Neutral("Saving reader settings..."),
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
                if (!ensureReaderReachable("saving settings")) {
                    pendingSettingsSave = null
                    updateState { it.copy(isSavingSettings = false, settingsSaveStatus = null) }
                    break
                }

                val result = runCatching { withNanoApi { companionController.saveSettings(address, settingsToSave) } }
                if (result.isFailure) {
                    val error = result.exceptionOrNull()
                    pendingSettingsSave = null
                    updateState { it.copy(isSavingSettings = false, settingsSaveStatus = null) }
                    markDisconnected(error?.message ?: "Reader disconnected before saving settings.")
                    break
                }

                val snapshot = result.getOrThrow()
                updateState { state ->
                    if (pendingSettingsSave == null && state.settings == settingsToSave) {
                        state.copy(
                            settings = snapshot.settings,
                            isSavingSettings = false,
                            settingsSaveStatus = "Saved on Nano. Exit Companion Sync to apply every setting.",
                            notice = CompanionNotice.Neutral("Saved to Nano. Some changes apply after leaving Companion Sync."),
                        )
                    } else {
                        state
                    }
                }
                if (pendingSettingsSave == null) {
                    scheduleSettingsSaveStatusClear()
                }
            }
        }
    }

    fun saveWifiSettings() {
        viewModelScope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before saving Wi-Fi."))
                return@launch
            }
            val ssid = state.wifiSsidDraft.trim()
            if (ssid.isEmpty()) {
                setNotice(CompanionNotice.Error("Wi-Fi SSID is required."))
                return@launch
            }
            setNotice(CompanionNotice.Attention("Saving Wi-Fi settings..."))
            if (!ensureReaderReachable("saving Wi-Fi")) return@launch
            runCatching { withNanoApi { companionController.saveWifiSettings(state.address, ssid, state.wifiPasswordDraft) } }
                .onSuccess { snapshot ->
                    val wifi = snapshot.wifiSettings
                    updateState {
                        it.copy(
                            wifiSettings = wifi,
                            wifiSsidDraft = wifi.ssid,
                            wifiPasswordDraft = "",
                            notice = CompanionNotice.Success("Wi-Fi settings saved."),
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
                setNotice(CompanionNotice.Error("Connect to your Nano before clearing Wi-Fi."))
                return@launch
            }
            setNotice(CompanionNotice.Attention("Clearing Wi-Fi settings..."))
            if (!ensureReaderReachable("clearing Wi-Fi")) return@launch
            runCatching { withNanoApi { companionController.clearWifiSettings(state.address) } }
                .onSuccess { snapshot ->
                    val wifi = snapshot.wifiSettings
                    updateState {
                        it.copy(
                            wifiSettings = wifi,
                            wifiSsidDraft = wifi.ssid,
                            wifiPasswordDraft = "",
                            notice = CompanionNotice.Success("Wi-Fi settings cleared."),
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
                setNotice(CompanionNotice.Error("RSS feed URLs must start with http:// or https://."))
                return@launch
            }
            val feeds = withContext(Dispatchers.IO) {
                companionController.saveRssFeeds(
                    baseUrl = state.address,
                    feeds = state.rssFeeds + feed,
                    syncToDevice = false,
                ).rssFeeds
            }
            updateState {
                it.copy(
                    rssFeeds = feeds,
                    rssFeedDraft = "",
                    notice = if (state.isConnected) {
                        CompanionNotice.Success("RSS feed saved locally. Sync to write it to the reader.")
                    } else {
                        CompanionNotice.Success("RSS feed saved locally.")
                    },
                )
            }
        }
    }

    fun deleteRssFeed(feed: String) {
        viewModelScope.launch {
            val address = current.address
            val nextFeeds = current.rssFeeds.filterNot { it == feed }
            val feeds = withContext(Dispatchers.IO) {
                companionController.saveRssFeeds(
                    baseUrl = address,
                    feeds = nextFeeds,
                    syncToDevice = false,
                ).rssFeeds
            }
            updateState {
                it.copy(
                    rssFeeds = feeds,
                    notice = CompanionNotice.Success("RSS feed removed."),
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
                setNotice(CompanionNotice.Error("Text drafts need a title and body."))
                return@launch
            }
            val existing = state.editingDraftId?.let { id -> state.drafts.firstOrNull { it.id == id } }
            val snapshot = withContext(Dispatchers.IO) {
                companionController.saveDraft(
                    ImportPreparation.pendingUploadForText(
                        id = existing?.id ?: UUID.randomUUID().toString(),
                        title = title,
                        source = state.draftSourceUrl,
                        text = body,
                        createdAt = existing?.createdAt ?: SharedAppUtils.nowIso8601(),
                        fallbackTitle = "Untitled",
                    )
                )
            }
            clearDraftEditor(
                drafts = snapshot.drafts,
                notice = if (existing == null) CompanionNotice.Success("Text draft saved locally.") else CompanionNotice.Success("Text draft updated."),
            )
        }
    }

    fun saveSharedImports(imports: List<SharedImport>) {
        viewModelScope.launch {
            val prepared = withContext(Dispatchers.Default) {
                imports.mapNotNull {
                    ImportPreparation.prepareSharedImport(
                        id = UUID.randomUUID().toString(),
                        title = it.title,
                        text = it.text,
                        source = it.source,
                        createdAt = SharedAppUtils.nowIso8601(),
                    )
                }
            }
            if (prepared.isEmpty()) {
                setNotice(CompanionNotice.Error("Shared item is not readable text or a URL."))
                return@launch
            }

            var drafts = current.drafts
            var fetchedCount = 0
            prepared.forEach { item ->
                val snapshot = withContext(Dispatchers.IO) {
                    companionController.saveDraftFetchingArticleIfNeeded(item)
                }
                drafts = snapshot.drafts
                if (snapshot.fetchedArticle) {
                    fetchedCount += 1
                }
            }
            updateState {
                it.copy(
                    drafts = drafts,
                    notice = sharedImportNotice(savedCount = prepared.size, fetchedCount = fetchedCount),
                )
            }
        }
    }

    fun fetchPendingArticlesWhenOnline() {
        if (articleFetchJob?.isActive == true) return
        articleFetchJob = viewModelScope.launch {
            val pending = current.drafts.filter(companionController::needsArticleFetch)
            if (pending.isEmpty()) return@launch

            var drafts = current.drafts
            var fetchedCount = 0
            pending.forEach { item ->
                val snapshot = withContext(Dispatchers.IO) {
                    companionController.saveDraftFetchingArticleIfNeeded(item)
                }
                drafts = snapshot.drafts
                if (snapshot.fetchedArticle) {
                    fetchedCount += 1
                }
            }

            if (fetchedCount > 0) {
                updateState {
                    it.copy(
                        drafts = drafts,
                        notice = CompanionNotice.Success("Fetched $fetchedCount saved articles. Connect to the Nano Wi-Fi to sync."),
                    )
                }
            }
        }
    }

    fun rememberCurrentNano() {
        val identity = currentRememberableNano()
        if (identity == null) {
            setNotice(CompanionNotice.Error("Connect to a Nano before remembering it."))
            return
        }
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                val currentSettings = settingsStore.load()
                settingsStore.save(currentSettings.copy(rememberedNano = identity))
            }
            suppressedRememberPrompt = null
            updateState {
                it.copy(
                    rememberedNano = identity,
                    canRememberCurrentNano = false,
                    notice = CompanionNotice.Success("Remembered ${identity.ssid}."),
                )
            }
        }
    }

    fun forgetRememberedNano() {
        viewModelScope.launch {
            val identity = currentRememberableNano()
            suppressedRememberPrompt = identity
            withContext(Dispatchers.IO) {
                val currentSettings = settingsStore.load()
                settingsStore.save(currentSettings.copy(rememberedNano = null))
            }
            updateState {
                it.copy(
                    rememberedNano = null,
                    canRememberCurrentNano = false,
                    notice = CompanionNotice.Success("Forgot remembered Nano."),
                )
            }
        }
    }

    private fun sharedImportNotice(savedCount: Int, fetchedCount: Int): CompanionNotice {
        return when {
            fetchedCount > 0 && savedCount == 1 -> {
                CompanionNotice.Success("Shared article fetched and saved. Connect to the Nano Wi-Fi when you are ready to sync it.")
            }
            fetchedCount > 0 -> {
                CompanionNotice.Success("Saved $savedCount shared items and fetched $fetchedCount articles. Connect to the Nano Wi-Fi to sync.")
            }
            savedCount == 1 -> {
                CompanionNotice.Attention("Shared link saved locally. It will fetch article text when the phone has internet again; then connect to the Nano Wi-Fi to sync.")
            }
            else -> {
                CompanionNotice.Attention("Saved $savedCount shared items locally. URL-only drafts will fetch when the phone has internet again.")
            }
        }
    }

    fun saveLinkDraft() {
        viewModelScope.launch {
            val state = current
            val sourceUrl = state.draftSourceUrl.trim()
            if (!sourceUrl.startsWith("http://") && !sourceUrl.startsWith("https://")) {
                setNotice(CompanionNotice.Error("Saved links need an http:// or https:// URL."))
                return@launch
            }
            val title = state.draftTitle.trim().ifEmpty { hostName(sourceUrl).ifEmpty { "Saved Article" } }
            val existing = state.editingDraftId?.let { id -> state.drafts.firstOrNull { it.id == id } }
            val pending = ImportPreparation.pendingUploadForUrl(
                id = existing?.id ?: UUID.randomUUID().toString(),
                title = title,
                source = sourceUrl,
                host = hostName(sourceUrl),
                createdAt = existing?.createdAt ?: SharedAppUtils.nowIso8601(),
            )
            val snapshot = withContext(Dispatchers.IO) {
                companionController.saveDraftFetchingArticleIfNeeded(pending)
            }
            clearDraftEditor(
                drafts = snapshot.drafts,
                notice = when {
                    snapshot.fetchedArticle -> {
                        CompanionNotice.Success("Fetched and saved ${snapshot.item.title}. Connect to the Nano Wi-Fi to sync it.")
                    }
                    existing == null -> {
                        CompanionNotice.Attention("Link saved locally. If article text was not fetched, edit it while online before syncing.")
                    }
                    else -> {
                        CompanionNotice.Attention("Link updated. If article text was not fetched, edit it while online before syncing.")
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
                notice = CompanionNotice.Neutral("Editing ${draft.title}."),
            )
        }
    }

    fun cancelDraftEdit() {
        clearDraftEditor(notice = CompanionNotice.Neutral("Edit cancelled."))
    }

    fun deleteDraft(draft: PendingUpload) {
        viewModelScope.launch {
            val drafts = withContext(Dispatchers.IO) {
                companionController.deleteDraft(draft).drafts
            }
            if (current.editingDraftId == draft.id) {
                clearDraftEditor(drafts = drafts, notice = CompanionNotice.Success("Draft deleted."))
            } else {
                updateState { it.copy(drafts = drafts, notice = CompanionNotice.Success("Draft deleted.")) }
            }
        }
    }

    fun syncRssFeeds() {
        viewModelScope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before syncing RSS feeds."))
                return@launch
            }
            setNotice(CompanionNotice.Attention("Syncing RSS feeds..."))
            if (!ensureReaderReachable("syncing RSS feeds")) return@launch
            runCatching {
                withNanoApi {
                    companionController.saveRssFeeds(
                        baseUrl = state.address,
                        feeds = state.rssFeeds,
                        syncToDevice = true,
                    )
                }
            }.onSuccess { rss ->
                updateState { it.copy(rssFeeds = rss.rssFeeds, notice = CompanionNotice.Success("RSS feeds synced to the reader.")) }
            }.onFailure { error ->
                markDisconnected(error.message ?: "Reader disconnected before syncing RSS feeds.")
            }
        }
    }

    fun syncSavedArticles() {
        viewModelScope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before syncing saved articles."))
                return@launch
            }
            val readyDrafts = state.drafts.filterNot(companionController::needsArticleFetch)
            if (readyDrafts.isEmpty()) {
                setNotice(CompanionNotice.Error("No fetched articles are ready. Share links while online, or paste article text before syncing."))
                return@launch
            }
            setNotice(CompanionNotice.Attention("Syncing saved articles..."))
            if (!ensureReaderReachable("syncing saved articles")) return@launch
            runCatching {
                withNanoApi {
                    companionController.syncPendingUploads(
                        baseUrl = state.address,
                        items = readyDrafts,
                    )
                }
            }.onSuccess { synced ->
                updateState {
                    it.copy(
                        drafts = synced.drafts,
                        books = synced.books,
                        notice = CompanionNotice.Success("Synced ${synced.syncedCount} saved articles."),
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
                setNotice(CompanionNotice.Error("Connect to your Nano before deleting books."))
                return@launch
            }
            val title = book.displayTitle
            setNotice(CompanionNotice.Attention("Deleting $title..."))
            if (!ensureReaderReachable("deleting books")) return@launch
            runCatching {
                withNanoApi { companionController.deleteBooks(state.address, listOf(book.id)) }
            }.onSuccess { snapshot ->
                updateState { it.copy(books = snapshot.books, notice = CompanionNotice.Success("Deleted $title.")) }
            }.onFailure { error -> markDisconnected(error.message ?: "Reader disconnected before deleting books.") }
        }
    }

    fun uploadSelectedFile(displayName: String, data: ByteArray) {
        viewModelScope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before uploading files."))
                return@launch
            }
            if (!ensureReaderReachable("uploading files")) return@launch
            updateBookJob(BookJob(active = BookJobStep.Convert, name = displayName))
            val file = runCatching {
                withContext(Dispatchers.Default) {
                    RsvpConverter.bookFile(data = data, filename = displayName)
                }
            }.onFailure { error ->
                updateState {
                    it.copy(
                        bookJob = null,
                        notice = CompanionNotice.Error(error.message ?: "Could not convert $displayName."),
                    )
                }
            }.getOrNull() ?: return@launch

            val jobName = file.title.ifBlank { displayName }
            updateBookJob(
                BookJob(
                    active = BookJobStep.Upload,
                    name = jobName,
                    done = listOf(BookJobStep.Convert),
                    progress = 0f,
                )
            )
            runCatching {
                withNanoApi {
                    companionController.uploadBook(
                        baseUrl = state.address,
                        file = file,
                        category = "book",
                        onProgress = { sent, total ->
                            updateBookJob(
                                BookJob(
                                    active = BookJobStep.Upload,
                                    name = jobName,
                                    done = listOf(BookJobStep.Convert),
                                    progress = uploadProgress(sent = sent, total = total),
                                )
                            )
                        },
                    )
                }
            }.onSuccess { snapshot ->
                val uploadedName = current.bookJob?.name ?: jobName
                updateState {
                    it.copy(
                        books = snapshot.books,
                        bookJob = null,
                        notice = CompanionNotice.Success("Uploaded $uploadedName."),
                    )
                }
            }.onFailure { error ->
                updateState { it.copy(bookJob = null) }
                markDisconnected(error.message ?: "Reader disconnected before uploading files.")
            }
        }
    }

    fun needsArticleFetch(draft: PendingUpload): Boolean = companionController.needsArticleFetch(draft)

    private fun clearDraftEditor(
        drafts: List<PendingUpload> = current.drafts,
        notice: CompanionNotice,
    ) {
        updateState {
            it.copy(
                drafts = drafts,
                draftTitle = "",
                draftSourceUrl = "",
                draftBody = "",
                editingDraftId = null,
                notice = notice,
            )
        }
    }

    private fun setNotice(notice: CompanionNotice) = updateState { it.copy(notice = notice) }

    private fun updateBookJob(bookJob: BookJob) = updateState { it.copy(bookJob = bookJob) }

    private fun observeNanoNetwork() {
        viewModelScope.launch {
            nanoNetworkController.snapshot.collect { snapshot ->
                onNanoNetworkSnapshot(snapshot)
            }
        }
    }

    private fun observeNanoNetworkEvents() {
        viewModelScope.launch {
            nanoNetworkController.events.collect { event ->
                when (event) {
                    NanoWifiEvent.RequestUnavailable -> {
                        setNotice(CompanionNotice.Error("Android did not find a matching RSVP-Nano Wi-Fi network."))
                    }
                }
            }
        }
    }

    private fun onNanoNetworkSnapshot(snapshot: NanoWifiSnapshot) {
        val stateBefore = current
        val remembered = stateBefore.rememberedNano
        val currentIdentity = nanoIdentity(snapshot)
        val canRemember = canPromptToRemember(currentIdentity, remembered)
        updateState {
            it.copy(
                connectionState = snapshot.toConnectionState(previous = it.connectionState),
                canRememberCurrentNano = canRemember,
            )
        }
        
        when {
            snapshot.isAttached && !stateBefore.isConnected && !stateBefore.isCheckingReader -> {
                checkDefaultAddress(showBusyStatus = false)
            }
            !snapshot.isAttached && (stateBefore.isConnected || stateBefore.isNanoWifiAttached) -> {
                markDisconnected("Reader disconnected.")
            }
        }
    }

    private fun checkDefaultAddress(showBusyStatus: Boolean = true) {
        if (connectionCheckJob?.isActive == true) return
        connectionCheckJob = viewModelScope.launch {
            updateState {
                it.copy(
                    address = SharedAppUtils.DEFAULT_DEVICE_ADDRESS,
                    connectionState = NanoConnectionState.CheckingReader(it.currentNano),
                )
            }
            try {
                connectCurrentAddress(showBusyStatus = showBusyStatus, markFailure = true)
            } finally {
                updateState {
                    if (it.connectionState is NanoConnectionState.CheckingReader) {
                        it.copy(connectionState = NanoConnectionState.WifiAttached(it.currentNano))
                    } else {
                        it
                    }
                }
            }
        }
    }

    private fun connectNano(rememberedNano: RememberedNano?) {
        updateState {
            it.copy(
                notice = CompanionNotice.Attention(
                    rememberedNano?.let { nano -> "Connecting to remembered Nano ${nano.ssid}..." }
                        ?: "Searching for RSVP Nano Wi-Fi...",
                ),
            )
        }
        when (val result = nanoNetworkController.requestNanoNetwork(rememberedNano)) {
            NanoWifiRequestResult.Started -> Unit
            NanoWifiRequestResult.AlreadyAttached -> {
                checkDefaultAddress(showBusyStatus = false)
            }
            NanoWifiRequestResult.AlreadyRequesting -> Unit
            NanoWifiRequestResult.MissingPermissions -> {
                setNotice(CompanionNotice.Error("Wi-Fi permission is needed to find your Nano from the app."))
            }
            NanoWifiRequestResult.Unsupported -> {
                setNotice(CompanionNotice.Error("Android 10 or newer is required for app Wi-Fi scan. Use Wi-Fi settings instead."))
            }
            is NanoWifiRequestResult.Failed -> {
                setNotice(CompanionNotice.Error(result.reason))
            }
        }
    }

    private suspend fun refreshConnection(address: String, localRssFeeds: List<String>) {
        val snapshot = withTimeout(8_000) {
            companionController.connectWithRetry(address, localRssFeeds)
        }
        val device = snapshot.device
        val deviceName = device.info?.name ?: "RSVP Nano"
        val apiIdentity = NanoWifiIdentity.rememberedNanoOrNull(device.info?.networkSsid)
        val currentIdentity = nanoNetworkController.snapshot.value.currentNano ?: apiIdentity
        updateState {
            val nextConnectionState = if (device.info != null) {
                NanoConnectionState.ReaderConnected(currentIdentity ?: it.currentNano)
            } else {
                it.connectionState
            }
            it.copy(
                books = device.books,
                settings = device.settings,
                wifiSettings = device.wifiSettings,
                wifiSsidDraft = device.wifiSettings?.ssid.orEmpty(),
                wifiPasswordDraft = "",
                address = address,
                rssFeeds = snapshot.rssFeeds,
                drafts = snapshot.drafts,
                connectionState = nextConnectionState,
                canRememberCurrentNano = canPromptToRemember(currentIdentity, it.rememberedNano),
                showAddressEntry = false,
                notice = CompanionNotice.Success("Connected to $deviceName. ${device.summaryText}"),
            )
        }
        if (device.info != null && device.settings == null) {
            fetchMissingSettings(address)
        }
    }

    private suspend fun fetchMissingSettings(address: String) {
        runCatching {
            withNanoApi { companionController.refreshSettings(address) }
        }.onSuccess { snapshot ->
            updateState {
                it.copy(
                    settings = snapshot.settings,
                    wifiSettings = snapshot.wifiSettings ?: it.wifiSettings,
                    wifiSsidDraft = snapshot.wifiSettings?.ssid ?: it.wifiSsidDraft,
                    notice = CompanionNotice.Success("Reader settings loaded."),
                )
            }
        }.onFailure { error ->
            updateState {
                it.copy(notice = CompanionNotice.Attention("Connected, but reader settings could not be loaded: ${error.message ?: "unknown error"}."))
            }
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
                connectionState = NanoConnectionState.Disconnected,
                showAddressEntry = showAddressEntry,
                isSavingSettings = false,
                settingsSaveStatus = null,
                bookJob = null,
                notice = CompanionNotice.Error(status),
            )
        }
    }

    private fun scheduleSettingsSaveStatusClear() {
        settingsSaveStatusJob?.cancel()
        settingsSaveStatusJob = viewModelScope.launch {
            delay(3_200)
            updateState {
                if (it.isSavingSettings) {
                    it
                } else {
                    it.copy(settingsSaveStatus = null)
                }
            }
        }
    }

    private fun updateState(transform: (CompanionUiState) -> CompanionUiState) {
        _uiState.update(transform)
    }

    private suspend fun <T> withNanoApi(block: suspend () -> T): T {
        return nanoNetworkController.withNanoNetwork(block)
    }

    private fun currentRememberableNano(): RememberedNano? {
        return current.currentNano ?: nanoIdentity(nanoNetworkController.snapshot.value)
    }

    private fun nanoIdentity(snapshot: NanoWifiSnapshot): RememberedNano? {
        return snapshot.currentNano ?: current.currentNano
    }

    private fun canPromptToRemember(
        currentNano: RememberedNano?,
        rememberedNano: RememberedNano?,
    ): Boolean {
        return currentNano != null &&
            currentNano != rememberedNano &&
            currentNano != suppressedRememberPrompt
    }

    override fun onCleared() {
        nanoNetworkController.stop()
        super.onCleared()
    }

    private fun hostName(url: String): String {
        return runCatching {
            URI(url).host.orEmpty().ifEmpty { url.substringAfter("://").substringBefore("/") }
        }.getOrDefault(url.substringAfter("://").substringBefore("/"))
    }

    private fun uploadProgress(sent: Long, total: Long): Float? {
        if (total <= 0L) return null
        return (sent.toFloat() / total.toFloat()).coerceIn(0f, 1f)
    }

    class Factory(
        private val sharedApp: RsvpSharedApp,
        private val nanoNetworkController: NanoWifiConnector,
    ) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T {
            return CompanionViewModel(
                sharedApp = sharedApp,
                nanoNetworkController = nanoNetworkController,
                settingsStore = sharedApp.appSettingsStore,
            ) as T
        }
    }

    private companion object {
        const val MIN_REFRESH_INDICATOR_MS = 650L
    }
}
