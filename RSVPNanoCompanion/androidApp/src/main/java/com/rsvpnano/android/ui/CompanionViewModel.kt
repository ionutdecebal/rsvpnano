package com.rsvpnano.android.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.rsvpnano.android.net.AndroidNanoNetworkController
import com.rsvpnano.android.net.NanoNetworkSnapshot
import com.rsvpnano.app.NanoDeviceSyncService
import com.rsvpnano.app.RsvpSharedApp
import com.rsvpnano.app.SharedAppUtils
import com.rsvpnano.converters.ImportPreparation
import com.rsvpnano.converters.RsvpConverter
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.models.RememberedNano
import com.rsvpnano.models.canAutoConnectToNano
import com.rsvpnano.persistence.AppSettingsStore
import java.net.URI
import java.util.UUID
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeout

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
    val isNanoWifiAttached: Boolean = false,
    val isCheckingReader: Boolean = false,
    val isRequestingNanoNetwork: Boolean = false,
    val nanoSsid: String? = null,
    val rememberedNano: RememberedNano? = null,
    val canRememberCurrentNano: Boolean = false,
    val showAddressEntry: Boolean = false,
    val isRefreshing: Boolean = false,
    val status: String = "Ready",
)

data class SharedImport(
    val title: String,
    val text: String,
    val source: String,
)

class CompanionViewModel(
    private val sharedApp: RsvpSharedApp,
    private val nanoNetworkController: AndroidNanoNetworkController,
    private val settingsStore: AppSettingsStore,
) : ViewModel() {
    private val deviceSyncService: NanoDeviceSyncService = sharedApp.deviceSyncService
    private val companionController = sharedApp.companionController
    private val _uiState = MutableStateFlow(CompanionUiState(status = "Loading shared data..."))
    val uiState: StateFlow<CompanionUiState> = _uiState
    private var pendingSettingsSave: NanoSettings? = null
    private var settingsSaveJob: Job? = null
    private var recheckJob: Job? = null
    private var articleFetchJob: Job? = null
    private val current: CompanionUiState
        get() = _uiState.value

    init {
        nanoNetworkController.start()
        viewModelScope.launch {
            val appSettings = settingsStore.load()
            updateState { 
                it.copy(
                    rememberedNano = appSettings.rememberedNano,
                    address = appSettings.defaultAddress,
                )
            }
            autoConnectIfPossible()
        }
        observeNanoNetwork()
        refresh()
    }

    fun setAddress(value: String) = updateState { it.copy(address = value) }

    fun showAddressEntry() = updateState {
        val shouldShowAddressEntry = !it.showAddressEntry
        it.copy(
            showAddressEntry = shouldShowAddressEntry,
            status = if (shouldShowAddressEntry) {
                "If the default address is not working, enter the address shown by the reader."
            } else {
                it.status
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
        setStatus("Starting RSVP Nano Wi-Fi scan...")
        requestNanoNetwork(rememberedNano = current.rememberedNano, manual = true)
    }

    fun testNanoScanStartingPermission() {
        setStatus("Starting RSVP Nano Wi-Fi scan...")
    }

    private fun autoConnectIfPossible() {
        viewModelScope.launch {
            val settings = settingsStore.load()
            val remembered = settings.rememberedNano ?: return@launch
            if (settings.canAutoConnectToNano(current.drafts)) {
                requestNanoNetwork(rememberedNano = remembered, manual = false)
            }
        }
    }

    fun scanPermissionDenied() {
        setStatus("Scan permission denied. Use Wi-Fi settings for the default connection flow.")
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
            updateState { it.copy(isRefreshing = true, status = "Refreshing...") }
            runCatching {
                val local = companionController.refreshLocal()
                updateState {
                    it.copy(
                        drafts = local.drafts,
                        rssFeeds = local.rssFeeds,
                        status = "Loaded ${local.drafts.size} drafts.",
                    )
                }
                if (!current.isConnected) {
                    setStatus("Ready. Join RSVP-Nano Wi-Fi, then tap Check.")
                } else {
                    verifyCurrentConnection()
                }
            }.onFailure { error ->
                updateState {
                    it.copy(status = error.message ?: "Refresh failed.")
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
        connect(showBusyStatus = true)
    }

    private fun connect(showBusyStatus: Boolean) {
        viewModelScope.launch {
            connectNow(showBusyStatus = showBusyStatus)
        }
    }

    private suspend fun connectNow(showBusyStatus: Boolean) {
        connectCurrentAddress(showBusyStatus = showBusyStatus, markFailure = true)
    }

    private suspend fun connectCurrentAddress(
        showBusyStatus: Boolean,
        markFailure: Boolean,
    ): Boolean {
        if (showBusyStatus) {
            setStatus("Connecting... Give the phone a few seconds after joining the Nano Wi-Fi.")
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
                "Could not find RSVP Nano at ${SharedAppUtils.DEFAULT_DEVICE_ADDRESS}. Use Connect to search, or open Wi-Fi settings and join the Nano AP manually."
            } else {
                "Connection failed."
            },
            showAddressEntry = current.showAddressEntry || address == SharedAppUtils.DEFAULT_DEVICE_ADDRESS,
        )
    }

    fun recheckConnectionAfterResume() {
        recheckJob?.cancel()
        recheckJob = viewModelScope.launch {
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
                setStatus("Nano Wi-Fi connected, reader API unavailable.")
            } else {
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

                val result = runCatching { withNanoApi { companionController.saveSettings(address, settingsToSave) } }
                if (result.isFailure) {
                    val error = result.exceptionOrNull()
                    pendingSettingsSave = null
                    markDisconnected(error?.message ?: "Reader disconnected before saving settings.")
                    break
                }

                val snapshot = result.getOrThrow()
                updateState { state ->
                    if (pendingSettingsSave == null && state.settings == settingsToSave) {
                        state.copy(settings = snapshot.settings, status = "Saved to Nano. Some changes apply after leaving Companion Sync.")
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
            runCatching { withNanoApi { companionController.saveWifiSettings(state.address, ssid, state.wifiPasswordDraft) } }
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
            runCatching { withNanoApi { companionController.clearWifiSettings(state.address) } }
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

    fun deleteRssFeed(feed: String) {
        viewModelScope.launch {
            val feeds = companionController.saveRssFeeds(
                baseUrl = current.address,
                feeds = current.rssFeeds.filterNot { it == feed },
                syncToDevice = false,
            ).rssFeeds
            updateState {
                it.copy(
                    rssFeeds = feeds,
                    status = "RSS feed removed.",
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

    fun fetchPendingArticlesWhenOnline() {
        if (articleFetchJob?.isActive == true) return
        articleFetchJob = viewModelScope.launch {
            val pending = current.drafts.filter(companionController::needsArticleFetch)
            if (pending.isEmpty()) return@launch

            var drafts = current.drafts
            var fetchedCount = 0
            pending.forEach { item ->
                val snapshot = companionController.saveDraftFetchingArticleIfNeeded(item)
                drafts = snapshot.drafts
                if (snapshot.fetchedArticle) {
                    fetchedCount += 1
                }
            }

            if (fetchedCount > 0) {
                updateState {
                    it.copy(
                        drafts = drafts,
                status = "Fetched $fetchedCount saved articles. Connect to the Nano Wi-Fi to sync.",
                    )
                }
            }
        }
    }

    fun rememberCurrentNano() {
        val identity = currentRememberableNano()
        if (identity == null) {
            setStatus("Connect to a Nano before remembering it.")
            return
        }
        viewModelScope.launch {
            val currentSettings = settingsStore.load()
            settingsStore.save(currentSettings.copy(rememberedNano = identity))
            updateState {
                it.copy(
                    rememberedNano = identity,
                    canRememberCurrentNano = false,
                    status = "Remembered ${identity.ssid}.",
                )
            }
        }
    }

    fun forgetRememberedNano() {
        viewModelScope.launch {
            val currentSettings = settingsStore.load()
            settingsStore.save(currentSettings.copy(rememberedNano = null))
            updateState {
                it.copy(
                    rememberedNano = null,
                    canRememberCurrentNano = currentRememberableNano() != null,
                    status = "Forgot remembered Nano.",
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
                "Shared link saved locally. It will fetch article text when the phone has internet again; then connect to the Nano Wi-Fi to sync."
            }
            else -> {
                "Saved $savedCount shared items locally. URL-only drafts will fetch when the phone has internet again."
            }
        }
    }

    fun saveLinkDraft() {
        viewModelScope.launch {
            val state = current
            val sourceUrl = state.draftSourceUrl.trim()
            if (!sourceUrl.startsWith("http://") && !sourceUrl.startsWith("https://")) {
                setStatus("Saved links need an http:// or https:// URL.")
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
                withNanoApi {
                    companionController.saveRssFeeds(
                        baseUrl = state.address,
                        feeds = state.rssFeeds,
                        syncToDevice = true,
                    )
                }
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
                withNanoApi { companionController.deleteBooks(state.address, listOf(book.id)) }
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
                withNanoApi {
                    companionController.uploadBook(
                        baseUrl = state.address,
                        file = file,
                        category = "book",
                    )
                }
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

    private fun observeNanoNetwork() {
        viewModelScope.launch {
            nanoNetworkController.snapshot.collect { snapshot ->
                onNanoNetworkSnapshot(snapshot)
            }
        }
    }

    private fun onNanoNetworkSnapshot(snapshot: NanoNetworkSnapshot) {
        val remembered = current.rememberedNano
        val currentIdentity = snapshot.identity ?: current.nanoSsid?.toRememberedNano()
        updateState {
            it.copy(
                isNanoWifiAttached = snapshot.isNanoWifi,
                isRequestingNanoNetwork = snapshot.isRequestingNano,
                nanoSsid = snapshot.ssid ?: it.nanoSsid,
                canRememberCurrentNano = currentIdentity != null && currentIdentity != remembered,
            )
        }
        
        if (snapshot.requestFailed) {
            setStatus(snapshot.requestFailureReason ?: "Could not connect to RSVP Nano Wi-Fi.")
        } else if (snapshot.isNanoWifi && !current.isConnected && !current.isCheckingReader) {
            checkDefaultAddress(showBusyStatus = false)
        } else if (!snapshot.isNanoWifi && current.isConnected) {
            markDisconnected("Reader disconnected.")
        }
    }

    private fun checkDefaultAddress(showBusyStatus: Boolean = true) {
        viewModelScope.launch {
            updateState { it.copy(address = SharedAppUtils.DEFAULT_DEVICE_ADDRESS, isCheckingReader = true) }
            connectCurrentAddress(showBusyStatus = showBusyStatus, markFailure = true)
            updateState { it.copy(isCheckingReader = false) }
        }
    }

    private fun requestNanoNetwork(rememberedNano: RememberedNano?, manual: Boolean): Boolean {
        val pendingFetches = current.drafts.any(companionController::needsArticleFetch)
        if (!manual && pendingFetches) return false
        if (current.isRequestingNanoNetwork || current.isNanoWifiAttached) return false
        if (!nanoNetworkController.hasRequiredPermissions()) {
            if (manual) {
                setStatus("Scan needs nearby Wi-Fi and location permission. Use Wi-Fi settings for the default flow.")
            }
            return false
        }

        updateState {
            it.copy(
                status = rememberedNano?.let { nano -> "Connecting to remembered Nano ${nano.ssid}..." }
                    ?: "Searching for RSVP Nano Wi-Fi...",
            )
        }
        nanoNetworkController.requestNanoNetwork(rememberedNano)
        return true
    }

    private suspend fun refreshConnection(address: String, localRssFeeds: List<String>) {
        val snapshot = withTimeout(8_000) {
            companionController.connectWithRetry(address, localRssFeeds)
        }
        val device = snapshot.device
        val deviceName = device.info?.name ?: "RSVP Nano"
        val apiIdentity = device.info?.networkSsid?.toRememberedNano()
        val currentIdentity = nanoNetworkController.snapshot.value.identity ?: apiIdentity
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
                nanoSsid = currentIdentity?.ssid ?: it.nanoSsid,
                canRememberCurrentNano = currentIdentity != null && currentIdentity != it.rememberedNano,
                showAddressEntry = false,
                status = "Connected to $deviceName. ${device.summaryText}",
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
                    status = "Reader settings loaded.",
                )
            }
        }.onFailure { error ->
            updateState {
                it.copy(status = "Connected, but reader settings could not be loaded: ${error.message ?: "unknown error"}.")
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
                isConnected = false,
                showAddressEntry = showAddressEntry,
                status = status,
            )
        }
    }

    private fun updateState(transform: (CompanionUiState) -> CompanionUiState) {
        _uiState.update(transform)
    }

    private suspend fun <T> withNanoApi(block: suspend () -> T): T {
        return nanoNetworkController.withNanoNetwork(block)
    }

    private fun currentRememberableNano(): RememberedNano? {
        return nanoNetworkController.snapshot.value.identity ?: current.nanoSsid?.toRememberedNano()
    }

    private fun String.toRememberedNano(): RememberedNano? {
        val clean = trim().trim('"')
        return clean.takeIf { it.isNanoSsid() }?.let { RememberedNano(ssid = it) }
    }

    private fun String.isNanoSsid(): Boolean {
        return startsWith(NANO_SSID_PREFIX) || startsWith(LEGACY_NANO_SSID_PREFIX)
    }

    fun releaseNanoForInternetWork() {
        nanoNetworkController.releaseRequestedNanoNetwork()
        if (current.isConnected) {
            markDisconnected("Disconnected from RSVP Nano to use phone internet.")
        }
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

    class Factory(
        private val sharedApp: RsvpSharedApp,
        private val nanoNetworkController: AndroidNanoNetworkController,
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
        const val DEFAULT_DEVICE_ADDRESS = "http://192.168.4.1"
        const val MIN_REFRESH_INDICATOR_MS = 650L
        const val NANO_SSID_PREFIX = "RSVP-Nano-"
        const val LEGACY_NANO_SSID_PREFIX = "RSVP_Nano-"
    }
}
