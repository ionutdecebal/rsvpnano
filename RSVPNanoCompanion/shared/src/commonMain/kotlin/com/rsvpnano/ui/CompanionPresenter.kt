package com.rsvpnano.ui

import com.rsvpnano.app.CompanionNotice
import com.rsvpnano.app.CompanionCatalogFile
import com.rsvpnano.app.FirmwareUpdates
import com.rsvpnano.app.NanoCompanionController
import com.rsvpnano.app.NanoConnectionState
import com.rsvpnano.app.NanoConnectionTransport
import com.rsvpnano.app.NanoEndpoint
import com.rsvpnano.app.NanoSettingsResource
import com.rsvpnano.app.NanoWifiConnector
import com.rsvpnano.app.NanoWifiEvent
import com.rsvpnano.app.NanoWifiIdentity
import com.rsvpnano.app.NanoWifiRequestResult
import com.rsvpnano.app.NanoWifiSnapshot
import com.rsvpnano.app.SharedAppUtils
import com.rsvpnano.app.catalogContentUrl
import com.rsvpnano.app.releaseSource
import com.rsvpnano.converters.ImportPreparation
import com.rsvpnano.converters.RsvpConverter
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoLanguageFont
import com.rsvpnano.models.NanoFocusTimers
import com.rsvpnano.models.NanoLocales
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoSettingsSchema
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.models.RememberedNano
import com.rsvpnano.models.needsArticleFetch
import com.rsvpnano.persistence.AppSettingsStore
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Job
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

class CompanionPresenter(
    private val companionController: NanoCompanionController,
    private val firmwareUpdates: FirmwareUpdates,
    private val nanoNetworkController: NanoWifiConnector,
    private val settingsStore: AppSettingsStore,
    private val scope: CoroutineScope,
) {
    private val _uiState = MutableStateFlow(CompanionUiState(notice = CompanionNotice.Neutral("Loading shared data...")))
    val uiState: StateFlow<CompanionUiState> = _uiState
    private var pendingSettingsSave: PendingSettingsSave? = null
    private var settingsSaveJob: Job? = null
    private var recheckJob: Job? = null
    private var connectionCheckJob: Job? = null
    private var articleFetchJob: Job? = null
    private var connectionGeneration = 0
    private var suppressedRememberPrompt: RememberedNano? = null
    private val nanoApiMutex = Mutex()
    private val current: CompanionUiState
        get() = _uiState.value

    init {
        nanoNetworkController.start()
        scope.launch {
            val appSettings = withContext(Dispatchers.Default) { settingsStore.load() }
            updateState { 
                it.copy(
                    rememberedNano = appSettings.rememberedNano,
                    firmwareNotificationsEnabled = appSettings.firmwareNotificationsEnabled,
                )
            }
        }
        observeNanoNetwork()
        observeNanoNetworkEvents()
        loadDrafts()
    }

    fun connectNanoScan(onWifiPermissionRequired: () -> Unit = {}) {
        if (connectionCheckJob?.isActive == true) return
        suppressedRememberPrompt = null
        connectionCheckJob = scope.launch {
            val rememberedNano = current.rememberedNano
            updateState {
                it.copy(
                    discoveredNanos = emptyList(),
                    connectionState = NanoConnectionState.CheckingReader(
                        rememberedNano,
                        NanoConnectionTransport.LocalNetwork,
                    ),
                    notice = CompanionNotice.Attention("Looking for your RSVP Nano..."),
                )
            }
            val endpoints = try {
                nanoNetworkController.discoverNanos()
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (error: Throwable) {
                updateState {
                    it.copy(
                        connectionState = NanoConnectionState.Disconnected,
                        notice = CompanionNotice.Error("Local discovery failed: ${failureDetail(error)}"),
                    )
                }
                return@launch
            }
            val endpoint = when {
                endpoints.isEmpty() -> {
                    updateState { it.copy(connectionState = NanoConnectionState.Disconnected) }
                    connectNano(rememberedNano, onWifiPermissionRequired)
                    return@launch
                }
                endpoints.size == 1 -> endpoints.first()
                else -> endpoints.firstOrNull { it.nano == rememberedNano }
            }
            if (endpoint == null) {
                updateState {
                    it.copy(
                        discoveredNanos = endpoints,
                        connectionState = NanoConnectionState.Disconnected,
                        notice = CompanionNotice.Attention("Choose which RSVP Nano to connect to."),
                    )
                }
                return@launch
            }

            connectLocalNano(endpoint)
        }
    }

    fun selectDiscoveredNano(endpoint: NanoEndpoint) {
        if (endpoint !in current.discoveredNanos) return
        connectionCheckJob = scope.launch { connectLocalNano(endpoint) }
    }

    fun connectEndpoint(
        endpoint: NanoEndpoint,
        transport: NanoConnectionTransport = NanoConnectionTransport.LocalNetwork,
    ) {
        connectionCheckJob?.cancel()
        suppressedRememberPrompt = null
        connectionCheckJob = scope.launch { connectEndpointNow(endpoint, transport) }
    }

    fun cancelNanoSelection() {
        updateState {
            it.copy(
                discoveredNanos = emptyList(),
                connectionState = NanoConnectionState.Disconnected,
                notice = CompanionNotice.Neutral("Connection cancelled."),
            )
        }
    }

    fun reportConnectionFailure(message: String) {
        markDisconnected(message)
    }

    private suspend fun connectLocalNano(endpoint: NanoEndpoint) =
        connectEndpointNow(endpoint, NanoConnectionTransport.LocalNetwork)

    private suspend fun connectEndpointNow(endpoint: NanoEndpoint, transport: NanoConnectionTransport) {
        updateState {
            it.copy(
                discoveredNanos = emptyList(),
                connectionState = NanoConnectionState.CheckingReader(
                    endpoint.nano,
                    transport,
                ),
                notice = CompanionNotice.Attention("Connecting to ${endpoint.nano.ssid}..."),
            )
        }
        try {
            refreshConnection(endpoint.baseUrl)
        } catch (cancelled: CancellationException) {
            throw cancelled
        } catch (error: Throwable) {
            markDisconnected("Found ${endpoint.nano.ssid}, but connection failed: ${failureDetail(error)}")
        }
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

    private fun loadDrafts() {
        if (!beginResourceLoad(CompanionResource.Drafts)) return
        scope.launch {
            runCatching {
                val drafts = withContext(Dispatchers.Default) { companionController.refreshLocal() }
                updateState {
                    it.copy(
                        drafts = drafts,
                        loadedResources = it.loadedResources + CompanionResource.Drafts,
                        notice = CompanionNotice.Neutral("Ready. Connect to your Nano when you want to sync."),
                    )
                }
            }.onFailure { error ->
                updateState {
                    it.copy(notice = CompanionNotice.Error(error.message ?: "Local drafts could not be loaded."))
                }
            }.also {
                finishResourceLoad(CompanionResource.Drafts)
            }
        }
    }

    fun refreshThemeCatalog() {
        scope.launch {
            val catalogUrl = runCatching { catalogUrl("themes/index.json") }.getOrNull() ?: return@launch
            runCatching {
                catalogUrl to companionController.fetchThemeCatalog(catalogUrl)
            }
                .onSuccess { (catalogUrl, themes) ->
                    updateState {
                        it.copy(
                            themeCatalog = themes,
                            themeCatalogUrl = catalogUrl,
                        )
                    }
                }
                .onFailure {
                    updateState { it.copy(themeCatalogUrl = catalogUrl) }
                }
        }
    }

    fun refreshFontCatalog() {
        scope.launch {
            val catalogUrl = runCatching { catalogUrl("fonts/index.json") }.getOrNull() ?: return@launch
            runCatching {
                catalogUrl to companionController.fetchFontCatalog(catalogUrl)
            }
                .onSuccess { (catalogUrl, fonts) ->
                    updateState {
                        it.copy(
                            fontCatalog = fonts,
                            fontCatalogUrl = catalogUrl,
                        )
                    }
                }
                .onFailure {
                    updateState { it.copy(fontCatalogUrl = catalogUrl) }
                }
        }
    }

    fun recheckConnectionAfterResume() {
        recheckJob?.cancel()
        recheckJob = scope.launch {
            nanoNetworkController.refreshSnapshot()
            if (current.isConnected) {
                verifyCurrentConnection()
            }
        }
    }

    fun recheckConnectionAfterNetworkChange() {
        recheckJob?.cancel()
        recheckJob = scope.launch {
            if (current.isConnected) {
                verifyCurrentConnection()
            }
        }
    }

    private suspend fun verifyCurrentConnection() {
        val state = current
        if (!state.isConnected) return
        try {
            refreshConnection(state.baseUrl)
        } catch (cancelled: CancellationException) {
            throw cancelled
        } catch (_: Throwable) {
            currentCoroutineContext().ensureActive()
            if (current.isNanoWifiAttached) {
                setNotice(CompanionNotice.Attention("Nano Wi-Fi is connected, but the reader is not responding."))
            } else {
                markDisconnected("Reader disconnected. Reconnect to your Nano before continuing.")
            }
        }
    }

    private fun ensureReaderConnected(action: String): Boolean {
        if (!current.isConnected) {
            setNotice(CompanionNotice.Error("Connect to your Nano before $action."))
            return false
        }
        return true
    }

    fun updateSettings(transform: (NanoSettings) -> NanoSettings) {
        val state = current
        val currentSettings = state.settings
        if (!state.isConnected || currentSettings == null) {
            setNotice(CompanionNotice.Error("Connect to your Nano before saving settings."))
            return
        }

        val nextSettings = transform(currentSettings)
        val resources = buildSet {
            if (currentSettings.reading != nextSettings.reading) add(NanoSettingsResource.Reading)
            if (currentSettings.`interface` != nextSettings.`interface`) add(NanoSettingsResource.Display)
            if (currentSettings.updates != nextSettings.updates) add(NanoSettingsResource.Updates)
        }
        if (resources.isEmpty()) return
        updateState {
            it.copy(
                settings = nextSettings,
                isSavingSettings = true,
                notice = CompanionNotice.Neutral("Saving reader settings..."),
            )
        }
        enqueueSettingsSave(nextSettings, resources)
    }

    fun selectTheme(id: String) = selectCatalogAsset(CatalogAsset.Theme, id)

    fun selectFont(id: String) = selectCatalogAsset(CatalogAsset.Font, id)

    fun selectLocale(id: String) = selectCatalogAsset(CatalogAsset.Locale, id)

    private fun selectCatalogAsset(asset: CatalogAsset, id: String) {
        val state = current
        val settings = state.settings
        if (!state.isConnected || settings == null) {
            setNotice(CompanionNotice.Error("Connect to your Nano before changing the ${asset.selectionLabel}."))
            return
        }
        val previousId = selectedCatalogAsset(asset, settings)
        if (previousId == id) return

        pendingSettingsSave = pendingSettingsSave?.let {
            it.copy(settings = updateSelectedCatalogAsset(asset, it.settings, id))
        }
        updateState {
            it.copy(
                settings = updateSelectedCatalogAsset(asset, settings, id),
                notice = CompanionNotice.Neutral("Applying ${asset.selectionLabel}..."),
            )
        }
        scope.launch {
            try {
                val selectedId = withNanoApi { selectCatalogAssetOnDevice(asset, state.baseUrl, id) }
                updateState { currentState ->
                    if (currentState.settings?.let { selectedCatalogAsset(asset, it) } != id) {
                        currentState
                    } else {
                        currentState.copy(
                            settings = currentState.settings.let {
                                updateSelectedCatalogAsset(asset, it, selectedId)
                            },
                            notice = CompanionNotice.Success(
                                "${asset.selectionLabel.replaceFirstChar(Char::uppercase)} applied.",
                            ),
                        )
                    }
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (error: Throwable) {
                var rolledBack = false
                updateState { currentState ->
                    val currentSettings = currentState.settings
                    if (currentSettings?.let { selectedCatalogAsset(asset, it) } != id) {
                        currentState
                    } else {
                        rolledBack = true
                        pendingSettingsSave = pendingSettingsSave?.let { pending ->
                            if (selectedCatalogAsset(asset, pending.settings) == id) {
                                pending.copy(
                                    settings = updateSelectedCatalogAsset(asset, pending.settings, previousId),
                                )
                            } else {
                                pending
                            }
                        }
                        currentState.copy(
                            settings = updateSelectedCatalogAsset(asset, currentSettings, previousId),
                        )
                    }
                }
                if (rolledBack) handleDeviceFailure(error, "Selecting ${asset.selectionLabel} failed")
            }
        }
    }

    fun refreshLibrary() {
        refreshResource(
            resource = CompanionResource.Library,
            failureMessage = "Loading the library failed",
            request = companionController::refreshLibrary,
        ) { state, books ->
            state.copy(
                books = books,
                notice = CompanionNotice.Success("Loaded ${librarySummary(books)}."),
            )
        }
    }

    fun repairStorage() {
        scope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before repairing its SD card."))
                return@launch
            }
            updateState {
                it.copy(
                    isRepairingStorage = true,
                    storageRepair = null,
                    notice = CompanionNotice.Attention("Checking and repairing the SD card..."),
                )
            }
            runCatching {
                withNanoApi { companionController.repairStorage(state.baseUrl) }
            }.onSuccess { report ->
                updateState {
                    it.copy(
                        isRepairingStorage = false,
                        storageRepair = report,
                        notice = if (report.healthy) {
                            CompanionNotice.Success("SD card repair finished successfully.")
                        } else {
                            CompanionNotice.Attention("SD card repair finished with items that need attention.")
                        },
                    )
                }
                refreshLibrary()
            }.onFailure { error ->
                updateState { it.copy(isRepairingStorage = false) }
                handleDeviceFailure(error, "Repairing the SD card failed")
            }
        }
    }

    fun refreshSettings() {
        refreshResource(
            resource = CompanionResource.Settings,
            failureMessage = "Loading reader settings failed",
            request = companionController::refreshSettings,
            afterApply = { settings ->
                firmwareUpdates.rememberDevice(current.firmwareVersion, current.otaAsset, settings)
            },
        ) { state, settings -> state.copy(settings = settings) }
    }

    fun refreshWifiSettings() {
        refreshResource(
            resource = CompanionResource.Wifi,
            failureMessage = "Loading Nano Wi-Fi settings failed",
            request = companionController::refreshWifiSettings,
        ) { state, wifi ->
            state.copy(
                wifiSettings = wifi,
                wifiSsidDraft = wifi.ssid,
                wifiPasswordDraft = "",
            )
        }
    }

    fun refreshThemes() {
        refreshResource(
            CompanionResource.Themes,
            "Loading installed themes failed",
            companionController::refreshThemes,
        ) { state, themes -> state.copy(availableThemes = themes) }
    }

    fun refreshFonts() {
        refreshResource(
            CompanionResource.Fonts,
            "Loading installed fonts failed",
            companionController::refreshFonts,
        ) { state, fonts -> state.copy(availableFonts = fonts) }
    }

    fun refreshLocales() {
        refreshResource(
            CompanionResource.Locales,
            "Loading installed languages failed",
            companionController::refreshLocales,
        ) { state, locales -> state.copy(availableLocales = locales) }
    }

    fun refreshFocusTimers() {
        refreshResource(
            CompanionResource.FocusTimers,
            "Loading focus timers failed",
            companionController::refreshFocusTimers,
        ) { state, timers -> state.copy(focusTimers = timers) }
    }

    fun setFirmwareNotificationsEnabled(enabled: Boolean) {
        scope.launch {
            firmwareUpdates.setNotificationsEnabled(enabled)
            updateState { it.copy(firmwareNotificationsEnabled = enabled) }
        }
    }

    private fun enqueueSettingsSave(settings: NanoSettings, resources: Set<NanoSettingsResource>) {
        pendingSettingsSave = pendingSettingsSave?.let { pending ->
            PendingSettingsSave(settings, pending.resources + resources)
        } ?: PendingSettingsSave(settings, resources)
        if (settingsSaveJob?.isActive == true) {
            return
        }

        settingsSaveJob = scope.launch {
            while (true) {
                val pending = pendingSettingsSave ?: break
                pendingSettingsSave = null
                val baseUrl = current.baseUrl
                val result = runCatching {
                    withNanoApi { companionController.saveSettings(baseUrl, pending.settings, pending.resources) }
                }
                if (result.isFailure) {
                    val error = result.exceptionOrNull()!!
                    pendingSettingsSave = null
                    updateState { it.copy(isSavingSettings = false) }
                    handleDeviceFailure(error, "Saving reader settings failed")
                    break
                }

                val savedSettings = result.getOrThrow()
                firmwareUpdates.rememberDevice(current.firmwareVersion, current.otaAsset, savedSettings)
                updateState { state ->
                    if (pendingSettingsSave != null) {
                        state
                    } else if (state.settings == pending.settings) {
                        state.copy(
                            settings = savedSettings,
                            isSavingSettings = false,
                            notice = CompanionNotice.Success("Reader settings applied."),
                        )
                    } else {
                        state.copy(isSavingSettings = false)
                    }
                }
            }
        }
    }

    fun saveWifiSettings() {
        scope.launch {
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
            runCatching { withNanoApi { companionController.saveWifiSettings(state.baseUrl, ssid, state.wifiPasswordDraft) } }
                .onSuccess { wifi ->
                    updateState {
                        it.copy(
                            wifiSettings = wifi,
                            wifiSsidDraft = wifi.ssid,
                            wifiPasswordDraft = "",
                            notice = CompanionNotice.Success("Wi-Fi settings saved."),
                        )
                    }
                }
                .onFailure { error -> handleDeviceFailure(error, "Saving Wi-Fi settings failed") }
        }
    }

    fun clearWifiSettings() {
        scope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before clearing Wi-Fi."))
                return@launch
            }
            setNotice(CompanionNotice.Attention("Clearing Wi-Fi settings..."))
            runCatching { withNanoApi { companionController.clearWifiSettings(state.baseUrl) } }
                .onSuccess {
                    updateState {
                        it.copy(
                            wifiSettings = NanoWifiSettings(ssid = ""),
                            wifiSsidDraft = "",
                            wifiPasswordDraft = "",
                            notice = CompanionNotice.Success("Wi-Fi settings cleared."),
                        )
                    }
                }
                .onFailure { error -> handleDeviceFailure(error, "Clearing Wi-Fi settings failed") }
        }
    }

    fun addRssFeed() {
        scope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before editing RSS feeds."))
                return@launch
            }
            if (CompanionResource.RssFeeds !in state.loadedResources) {
                setNotice(CompanionNotice.Error("Load the Nano RSS feeds before editing them."))
                return@launch
            }
            val feed = state.rssFeedDraft.trim()
            if (!feed.startsWith("http://") && !feed.startsWith("https://")) {
                setNotice(CompanionNotice.Error("RSS feed URLs must start with http:// or https://."))
                return@launch
            }
            setNotice(CompanionNotice.Attention("Saving RSS feed on Nano..."))
            runCatching {
                withNanoApi {
                    companionController.saveRssFeeds(
                        baseUrl = state.baseUrl,
                        feeds = state.rssFeeds + feed,
                    )
                }
            }.onSuccess { feeds ->
                updateState {
                    it.copy(
                        rssFeeds = feeds,
                        rssFeedDraft = "",
                        notice = CompanionNotice.Success("RSS feed saved on Nano."),
                    )
                }
            }.onFailure { error -> handleDeviceFailure(error, "Saving RSS feeds failed") }
        }
    }

    fun deleteRssFeed(feed: String) {
        scope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before editing RSS feeds."))
                return@launch
            }
            if (CompanionResource.RssFeeds !in state.loadedResources) {
                setNotice(CompanionNotice.Error("Load the Nano RSS feeds before editing them."))
                return@launch
            }
            val nextFeeds = state.rssFeeds.filterNot { it == feed }
            setNotice(CompanionNotice.Attention("Removing RSS feed from Nano..."))
            runCatching {
                withNanoApi {
                    companionController.saveRssFeeds(
                        baseUrl = state.baseUrl,
                        feeds = nextFeeds,
                    )
                }
            }.onSuccess { feeds ->
                updateState {
                    it.copy(
                        rssFeeds = feeds,
                        notice = CompanionNotice.Success("RSS feed removed from Nano."),
                    )
                }
            }.onFailure { error -> handleDeviceFailure(error, "Removing RSS feeds failed") }
        }
    }

    fun saveTextDraft() {
        scope.launch {
            val state = current
            val title = state.draftTitle.trim()
            val body = state.draftBody.trim()
            if (title.isEmpty() || body.isEmpty()) {
                setNotice(CompanionNotice.Error("Text drafts need a title and body."))
                return@launch
            }
            val existing = state.editingDraftId?.let { id -> state.drafts.firstOrNull { it.id == id } }
            val drafts = withContext(Dispatchers.Default) {
                companionController.saveDraft(
                    ImportPreparation.pendingUploadForText(
                        id = existing?.id ?: newId(),
                        title = title,
                        source = state.draftSourceUrl,
                        text = body,
                        createdAt = existing?.createdAt ?: SharedAppUtils.nowIso8601(),
                        fallbackTitle = "Untitled",
                    )
                )
            }
            clearDraftEditor(
                drafts = drafts,
                notice = if (existing == null) CompanionNotice.Success("Text draft saved locally.") else CompanionNotice.Success("Text draft updated."),
            )
        }
    }

    fun saveSharedImports(imports: List<SharedImport>) {
        scope.launch {
            val prepared = withContext(Dispatchers.Default) {
                imports.mapNotNull {
                    ImportPreparation.prepareSharedImport(
                        id = newId(),
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
                val snapshot = withContext(Dispatchers.Default) {
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
        articleFetchJob = scope.launch {
            val pending = current.drafts.filter(PendingUpload::needsArticleFetch)
            if (pending.isEmpty()) return@launch

            var drafts = current.drafts
            var fetchedCount = 0
            pending.forEach { item ->
                val snapshot = withContext(Dispatchers.Default) {
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
                        notice = CompanionNotice.Success("Fetched $fetchedCount saved articles. Connect to your Nano to sync."),
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
        scope.launch {
            withContext(Dispatchers.Default) {
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
        scope.launch {
            val identity = currentRememberableNano()
            suppressedRememberPrompt = identity
            withContext(Dispatchers.Default) {
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
                CompanionNotice.Success("Shared article fetched and saved. Connect to your Nano when you are ready to sync it.")
            }
            fetchedCount > 0 -> {
                CompanionNotice.Success("Saved $savedCount shared items and fetched $fetchedCount articles. Connect to your Nano to sync.")
            }
            savedCount == 1 -> {
                CompanionNotice.Attention("Shared link saved locally. It will fetch article text when the phone has internet again; then connect to your Nano to sync.")
            }
            else -> {
                CompanionNotice.Attention("Saved $savedCount shared items locally. URL-only drafts will fetch when the phone has internet again.")
            }
        }
    }

    fun saveLinkDraft() {
        scope.launch {
            val state = current
            val sourceUrl = state.draftSourceUrl.trim()
            if (!sourceUrl.startsWith("http://") && !sourceUrl.startsWith("https://")) {
                setNotice(CompanionNotice.Error("Saved links need an http:// or https:// URL."))
                return@launch
            }
            val title = state.draftTitle.trim().ifEmpty { hostName(sourceUrl).ifEmpty { "Saved Article" } }
            val existing = state.editingDraftId?.let { id -> state.drafts.firstOrNull { it.id == id } }
            val pending = ImportPreparation.pendingUploadForUrl(
                id = existing?.id ?: newId(),
                title = title,
                source = sourceUrl,
                host = hostName(sourceUrl),
                createdAt = existing?.createdAt ?: SharedAppUtils.nowIso8601(),
            )
            val snapshot = withContext(Dispatchers.Default) {
                companionController.saveDraftFetchingArticleIfNeeded(pending)
            }
            clearDraftEditor(
                drafts = snapshot.drafts,
                notice = when {
                    snapshot.fetchedArticle -> {
                        CompanionNotice.Success("Fetched and saved ${snapshot.item.title}. Connect to your Nano to sync it.")
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
        scope.launch {
            val drafts = withContext(Dispatchers.Default) {
                companionController.deleteDraft(draft)
            }
            if (current.editingDraftId == draft.id) {
                clearDraftEditor(drafts = drafts, notice = CompanionNotice.Success("Draft deleted."))
            } else {
                updateState { it.copy(drafts = drafts, notice = CompanionNotice.Success("Draft deleted.")) }
            }
        }
    }

    fun refreshRssFeeds() {
        refreshResource(
            CompanionResource.RssFeeds,
            "Refreshing RSS feeds failed",
            companionController::refreshRssFeeds,
        ) { state, feeds ->
            state.copy(rssFeeds = feeds, notice = CompanionNotice.Success("RSS feeds loaded from Nano."))
        }
    }

    fun syncSavedArticles() {
        scope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before syncing saved articles."))
                return@launch
            }
            val readyDrafts = state.drafts.filterNot(PendingUpload::needsArticleFetch)
            if (readyDrafts.isEmpty()) {
                setNotice(CompanionNotice.Error("No fetched articles are ready. Share links while online, or paste article text before syncing."))
                return@launch
            }
            setNotice(CompanionNotice.Attention("Syncing saved articles..."))
            runCatching {
                withNanoApi {
                    companionController.syncPendingUploads(
                        baseUrl = state.baseUrl,
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
            }.onFailure { error -> handleDeviceFailure(error, "Syncing saved articles failed") }
        }
    }

    fun deleteDeviceBook(book: NanoBook) {
        scope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before deleting books."))
                return@launch
            }
            val title = book.displayTitle
            setNotice(CompanionNotice.Attention("Deleting $title..."))
            runCatching {
                withNanoApi { companionController.deleteBooks(state.baseUrl, listOf(book.id)) }
            }.onSuccess {
                updateState {
                    it.copy(books = it.books.filterNot { candidate -> candidate.id == book.id },
                        notice = CompanionNotice.Success("Deleted $title."))
                }
            }.onFailure { error -> handleDeviceFailure(error, "Deleting books failed") }
        }
    }

    fun setBookPosition(book: NanoBook, wordIndex: Int) {
        scope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before setting book position."))
                return@launch
            }
            val title = book.displayTitle
            setNotice(CompanionNotice.Attention("Saving position for $title..."))
            runCatching {
                withNanoApi { companionController.setBookPosition(state.baseUrl, book, wordIndex) }
            }.onSuccess { updated ->
                updateState {
                    it.copy(books = it.books.withBook(updated),
                        notice = CompanionNotice.Success("Saved position for $title."))
                }
            }.onFailure { error -> handleDeviceFailure(error, "Saving book position failed") }
        }
    }

    fun saveFocusTimers(timers: NanoFocusTimers) {
        scope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before editing focus timers."))
                return@launch
            }
            setNotice(CompanionNotice.Attention("Saving focus timers..."))
            runCatching {
                withNanoApi { companionController.saveFocusTimers(state.baseUrl, timers) }
            }.onSuccess { saved ->
                updateState {
                    it.copy(
                        focusTimers = saved,
                        notice = CompanionNotice.Success("Focus timers saved."),
                    )
                }
            }.onFailure { error -> handleDeviceFailure(error, "Saving focus timers failed") }
        }
    }

    fun refreshLocaleCatalog() {
        scope.launch {
            val catalogUrl = runCatching { catalogUrl("locale-packs/index.json") }.getOrNull() ?: return@launch
            runCatching {
                catalogUrl to companionController.fetchLocaleCatalog(catalogUrl)
            }
                .onSuccess { (catalogUrl, locales) ->
                    updateState {
                        it.copy(
                            localeCatalog = locales,
                            localeCatalogUrl = catalogUrl,
                        )
                    }
                }
                .onFailure {
                    updateState { it.copy(localeCatalogUrl = catalogUrl) }
                }
        }
    }

    fun setBookLanguageFonts(book: NanoBook, languageFonts: List<NanoLanguageFont>) {
        scope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before configuring book fonts."))
                return@launch
            }
            val title = book.displayTitle
            setNotice(CompanionNotice.Attention("Saving language fonts for $title..."))
            runCatching {
                withNanoApi { companionController.setBookLanguageFonts(state.baseUrl, book, languageFonts) }
            }.onSuccess { updated ->
                updateState {
                    it.copy(books = it.books.withBook(updated),
                        notice = CompanionNotice.Success("Saved language fonts for $title."))
                }
            }.onFailure { error -> handleDeviceFailure(error, "Saving book fonts failed") }
        }
    }

    fun uploadSelectedFile(displayName: String, data: ByteArray) {
        scope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before uploading files."))
                return@launch
            }
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
                        baseUrl = state.baseUrl,
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
            }.onSuccess { uploaded ->
                val uploadedName = current.bookJob?.name ?: jobName
                updateState {
                    it.copy(
                        books = it.books.withBook(uploaded),
                        bookJob = null,
                        notice = CompanionNotice.Success("Uploaded $uploadedName."),
                    )
                }
            }.onFailure { error ->
                updateState { it.copy(bookJob = null) }
                handleDeviceFailure(error, "Uploading files failed")
            }
        }
    }

    fun uploadThemeFile(displayName: String, data: ByteArray) =
        uploadCatalogFile(CatalogAsset.Theme, displayName, data)

    fun uploadFontFile(displayName: String, data: ByteArray) =
        uploadCatalogFile(CatalogAsset.Font, displayName, data)

    fun installLocalePackFile(displayName: String, data: ByteArray) =
        uploadCatalogFile(CatalogAsset.Locale, displayName, data)

    fun installOnlineTheme(id: String) = installOnlineCatalogAsset(CatalogAsset.Theme, id)

    fun installOnlineFont(id: String) = installOnlineCatalogAsset(CatalogAsset.Font, id)

    fun installOnlineLocalePack(id: String) = installOnlineCatalogAsset(CatalogAsset.Locale, id)

    fun removeTheme(id: String) {
        scope.launch {
            val state = current
            if (!ensureReaderConnected("removing a theme")) return@launch
            if (!selectFallbackBeforeRemoval(CatalogAsset.Theme, id, state)) return@launch
            runCatching {
                withNanoApi { companionController.removeTheme(state.baseUrl, id) }
            }.onSuccess {
                updateState {
                    it.copy(
                        availableThemes = it.availableThemes.filterNot { theme -> theme.id == id },
                        notice = CompanionNotice.Success("Removed theme $id."),
                    )
                }
            }.onFailure { error ->
                handleDeviceFailure(error, "Removing theme failed")
            }
        }
    }

    fun removeFont(id: String) {
        scope.launch {
            val state = current
            if (!ensureReaderConnected("removing a font")) return@launch
            if (!selectFallbackBeforeRemoval(CatalogAsset.Font, id, state)) return@launch
            runCatching {
                withNanoApi { companionController.removeFont(state.baseUrl, id) }
            }.onSuccess {
                updateState {
                    it.copy(
                        availableFonts = it.availableFonts.filterNot { font -> font.id == id },
                        notice = CompanionNotice.Success("Removed font $id."),
                    )
                }
            }.onFailure { error ->
                handleDeviceFailure(error, "Removing font failed")
            }
        }
    }

    private fun uploadCatalogFile(asset: CatalogAsset, displayName: String, data: ByteArray) {
        scope.launch {
            installCatalogFile(asset, displayName, data)
        }
    }

    private fun installOnlineCatalogAsset(asset: CatalogAsset, id: String) {
        scope.launch {
            val state = current
            if (!state.isConnected) {
                setNotice(CompanionNotice.Error("Connect to your Nano before installing ${asset.plural}."))
                return@launch
            }
            val installedFont = state.availableFonts.firstOrNull { it.id == id }
            if (asset == CatalogAsset.Font && installedFont != null) {
                setNotice(CompanionNotice.Neutral("${installedFont.name} is already installed."))
                return@launch
            }
            val name = catalogAssetName(asset, state, id)
            if (name == null) {
                setNotice(CompanionNotice.Error("Load the online ${asset.plural} first."))
                return@launch
            }
            if (!beginCatalogInstall(asset, id)) return@launch

            try {
                setNotice(CompanionNotice.Attention("Downloading $name..."))
                val file = try {
                    downloadCatalogAsset(asset, state, id) { received, total ->
                        updateCatalogInstall(asset, id, CatalogInstallStage.Downloading, received, total)
                    }
                } catch (cancelled: CancellationException) {
                    throw cancelled
                } catch (error: Throwable) {
                    setNotice(
                        CompanionNotice.Error(
                            error.message ?: "${asset.label.replaceFirstChar(Char::uppercase)} download failed.",
                        ),
                    )
                    return@launch
                }
                installCatalogFile(asset, file.filename, file.data, id, name)
            } finally {
                finishCatalogInstall(asset, id)
            }
        }
    }

    private suspend fun downloadCatalogAsset(
        asset: CatalogAsset,
        state: CompanionUiState,
        id: String,
        onProgress: (received: Long, total: Long?) -> Unit,
    ): CompanionCatalogFile {
        val indexUrl = catalogIndexUrl(asset, state)
        return when (asset) {
            CatalogAsset.Theme -> companionController.downloadTheme(
                indexUrl,
                requireNotNull(state.themeCatalog.firstOrNull { it.id == id }),
                onProgress,
            )
            CatalogAsset.Font -> companionController.downloadFont(
                indexUrl,
                requireNotNull(state.fontCatalog.firstOrNull { it.id == id }),
                onProgress,
            )
            CatalogAsset.Locale -> companionController.downloadLocalePack(
                indexUrl,
                requireNotNull(state.localeCatalog.firstOrNull { it.id == id }),
                onProgress,
            )
        }
    }

    private suspend fun installCatalogFile(
        asset: CatalogAsset,
        displayName: String,
        data: ByteArray,
        catalogId: String? = null,
        noticeName: String = displayName,
    ) {
        val state = current
        if (!state.isConnected) {
            setNotice(CompanionNotice.Error("Connect to your Nano before installing ${asset.plural}."))
            return
        }
        if (!displayName.endsWith(asset.extension, ignoreCase = true)) {
            setNotice(
                CompanionNotice.Error(
                    "${asset.label.replaceFirstChar(Char::uppercase)} files must use the ${asset.extension} extension.",
                ),
            )
            return
        }
        if (catalogId != null) {
            updateCatalogInstall(asset, catalogId, CatalogInstallStage.Sending)
        }
        setNotice(CompanionNotice.Attention("Installing $noticeName..."))
        val onProgress = catalogId?.let { id ->
            { sent: Long, total: Long -> updateCatalogUpload(asset, id, sent, total) }
        }
        try {
            when (asset) {
                CatalogAsset.Theme -> {
                    val installed = withNanoApi {
                        companionController.uploadTheme(state.baseUrl, displayName, data, onProgress)
                    }
                    updateState {
                        it.copy(availableThemes = it.availableThemes.upsert(installed) { theme -> theme.id })
                    }
                }
                CatalogAsset.Font -> {
                    val installed = withNanoApi {
                        companionController.uploadFont(state.baseUrl, displayName, data, onProgress)
                    }
                    updateState {
                        it.copy(availableFonts = it.availableFonts.upsert(installed) { font -> font.id })
                    }
                }
                CatalogAsset.Locale -> {
                    val installed = withNanoApi {
                        companionController.installLocalePack(state.baseUrl, displayName, data, onProgress)
                    }
                    updateState {
                        it.copy(availableLocales = it.availableLocales.upsert(installed) { locale -> locale.id })
                    }
                }
            }
            setNotice(CompanionNotice.Success("Installed $noticeName."))
        } catch (cancelled: CancellationException) {
            throw cancelled
        } catch (error: Throwable) {
            handleDeviceFailure(error, "Installing ${asset.label} failed")
        }
    }

    fun removeLocalePack(id: String) {
        scope.launch {
            val state = current
            if (!ensureReaderConnected("removing a locale pack")) return@launch
            if (!selectFallbackBeforeRemoval(CatalogAsset.Locale, id, state)) return@launch
            runCatching {
                withNanoApi { companionController.removeLocalePack(state.baseUrl, id) }
            }.onSuccess {
                updateState {
                    it.copy(
                        availableLocales = it.availableLocales.filterNot { locale -> locale.id == id },
                        notice = CompanionNotice.Success("Removed locale pack $id."),
                    )
                }
            }.onFailure { error ->
                handleDeviceFailure(error, "Removing locale pack failed")
            }
        }
    }

    private suspend fun selectFallbackBeforeRemoval(
        asset: CatalogAsset,
        id: String,
        state: CompanionUiState,
    ): Boolean {
        val settings = state.settings ?: return false
        val fallback = when (asset) {
            CatalogAsset.Theme -> {
                if (settings.`interface`.selectedThemeId != id) return true
                NanoSettingsSchema.THEME_DEFAULT.takeIf { it != id }
            }
            CatalogAsset.Font -> {
                if (settings.reading.typography.fontId != id) return true
                state.availableFonts.firstOrNull { it.builtIn && it.id != id }?.id
            }
            CatalogAsset.Locale -> {
                val locale = state.availableLocales.firstOrNull { it.id == id }?.locale
                if (settings.`interface`.locale != locale) return true
                NanoLocales.DEFAULT
            }
        }
        if (fallback == null) {
            setNotice(CompanionNotice.Error("The built-in option cannot be removed."))
            return false
        }

        val selected = try {
            withNanoApi { selectCatalogAssetOnDevice(asset, state.baseUrl, fallback) }
        } catch (cancelled: CancellationException) {
            throw cancelled
        } catch (error: Throwable) {
            handleDeviceFailure(error, "Selecting a fallback failed")
            return false
        }
        pendingSettingsSave = pendingSettingsSave?.let { pending ->
            pending.copy(settings = updateSelectedCatalogAsset(asset, pending.settings, selected))
        }
        updateState { it.copy(settings = updateSelectedCatalogAsset(asset, settings, selected)) }
        return true
    }

    private fun selectedCatalogAsset(asset: CatalogAsset, settings: NanoSettings): String = when (asset) {
        CatalogAsset.Theme -> settings.`interface`.selectedThemeId
        CatalogAsset.Font -> settings.reading.typography.fontId
        CatalogAsset.Locale -> settings.`interface`.locale
    }

    private fun updateSelectedCatalogAsset(
        asset: CatalogAsset,
        settings: NanoSettings,
        id: String,
    ): NanoSettings = when (asset) {
        CatalogAsset.Theme -> settings.withThemeId(id)
        CatalogAsset.Font -> settings.withTypeface(id)
        CatalogAsset.Locale -> settings.withLocale(id)
    }

    private suspend fun selectCatalogAssetOnDevice(asset: CatalogAsset, baseUrl: String, id: String): String =
        when (asset) {
            CatalogAsset.Theme -> companionController.selectTheme(baseUrl, id)
            CatalogAsset.Font -> companionController.selectFont(baseUrl, id)
            CatalogAsset.Locale -> companionController.selectLocale(baseUrl, id)
        }

    private fun catalogAssetName(asset: CatalogAsset, state: CompanionUiState, id: String): String? = when (asset) {
        CatalogAsset.Theme -> state.themeCatalog.firstOrNull { it.id == id }?.name
        CatalogAsset.Font -> state.fontCatalog.firstOrNull { it.id == id }?.name
        CatalogAsset.Locale -> state.localeCatalog.firstOrNull { it.id == id }?.name
    }

    private fun catalogIndexUrl(asset: CatalogAsset, state: CompanionUiState): String = when (asset) {
        CatalogAsset.Theme -> state.themeCatalogUrl
        CatalogAsset.Font -> state.fontCatalogUrl
        CatalogAsset.Locale -> state.localeCatalogUrl
    }.ifBlank { catalogUrl(asset.catalogPath) }

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
        scope.launch {
            nanoNetworkController.snapshot.collect { snapshot ->
                onNanoNetworkSnapshot(snapshot)
            }
        }
    }

    private fun observeNanoNetworkEvents() {
        scope.launch {
            nanoNetworkController.events.collect { event ->
                when (event) {
                    NanoWifiEvent.RequestUnavailable -> {
                        setNotice(CompanionNotice.Error("No matching RSVP-Nano Wi-Fi network was found."))
                    }
                    NanoWifiEvent.NetworkChanged -> recheckConnectionAfterNetworkChange()
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
            snapshot.isAttached && !current.isConnected && !current.isCheckingReader -> {
                connectionCheckJob?.cancel()
                connectAccessPointApi()
            }
            !snapshot.isAttached && stateBefore.isNanoWifiAttached -> {
                markDisconnected("Reader disconnected.")
            }
        }
    }

    private fun connectAccessPointApi() {
        val state = current
        if (state.isConnected || state.isCheckingReader) return
        updateState {
            it.copy(
                baseUrl = SharedAppUtils.ACCESS_POINT_BASE_URL,
                connectionState = NanoConnectionState.CheckingReader(
                    it.currentNano,
                    NanoConnectionTransport.AccessPoint,
                ),
            )
        }
        connectionCheckJob = scope.launch {
            try {
                try {
                    refreshConnection(SharedAppUtils.ACCESS_POINT_BASE_URL)
                } catch (cancelled: CancellationException) {
                    throw cancelled
                } catch (error: Throwable) {
                    markDisconnected("Connected to Nano Wi-Fi, but connection failed: ${failureDetail(error)}")
                }
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

    private fun connectNano(
        rememberedNano: RememberedNano?,
        onWifiPermissionRequired: () -> Unit,
    ) {
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
                connectAccessPointApi()
            }
            NanoWifiRequestResult.AlreadyRequesting -> Unit
            NanoWifiRequestResult.MissingPermissions -> {
                onWifiPermissionRequired()
            }
            is NanoWifiRequestResult.Failed -> {
                setNotice(CompanionNotice.Error(result.reason))
            }
        }
    }

    private suspend fun refreshConnection(baseUrl: String) {
        val device = withNanoApi { companionController.connect(baseUrl) }
        currentCoroutineContext().ensureActive()
        val deviceName = "RSVP Nano"
        val currentIdentity = NanoWifiIdentity.rememberedNanoOrNull(device.ssid)
            ?: nanoNetworkController.snapshot.value.currentNano
            ?: current.currentNano
        val newConnection = !current.isConnected || current.baseUrl != baseUrl
        if (newConnection) connectionGeneration++
        updateState {
            val nextConnectionState = NanoConnectionState.ReaderConnected(
                currentIdentity ?: it.currentNano,
                it.connectionState.transport ?: NanoConnectionTransport.LocalNetwork,
            )
            it.copy(
                books = if (newConnection) emptyList() else it.books,
                settings = if (newConnection) null else it.settings,
                availableThemes = if (newConnection) emptyList() else it.availableThemes,
                availableFonts = if (newConnection) emptyList() else it.availableFonts,
                availableLocales = if (newConnection) emptyList() else it.availableLocales,
                focusTimers = if (newConnection) NanoFocusTimers(emptyList()) else it.focusTimers,
                rssFeeds = if (newConnection) emptyList() else it.rssFeeds,
                wifiSettings = if (newConnection) null else it.wifiSettings,
                wifiSsidDraft = if (newConnection) "" else it.wifiSsidDraft,
                wifiPasswordDraft = if (newConnection) "" else it.wifiPasswordDraft,
                firmwareVersion = device.firmwareVersion,
                otaAsset = device.otaAsset,
                baseUrl = baseUrl,
                connectionState = nextConnectionState,
                canRememberCurrentNano = canPromptToRemember(currentIdentity, it.rememberedNano),
                loadingResources = if (newConnection) {
                    it.loadingResources.intersect(setOf(CompanionResource.Drafts))
                } else {
                    it.loadingResources
                },
                loadedResources = if (newConnection) {
                    it.loadedResources.intersect(setOf(CompanionResource.Drafts))
                } else {
                    it.loadedResources
                },
                notice = CompanionNotice.Success("Connected to $deviceName."),
            )
        }
    }

    private fun markDisconnected(status: String) {
        connectionGeneration++
        updateState {
            it.copy(
                books = emptyList(),
                settings = null,
                availableThemes = emptyList(),
                availableFonts = emptyList(),
                availableLocales = emptyList(),
                focusTimers = NanoFocusTimers(emptyList()),
                rssFeeds = emptyList(),
                firmwareVersion = "",
                otaAsset = "",
                wifiSettings = null,
                wifiSsidDraft = "",
                wifiPasswordDraft = "",
                connectionState = NanoConnectionState.Disconnected,
                discoveredNanos = emptyList(),
                loadingResources = it.loadingResources.intersect(setOf(CompanionResource.Drafts)),
                loadedResources = it.loadedResources.intersect(setOf(CompanionResource.Drafts)),
                isSavingSettings = false,
                bookJob = null,
                catalogInstall = null,
                notice = CompanionNotice.Error(status),
            )
        }
    }

    private fun beginCatalogInstall(asset: CatalogAsset, id: String): Boolean {
        if (current.catalogInstall != null) {
            setNotice(CompanionNotice.Attention("Wait for the current installation to finish."))
            return false
        }
        updateState {
            it.copy(catalogInstall = CatalogInstall(asset, id, CatalogInstallStage.Downloading))
        }
        return true
    }

    private fun updateCatalogInstall(
        asset: CatalogAsset,
        id: String,
        stage: CatalogInstallStage,
        completed: Long = 0,
        total: Long? = null,
    ) {
        val progress = total?.takeIf { it > 0 }?.let { completed.toFloat() / it }
        updateState { state ->
            val install = state.catalogInstall
            if (install?.asset != asset || install.id != id) state
            else state.copy(catalogInstall = install.copy(stage = stage, progress = progress))
        }
    }

    private fun updateCatalogUpload(asset: CatalogAsset, id: String, sent: Long, total: Long) {
        if (sent >= total) {
            updateCatalogInstall(asset, id, CatalogInstallStage.Installing)
        } else {
            updateCatalogInstall(asset, id, CatalogInstallStage.Sending, sent, total)
        }
    }

    private fun finishCatalogInstall(asset: CatalogAsset, id: String) {
        updateState { state ->
            val install = state.catalogInstall
            if (install?.asset == asset && install.id == id) state.copy(catalogInstall = null) else state
        }
    }

    private fun handleDeviceFailure(
        error: Throwable,
        action: String,
        detail: (Throwable) -> String = ::failureDetail,
    ) {
        setNotice(CompanionNotice.Error("$action: ${detail(error)}"))
    }

    private fun <T> refreshResource(
        resource: CompanionResource,
        failureMessage: String,
        request: suspend (String) -> T,
        afterApply: suspend (T) -> Unit = {},
        apply: (CompanionUiState, T) -> CompanionUiState,
    ) {
        val state = current
        if (!state.isConnected || !beginResourceLoad(resource)) return
        val generation = connectionGeneration
        scope.launch {
            try {
                val value = withNanoApi { request(state.baseUrl) }
                var applied = false
                updateState { latest ->
                    applied = false
                    if (connectionGeneration != generation || !latest.isConnected || latest.baseUrl != state.baseUrl) {
                        latest
                    } else {
                        applied = true
                        val updated = apply(latest, value)
                        updated.copy(loadedResources = updated.loadedResources + resource)
                    }
                }
                if (applied) afterApply(value)
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (error: Throwable) {
                if (connectionGeneration == generation) handleDeviceFailure(error, failureMessage)
            } finally {
                if (connectionGeneration == generation) finishResourceLoad(resource)
            }
        }
    }

    private fun beginResourceLoad(resource: CompanionResource): Boolean {
        if (resource in current.loadingResources) return false
        updateState { it.copy(loadingResources = it.loadingResources + resource) }
        return true
    }

    private fun finishResourceLoad(resource: CompanionResource) {
        updateState { it.copy(loadingResources = it.loadingResources - resource) }
    }

    private fun failureDetail(error: Throwable): String =
        error.message?.takeIf(String::isNotBlank) ?: error::class.simpleName ?: "Unknown error"

    private fun updateState(transform: (CompanionUiState) -> CompanionUiState) {
        _uiState.update(transform)
    }

    private suspend fun <T> withNanoApi(block: suspend () -> T): T {
        return nanoApiMutex.withLock { block() }
    }

    private fun librarySummary(books: List<NanoBook>): String {
        val articles = books.count { it.isArticle }
        val regularBooks = books.size - articles
        return "$regularBooks ${if (regularBooks == 1) "book" else "books"} and " +
            "$articles ${if (articles == 1) "article" else "articles"}"
    }

    private fun catalogUrl(path: String): String {
        val settings = current.settings ?: error("Connect to your Nano before loading catalogs.")
        val source = releaseSource(settings.updates.repositoryOwner, settings.updates.releaseTag)
            ?: error("Configure a GitHub release owner on your Nano first.")
        return source.catalogContentUrl(path)
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

    fun close() {
        nanoNetworkController.stop()
        companionController.close()
    }

    private fun hostName(url: String): String {
        return url.substringAfter("://", url).substringBefore("/")
    }

    private fun uploadProgress(sent: Long, total: Long): Float? {
        if (total <= 0L) return null
        return (sent.toFloat() / total.toFloat()).coerceIn(0f, 1f)
    }

    @OptIn(ExperimentalUuidApi::class)
    private fun newId(): String = Uuid.random().toString()

}

private inline fun <T> List<T>.upsert(value: T, id: (T) -> String): List<T> {
    val index = indexOfFirst { id(it) == id(value) }
    if (index < 0) return this + value
    return toMutableList().also { it[index] = value }
}

private fun List<NanoBook>.withBook(book: NanoBook): List<NanoBook> = upsert(book, NanoBook::id)

private data class PendingSettingsSave(
    val settings: NanoSettings,
    val resources: Set<NanoSettingsResource>,
)


