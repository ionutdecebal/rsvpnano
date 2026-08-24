package com.rsvpnano.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import androidx.compose.material.icons.automirrored.outlined.HelpOutline
import androidx.compose.material.icons.automirrored.outlined.LibraryBooks
import androidx.compose.material.icons.outlined.Add
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material.icons.outlined.UploadFile
import androidx.compose.material.icons.outlined.Wifi
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.FabPosition
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationRail
import androidx.compose.material3.NavigationRailItem
import androidx.compose.material3.NavigationRailItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Snackbar
import androidx.compose.material3.SnackbarDuration
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.SnackbarResult
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.navigationevent.NavigationEventInfo
import androidx.navigationevent.compose.NavigationBackHandler
import androidx.navigationevent.compose.rememberNavigationEventState
import com.rsvpnano.app.CompanionNotice
import com.rsvpnano.app.NanoConnectionTransport
import com.rsvpnano.app.NanoEndpoint
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.models.needsArticleFetch
import io.github.vinceglb.filekit.name
import io.github.vinceglb.filekit.readBytes
import io.github.vinceglb.filekit.dialogs.FileKitType
import io.github.vinceglb.filekit.dialogs.compose.rememberFilePickerLauncher
import kotlinx.coroutines.launch

internal enum class CompanionScreen(val label: String, val icon: ImageVector) {
    Library("Library", Icons.AutoMirrored.Outlined.LibraryBooks),
    Settings("Settings", Icons.Outlined.Settings),
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RsvpNanoSharedApp(
    uiState: CompanionUiState,
    presenter: CompanionPresenter,
    hasPermissions: Boolean,
    onConnect: () -> Unit,
    onFirmwareNotificationsChange: (Boolean) -> Unit,
    onGrantPermissions: () -> Unit,
) {
    RsvpNanoTheme {
        val snackbarHostState = remember { SnackbarHostState() }
        val snackbarNotices = remember { mutableStateMapOf<String, CompanionNotice>() }
        val scope = rememberCoroutineScope()
        var selectedScreenName by rememberSaveable { mutableStateOf(CompanionScreen.Library.name) }
        val selectedScreen = CompanionScreen.valueOf(selectedScreenName)
        var selectedBookId by rememberSaveable { mutableStateOf<String?>(null) }
        val selectedBook = selectedBookId?.let { id -> uiState.books.firstOrNull { it.id == id } }
        var settingsDestinationName by rememberSaveable { mutableStateOf<String?>(null) }
        val settingsDestination = settingsDestinationName?.let(SettingsDestination::valueOf)
        var showAddPicker by rememberSaveable { mutableStateOf(false) }
        var showArticleDialog by rememberSaveable { mutableStateOf(false) }
        var showRssDialog by rememberSaveable { mutableStateOf(false) }
        var showConnectionDialog by rememberSaveable { mutableStateOf(false) }
        var showHelpDialog by rememberSaveable { mutableStateOf(false) }
        LaunchedEffect(uiState.isConnected, uiState.canRememberCurrentNano, uiState.nanoSsid) {
            if (uiState.isConnected && uiState.canRememberCurrentNano) {
                val result = snackbarHostState.showSnackbar(
                    message = "Remember ${uiState.nanoSsid ?: "this Nano"} for quicker reconnects?",
                    actionLabel = "Remember",
                    withDismissAction = true,
                    duration = SnackbarDuration.Long,
                )
                if (result == SnackbarResult.ActionPerformed) presenter.rememberCurrentNano()
            }
        }
        val filePicker = rememberFilePickerLauncher(
            type = FileKitType.File(extensions = listOf("epub", "txt", "html", "htm", "rsvp")),
        ) { file ->
            if (file != null) {
                scope.launch {
                    presenter.uploadSelectedFile(file.name, file.readBytes())
                }
            }
        }
        val themePicker = rememberFilePickerLauncher(
            type = FileKitType.File(extensions = listOf("toml")),
        ) { file ->
            if (file != null) {
                scope.launch {
                    presenter.uploadThemeFile(file.name, file.readBytes())
                }
            }
        }
        val fontPicker = rememberFilePickerLauncher(
            type = FileKitType.File(extensions = listOf("rfont4")),
        ) { file ->
            if (file != null) {
                scope.launch {
                    presenter.uploadFontFile(file.name, file.readBytes())
                }
            }
        }
        val localePackPicker = rememberFilePickerLauncher(
            type = FileKitType.File(extensions = listOf("zip")),
        ) { file ->
            if (file != null) {
                scope.launch {
                    presenter.installLocalePackFile(file.name, file.readBytes())
                }
            }
        }

        LaunchedEffect(uiState.notice) {
            if (uiState.notice.showTransient) {
                snackbarNotices[uiState.status] = uiState.notice
                snackbarHostState.showSnackbar(uiState.status)
            }
        }

        BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
            val wide = maxWidth >= 840.dp
            val activeSettingsDestination = if (wide) {
                settingsDestination ?: SettingsDestination.Device
            } else {
                settingsDestination
            }
            LaunchedEffect(uiState.isConnected, selectedScreen, selectedBookId) {
                if (!uiState.isConnected || selectedScreen != CompanionScreen.Library) return@LaunchedEffect
                if (selectedBookId == null) {
                    presenter.refreshLibrary()
                } else {
                    if (uiState.settings == null) presenter.refreshSettings()
                    if (uiState.availableFonts.isEmpty()) presenter.refreshFonts()
                }
            }
            LaunchedEffect(uiState.isConnected, selectedScreen, activeSettingsDestination) {
                if (!uiState.isConnected || selectedScreen != CompanionScreen.Settings) return@LaunchedEffect
                when (activeSettingsDestination) {
                    null -> presenter.refreshSettings()
                    SettingsDestination.Device -> {
                        presenter.refreshSettings()
                        presenter.refreshWifiSettings()
                    }
                    SettingsDestination.Reading,
                    SettingsDestination.Display,
                    -> presenter.refreshSettings()
                    SettingsDestination.Typography -> {
                        presenter.refreshSettings()
                        presenter.refreshFonts()
                    }
                    SettingsDestination.FocusTimers -> presenter.refreshFocusTimers()
                    SettingsDestination.Themes -> {
                        presenter.refreshSettings()
                        presenter.refreshThemes()
                    }
                    SettingsDestination.Locales -> {
                        presenter.refreshSettings()
                        presenter.refreshLocales()
                    }
                    SettingsDestination.Fonts -> {
                        presenter.refreshSettings()
                        presenter.refreshFonts()
                    }
                    SettingsDestination.About -> Unit
                }
            }
            LaunchedEffect(uiState.settings, selectedScreen, activeSettingsDestination) {
                if (uiState.settings == null || selectedScreen != CompanionScreen.Settings) return@LaunchedEffect
                when (activeSettingsDestination) {
                    SettingsDestination.Themes -> if (uiState.themeCatalog.isEmpty()) presenter.refreshThemeCatalog()
                    SettingsDestination.Locales -> if (uiState.localeCatalog.isEmpty()) presenter.refreshLocaleCatalog()
                    SettingsDestination.Fonts -> if (uiState.fontCatalog.isEmpty()) presenter.refreshFontCatalog()
                    else -> Unit
                }
            }
            LaunchedEffect(uiState.isConnected, showRssDialog) {
                if (uiState.isConnected && showRssDialog) presenter.refreshRssFeeds()
            }
            val openBook = selectedBook.takeIf { selectedScreen == CompanionScreen.Library }
            val viewingBook = openBook != null
            val navigateBack = {
                if (viewingBook) {
                    selectedBookId = null
                } else {
                    val (screen, destination) = previousScreen(selectedScreen, settingsDestination, wide)
                    selectedScreenName = screen.name
                    settingsDestinationName = destination?.name
                }
            }
            NavigationBackHandler(
                state = rememberNavigationEventState(NavigationEventInfo.None),
                isBackEnabled = viewingBook || selectedScreen == CompanionScreen.Settings,
                onBackCompleted = navigateBack,
            )
            Row(modifier = Modifier.fillMaxSize()) {
                if (wide) {
                    NavigationRail(containerColor = MaterialTheme.colorScheme.surface) {
                        CompanionScreen.entries.forEach { screen ->
                            NavigationRailItem(
                                selected = selectedScreen == screen,
                                onClick = { selectedScreenName = screen.name },
                                icon = { Icon(imageVector = screen.icon, contentDescription = null) },
                                label = { Text(screen.label) },
                                colors = NavigationRailItemDefaults.colors(
                                    indicatorColor = MaterialTheme.colorScheme.primaryContainer,
                                    selectedIconColor = MaterialTheme.colorScheme.onPrimaryContainer,
                                    selectedTextColor = MaterialTheme.colorScheme.onPrimaryContainer,
                                ),
                            )
                        }
                    }
                }
                Scaffold(
                    modifier = Modifier.weight(1f),
            topBar = {
                TopAppBar(
                    title = {
                        Row(
                            horizontalArrangement = Arrangement.spacedBy(10.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Box(
                                Modifier.width(4.dp).height(28.dp).clip(CircleShape)
                                    .background(MaterialTheme.colorScheme.tertiary),
                            )
                            Text(
                                text = if (!wide && selectedScreen == CompanionScreen.Settings) {
                                    settingsDestination?.label ?: "Settings"
                                } else if (openBook != null) {
                                    openBook.displayTitle
                                } else {
                                    selectedScreen.label
                                },
                                modifier = Modifier.weight(1f),
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis,
                            )
                            ConnectionButton(
                                uiState = uiState,
                                onConnect = onConnect,
                                onOpenControls = { showConnectionDialog = true },
                            )
                        }
                    },
                    navigationIcon = {
                        if (viewingBook || (!wide && selectedScreen == CompanionScreen.Settings)) {
                            IconButton(onClick = navigateBack) {
                                Icon(
                                    Icons.AutoMirrored.Outlined.ArrowBack,
                                    contentDescription = when {
                                        viewingBook -> "Back to library"
                                        settingsDestination != null -> "Back to settings"
                                        else -> "Back to library"
                                    },
                                )
                            }
                        }
                    },
                    actions = {
                        IconButton(onClick = { showHelpDialog = true }) {
                            Icon(Icons.AutoMirrored.Outlined.HelpOutline, contentDescription = "Help")
                        }
                        if (!wide && selectedScreen == CompanionScreen.Library && !viewingBook) {
                            IconButton(onClick = { selectedScreenName = CompanionScreen.Settings.name }) {
                                Icon(Icons.Outlined.Settings, contentDescription = "Settings")
                            }
                        }
                    },
                    colors = TopAppBarDefaults.topAppBarColors(containerColor = MaterialTheme.colorScheme.surface),
                )
            },
            snackbarHost = {
                val bookJob = uiState.bookJob
                if (bookJob != null) {
                    BookJobSnackbar(bookJob)
                } else {
                    SnackbarHost(hostState = snackbarHostState) { data ->
                        val notice = snackbarNotices[data.visuals.message]
                            ?: CompanionNotice.Neutral(data.visuals.message)
                        Snackbar(
                            snackbarData = data,
                            containerColor = snackbarColor(notice),
                            contentColor = snackbarContentColor(notice),
                            actionColor = snackbarActionColor(notice),
                        )
                    }
                }
            },
            floatingActionButton = {
                if (selectedScreen == CompanionScreen.Library && !viewingBook) {
                    ExtendedFloatingActionButton(
                        onClick = { showAddPicker = true },
                        icon = { Icon(Icons.Outlined.Add, contentDescription = null) },
                        text = { Text("Add content") },
                    )
                }
            },
            floatingActionButtonPosition = FabPosition.End,
        ) { contentPadding ->
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(
                        Brush.linearGradient(
                            listOf(
                                MaterialTheme.colorScheme.background,
                                MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.22f),
                                MaterialTheme.colorScheme.background,
                            ),
                        ),
                    )
                    .padding(contentPadding)
                    .padding(horizontal = 16.dp, vertical = 8.dp),
                contentAlignment = Alignment.TopCenter,
            ) {
                Box(modifier = Modifier.fillMaxSize().widthIn(max = 840.dp)) {
                    if (openBook != null) {
                        BookDetailScreen(
                            book = openBook,
                            availableFonts = uiState.availableFonts,
                            globalFontId = uiState.settings?.reading?.typography?.fontId.orEmpty(),
                            wpm = uiState.settings?.reading?.wpm ?: 300,
                            onSetPosition = { presenter.setBookPosition(openBook, it) },
                            onSetLanguageFonts = { presenter.setBookLanguageFonts(openBook, it) },
                        )
                    } else {
                        when (selectedScreen) {
                            CompanionScreen.Library -> LibraryScreen(
                                uiState = uiState,
                                onRefresh = presenter::refreshLibrary,
                                needsArticleFetch = PendingUpload::needsArticleFetch,
                                onEditDraft = {
                                    presenter.editDraft(it)
                                    showArticleDialog = true
                                },
                                onDeleteDraft = presenter::deleteDraft,
                                onSyncArticles = presenter::syncSavedArticles,
                                onOpenBook = { selectedBookId = it.id },
                                onDeleteBook = presenter::deleteDeviceBook,
                                onAddContent = { showAddPicker = true },
                            )

                            CompanionScreen.Settings -> SettingsScreen(
                                uiState = uiState,
                                presenter = presenter,
                                onFirmwareNotificationsChange = onFirmwareNotificationsChange,
                                hasPermissions = hasPermissions,
                                onGrantPermissions = onGrantPermissions,
                                onUploadTheme = { themePicker.launch() },
                                onUploadFont = { fontPicker.launch() },
                                onUploadLocalePack = { localePackPicker.launch() },
                                destination = activeSettingsDestination,
                                onDestinationSelected = { settingsDestinationName = it.name },
                            )
                        }
                    }
                }
            }

            if (showAddPicker) {
                AddContentDialog(
                    onDismiss = { showAddPicker = false },
                    onUploadBook = {
                        showAddPicker = false
                        filePicker.launch()
                    },
                    onAddArticle = {
                        showAddPicker = false
                        showArticleDialog = true
                    },
                    onAddRssFeed = {
                        showAddPicker = false
                        showRssDialog = true
                    },
                )
            }

            if (uiState.discoveredNanos.isNotEmpty()) {
                NanoPickerDialog(
                    nanos = uiState.discoveredNanos,
                    onSelect = presenter::selectDiscoveredNano,
                    onDismiss = presenter::cancelNanoSelection,
                )
            }

            if (showArticleDialog) {
                AddArticleDialog(
                    uiState = uiState,
                    onDismiss = {
                        showArticleDialog = false
                        presenter.cancelDraftEdit()
                    },
                    onTitleChange = presenter::setDraftTitle,
                    onSourceChange = presenter::setDraftSourceUrl,
                    onBodyChange = presenter::setDraftBody,
                    onSaveText = {
                        showArticleDialog = false
                        presenter.saveTextDraft()
                    },
                    onSaveLink = {
                        showArticleDialog = false
                        presenter.saveLinkDraft()
                    },
                )
            }

            if (showRssDialog) {
                RssFeedsDialog(
                    uiState = uiState,
                    onDismiss = { showRssDialog = false },
                    onFeedChange = presenter::setRssFeedDraft,
                    onAddFeed = presenter::addRssFeed,
                    onRefreshFeeds = presenter::refreshRssFeeds,
                    onDeleteFeed = presenter::deleteRssFeed,
                )
            }

            if (showConnectionDialog) {
                ConnectionDialog(
                    uiState = uiState,
                    onDismiss = { showConnectionDialog = false },
                    onReconnect = {
                        showConnectionDialog = false
                        onConnect()
                    },
                    onRememberCurrentNano = presenter::rememberCurrentNano,
                )
            }

            if (showHelpDialog) {
                val help = when (selectedScreen) {
                    CompanionScreen.Library -> if (viewingBook) {
                        "Book details" to "Review metadata, choose language fonts, or set a new reading position by chapter or percentage."
                    } else {
                        "Library" to "Add books, saved articles, or RSS feeds here. Connect to sync them with your reader."
                    }
                    CompanionScreen.Settings -> activeSettingsDestination
                        ?.let { it.label to it.help }
                        ?: ("Settings" to SETTINGS_INDEX_HELP)
                }
                HelpDialog(
                    title = help.first,
                    body = help.second,
                    onDismiss = { showHelpDialog = false },
                )
            }
        }
            }
        }
    }
}

internal fun previousScreen(
    screen: CompanionScreen,
    settingsDestination: SettingsDestination?,
    wide: Boolean,
): Pair<CompanionScreen, SettingsDestination?> =
    if (!wide && screen == CompanionScreen.Settings && settingsDestination != null) {
        CompanionScreen.Settings to null
    } else {
        CompanionScreen.Library to settingsDestination
    }

@Composable
private fun ConnectionButton(
    uiState: CompanionUiState,
    onConnect: () -> Unit,
    onOpenControls: () -> Unit,
) {
    val busy = uiState.isCheckingReader || uiState.isRequestingNanoNetwork
    TextButton(
        onClick = if (uiState.isConnected) onOpenControls else onConnect,
        enabled = !busy,
    ) {
        if (busy) {
            CircularProgressIndicator(modifier = Modifier.size(16.dp), strokeWidth = 2.dp)
        } else if (uiState.isConnected) {
            ConnectionDot()
        } else {
            Icon(Icons.Outlined.Wifi, contentDescription = null)
        }
        Spacer(Modifier.width(8.dp))
        Text(
            text = when {
                busy -> "Connecting"
                uiState.isConnected -> uiState.currentNano?.ssid ?: "Nano"
                else -> "Connect"
            },
            maxLines = 1,
        )
    }
}

@Composable
private fun ConnectionDot() {
    Box(
        Modifier
            .size(8.dp)
            .clip(CircleShape)
            .background(Color(0xFF3C8C69)),
    )
}

@Composable
private fun ConnectionDialog(
    uiState: CompanionUiState,
    onDismiss: () -> Unit,
    onReconnect: () -> Unit,
    onRememberCurrentNano: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        icon = { Icon(Icons.Outlined.CheckCircle, contentDescription = null) },
        title = { Text(uiState.currentNano?.ssid ?: "RSVP Nano") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Connected", style = MaterialTheme.typography.titleSmall)
                Text(
                    when (uiState.connectionState.transport) {
                        NanoConnectionTransport.LocalNetwork -> "Using the local network"
                        NanoConnectionTransport.AccessPoint -> "Using the Nano's direct Wi-Fi"
                        NanoConnectionTransport.Usb -> "Using USB"
                        null -> uiState.baseUrl
                    },
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                if (uiState.canRememberCurrentNano) {
                    TextButton(onClick = onRememberCurrentNano) {
                        Text("Remember this Nano")
                    }
                }
            }
        },
        confirmButton = { TextButton(onClick = onReconnect) { Text("Reconnect") } },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Close") } },
    )
}

@Composable
private fun HelpDialog(title: String, body: String, onDismiss: () -> Unit) {
    AlertDialog(
        onDismissRequest = onDismiss,
        icon = { Icon(Icons.AutoMirrored.Outlined.HelpOutline, contentDescription = null) },
        title = { Text("$title help") },
        text = { Text(body) },
        confirmButton = { TextButton(onClick = onDismiss) { Text("Got it") } },
    )
}

@Composable
private fun NanoPickerDialog(
    nanos: List<NanoEndpoint>,
    onSelect: (NanoEndpoint) -> Unit,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Choose a Nano") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                nanos.forEach { endpoint ->
                    TextButton(
                        onClick = { onSelect(endpoint) },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text(endpoint.nano.ssid, modifier = Modifier.fillMaxWidth())
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel")
            }
        },
    )
}

@Composable
private fun BookJobSnackbar(job: BookJob) {
    val progress = job.progress
    val percent = job.percent
    Snackbar(
        modifier = Modifier.padding(12.dp),
        containerColor = MaterialTheme.colorScheme.inverseSurface,
        contentColor = MaterialTheme.colorScheme.inverseOnSurface,
    ) {
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            job.done.forEach { step ->
                Row(
                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(imageVector = Icons.Outlined.CheckCircle, contentDescription = null, modifier = Modifier.size(18.dp))
                    Text(
                        text = "${step.doneLabel} \"${job.name}\"",
                        modifier = Modifier.weight(1f),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
            }
            Row(
                horizontalArrangement = Arrangement.spacedBy(10.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                if (progress == null) {
                    CircularProgressIndicator(
                        modifier = Modifier.size(18.dp),
                        strokeWidth = 2.dp,
                        color = MaterialTheme.colorScheme.inverseOnSurface,
                        trackColor = MaterialTheme.colorScheme.inverseSurface,
                    )
                } else {
                    Icon(imageVector = Icons.Outlined.UploadFile, contentDescription = null, modifier = Modifier.size(20.dp))
                }
                Text(
                    text = buildString {
                        append(job.active.activeLabel)
                        append(" \"")
                        append(job.name)
                        append("\"")
                        if (percent != null) {
                            append(" ")
                            append(percent)
                            append("%")
                        }
                    },
                    modifier = Modifier.weight(1f),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    style = MaterialTheme.typography.bodyMedium,
                )
            }
            if (progress != null) {
                LinearProgressIndicator(
                    progress = { progress.coerceIn(0f, 1f) },
                    modifier = Modifier.fillMaxWidth(),
                    color = MaterialTheme.colorScheme.inverseOnSurface,
                    trackColor = MaterialTheme.colorScheme.inverseSurface.copy(alpha = 0.32f),
                )
            }
        }
    }
}

private fun snackbarColor(notice: CompanionNotice): Color =
    when (notice) {
        is CompanionNotice.Success -> Color(0xFF0F5F3D)
        is CompanionNotice.Attention -> Color(0xFF705100)
        is CompanionNotice.Error -> Color(0xFF8C1D18)
        is CompanionNotice.Neutral -> Color(0xFF1F2933)
    }

private fun snackbarContentColor(notice: CompanionNotice): Color =
    when (notice) {
        is CompanionNotice.Attention -> Color(0xFFFFF4CC)
        else -> Color.White
    }

private fun snackbarActionColor(notice: CompanionNotice): Color =
    when (notice) {
        is CompanionNotice.Attention -> Color(0xFFFFD766)
        else -> Color(0xFFB8E6FF)
    }
