package com.rsvpnano.android.ui

import android.content.Context
import android.content.Intent
import android.os.Build
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.Uri
import android.provider.OpenableColumns
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.clickable
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.pullrefresh.PullRefreshIndicator
import androidx.compose.material.pullrefresh.pullRefresh
import androidx.compose.material.pullrefresh.rememberPullRefreshState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.HelpOutline
import androidx.compose.material.icons.automirrored.outlined.LibraryBooks
import androidx.compose.material.icons.automirrored.outlined.MenuBook
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material.icons.outlined.CloudUpload
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material.icons.outlined.Edit
import androidx.compose.material.icons.outlined.Newspaper
import androidx.compose.material.icons.outlined.Refresh
import androidx.compose.material.icons.outlined.RssFeed
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material.icons.outlined.Sync
import androidx.compose.material.icons.outlined.UploadFile
import androidx.compose.material.icons.outlined.WarningAmber
import androidx.compose.material.icons.outlined.Wifi
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.AssistChip
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.SnackbarResult
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.viewmodel.compose.viewModel
import com.rsvpnano.android.net.AndroidNanoNetworkController
import com.rsvpnano.app.RsvpSharedApp
import com.rsvpnano.converters.RsvpSupportedFileTypes
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.PendingUpload
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

private enum class CompanionTab(val label: String, val icon: ImageVector) {
    Library("Library", Icons.AutoMirrored.Outlined.LibraryBooks),
    Settings("Settings", Icons.Outlined.Settings),
}

private enum class LibraryFilter(val label: String) {
    All("All"),
    Books("Books"),
    Articles("Articles"),
}

private val CompanionLightColors = lightColorScheme(
    primary = Color(0xFF2D5B45),
    onPrimary = Color.White,
    primaryContainer = Color(0xFFC8EAD6),
    onPrimaryContainer = Color(0xFF092016),
    secondary = Color(0xFF566159),
    background = Color(0xFFF6F7F1),
    surface = Color(0xFFFDFCF6),
    surfaceVariant = Color(0xFFE2E7DD),
)

private val CompanionDarkColors = darkColorScheme(
    primary = Color(0xFF9CD8B6),
    onPrimary = Color(0xFF06351F),
    primaryContainer = Color(0xFF174C31),
    onPrimaryContainer = Color(0xFFC8EAD6),
    secondary = Color(0xFFBAC8BC),
    background = Color(0xFF101511),
    surface = Color(0xFF171D18),
    surfaceVariant = Color(0xFF3F4941),
)

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterialApi::class)
@Composable
fun CompanionApp(
    sharedApp: RsvpSharedApp,
    shareIntent: Intent? = null,
    onShareIntentHandled: () -> Unit = {},
) {
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current
    val nanoNetworkController = remember(context) { AndroidNanoNetworkController(context.applicationContext) }
    val viewModel: CompanionViewModel = viewModel(
        factory = CompanionViewModel.Factory(
            sharedApp = sharedApp,
            nanoNetworkController = nanoNetworkController,
        )
    )
    val uiState by viewModel.uiState.collectAsState()
    val filePicker = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument(),
    ) { uri ->
        if (uri != null) {
            context.readSelectedFile(uri)?.let { file ->
                viewModel.uploadSelectedFile(displayName = file.displayName, data = file.data)
            }
        }
    }
    val scope = rememberCoroutineScope()
    val snackbarHostState = remember { SnackbarHostState() }
    val nanoWifiPermissionLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.RequestMultiplePermissions(),
    ) { permissions ->
        val granted = permissions.values.all { it } || nanoNetworkController.hasRequiredPermissions()
        if (granted) {
            viewModel.connectNanoScan()
        } else {
            viewModel.scanPermissionDenied()
        }
    }
    fun connectNanoFromApp() {
        if (nanoNetworkController.hasRequiredPermissions()) {
            viewModel.connectNanoScan()
        } else {
            nanoWifiPermissionLauncher.launch(nanoWifiPermissions())
        }
    }
    DisposableEffect(lifecycleOwner, viewModel) {
        val observer = LifecycleEventObserver { _, event ->
            if (event == Lifecycle.Event.ON_RESUME) {
                viewModel.recheckConnectionAfterResume()
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
    }
    DisposableEffect(context, viewModel) {
        val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager
        if (connectivityManager == null) {
            onDispose { }
        } else {
            val callback = object : ConnectivityManager.NetworkCallback() {
                override fun onAvailable(network: Network) {
                    viewModel.fetchPendingArticlesWhenOnline()
                }

                override fun onLost(network: Network) {
                }

                override fun onCapabilitiesChanged(network: Network, networkCapabilities: NetworkCapabilities) {
                    if (networkCapabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)) {
                        viewModel.fetchPendingArticlesWhenOnline()
                    }
                }
            }
            connectivityManager.registerDefaultNetworkCallback(callback)
            onDispose { connectivityManager.unregisterNetworkCallback(callback) }
        }
    }
    LaunchedEffect(shareIntent) {
        val intent = shareIntent ?: return@LaunchedEffect
        viewModel.releaseNanoForInternetWork()
        delay(800)
        val imports = withContext(Dispatchers.IO) { context.sharedImportsFrom(intent) }
        if (imports.isNotEmpty() || intent.isAndroidShareIntent()) {
            viewModel.saveSharedImports(imports)
        }
        onShareIntentHandled()
    }

    val colorScheme = if (isSystemInDarkTheme()) CompanionDarkColors else CompanionLightColors
    MaterialTheme(colorScheme = colorScheme) {
        Surface(
            modifier = Modifier.fillMaxSize(),
            color = MaterialTheme.colorScheme.background,
        ) {
            var selectedTab by remember { mutableStateOf(CompanionTab.Library) }
            var showAddPicker by remember { mutableStateOf(false) }
            var showArticleDialog by remember { mutableStateOf(false) }
            var showRssDialog by remember { mutableStateOf(false) }
            LaunchedEffect(uiState.status) {
                if (
                    uiState.status.startsWith("Uploaded") ||
                    uiState.status.startsWith("Synced") ||
                    uiState.status.startsWith("RSS") ||
                    uiState.status.startsWith("Reader settings saved") ||
                    uiState.status.startsWith("Wi-Fi settings") ||
                    uiState.status.startsWith("Shared") ||
                    uiState.status.startsWith("Saved") ||
                    uiState.status.startsWith("Reader disconnected") ||
                    uiState.status.startsWith("Could not find") ||
                    uiState.status.startsWith("Searching for RSVP Nano") ||
                    uiState.status.startsWith("Starting RSVP Nano") ||
                    uiState.status.startsWith("Waiting for Android") ||
                    uiState.status.startsWith("Could not connect to RSVP Nano Wi-Fi") ||
                    uiState.status.startsWith("Scan permission denied") ||
                    uiState.status.startsWith("Scan needs") ||
                    uiState.status.startsWith("Android did not find") ||
                    uiState.status.startsWith("Android rejected")
                ) {
                    snackbarHostState.showSnackbar(uiState.status)
                }
            }
            LaunchedEffect(uiState.canRememberCurrentNano, uiState.nanoSsid) {
                if (uiState.canRememberCurrentNano && !uiState.nanoSsid.isNullOrBlank()) {
                    val result = snackbarHostState.showSnackbar(
                        message = "Connected to ${uiState.nanoSsid}.",
                        actionLabel = "Remember",
                    )
                    if (result == SnackbarResult.ActionPerformed) {
                        viewModel.rememberCurrentNano()
                    }
                }
            }
            Scaffold(
                topBar = {
                    Column {
                        TopAppBar(
                            title = { Text(text = "RSVP Nano") },
                            actions = {
                                IconButton(
                                    onClick = {
                                        scope.launch {
                                            snackbarHostState.showSnackbar("Open Companion sync on the reader, join RSVP-Nano Wi-Fi, then tap the refresh icon.")
                                        }
                                    },
                                ) {
                                    Icon(imageVector = Icons.AutoMirrored.Outlined.HelpOutline, contentDescription = "Help")
                                }
                            },
                            colors = TopAppBarDefaults.topAppBarColors(
                                containerColor = MaterialTheme.colorScheme.background,
                            ),
                        )
                        ConnectionBar(
                            uiState = uiState,
                            onOpenWifiSettings = context::openWifiSettings,
                            onConnect = { connectNanoFromApp() },
                            onShowAddressEntry = viewModel::showAddressEntry,
                            onAddressChange = viewModel::setAddress,
                            onConnectCustom = viewModel::connect,
                            hasPermissions = nanoNetworkController.hasRequiredPermissions(),
                        )
                    }
                },
                snackbarHost = { SnackbarHost(hostState = snackbarHostState) },
                bottomBar = {
                    NavigationBar {
                        CompanionTab.entries.forEach { tab ->
                            NavigationBarItem(
                                selected = selectedTab == tab,
                                onClick = { selectedTab = tab },
                                icon = { Icon(imageVector = tab.icon, contentDescription = null) },
                                label = { Text(text = tab.label) },
                            )
                        }
                    }
                },
            ) { contentPadding ->
                Column(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(contentPadding)
                        .padding(horizontal = 20.dp, vertical = 12.dp),
                ) {
                    when (selectedTab) {
                        CompanionTab.Library -> LibraryTab(
                            uiState = uiState,
                            onRefresh = viewModel::refresh,
                            needsArticleFetch = viewModel::needsArticleFetch,
                            onEditDraft = {
                                viewModel.editDraft(it)
                                showArticleDialog = true
                            },
                            onDeleteDraft = viewModel::deleteDraft,
                            onSyncArticles = viewModel::syncSavedArticles,
                            onDeleteBook = viewModel::deleteDeviceBook,
                            onShowUpload = { showAddPicker = true },
                        )
                        CompanionTab.Settings -> SettingsTab(
                            uiState = uiState,
                            onRefresh = viewModel::refresh,
                            onUpdateSettings = viewModel::updateSettings,
                            onAddressChange = viewModel::setAddress,
                            onConnectDefault = viewModel::connectDefault,
                            onWifiSsidChange = viewModel::setWifiSsidDraft,
                            onWifiPasswordChange = viewModel::setWifiPasswordDraft,
                            onSaveWifi = viewModel::saveWifiSettings,
                            onClearWifi = viewModel::clearWifiSettings,
                            onForgetRememberedNano = viewModel::forgetRememberedNano,
                            hasPermissions = nanoNetworkController.hasRequiredPermissions(),
                            onGrantPermissions = { connectNanoFromApp() },
                        )
                    }
                }
            }

            if (showAddPicker) {
                AddContentDialog(
                    onDismiss = { showAddPicker = false },
                    onUploadBook = {
                        showAddPicker = false
                        filePicker.launch(
                            arrayOf(
                                "application/epub+zip",
                                "text/*",
                                "text/html",
                                "application/octet-stream",
                            ),
                        )
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

            if (showArticleDialog) {
                AddArticleDialog(
                    uiState = uiState,
                    onDismiss = {
                        showArticleDialog = false
                        viewModel.cancelDraftEdit()
                    },
                    onTitleChange = viewModel::setDraftTitle,
                    onSourceChange = viewModel::setDraftSourceUrl,
                    onBodyChange = viewModel::setDraftBody,
                    onSaveText = {
                        showArticleDialog = false
                        viewModel.saveTextDraft()
                    },
                    onSaveLink = {
                        showArticleDialog = false
                        viewModel.saveLinkDraft()
                    },
                )
            }

            if (showRssDialog) {
                RssFeedsDialog(
                    uiState = uiState,
                    onDismiss = { showRssDialog = false },
                    onFeedChange = viewModel::setRssFeedDraft,
                    onAddFeed = viewModel::addRssFeed,
                    onSyncFeeds = viewModel::syncRssFeeds,
                    onDeleteFeed = viewModel::deleteRssFeed,
                )
            }
        }
    }
}

@Composable
private fun AddContentDialog(
    onDismiss: () -> Unit,
    onUploadBook: () -> Unit,
    onAddArticle: () -> Unit,
    onAddRssFeed: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        icon = {
            Icon(imageVector = Icons.Outlined.UploadFile, contentDescription = null)
        },
        title = { Text("Upload") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                UploadActionRow(
                    icon = Icons.Outlined.UploadFile,
                    label = "Upload book",
                    onClick = onUploadBook,
                )
                UploadActionRow(
                    icon = Icons.Outlined.Newspaper,
                    label = "Add article",
                    onClick = onAddArticle,
                )
                UploadActionRow(
                    icon = Icons.Outlined.RssFeed,
                    label = "Add RSS feed",
                    onClick = onAddRssFeed,
                )
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel")
            }
        },
    )
}

@Composable
private fun UploadActionRow(
    icon: ImageVector,
    label: String,
    onClick: () -> Unit,
) {
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick),
        color = MaterialTheme.colorScheme.surfaceContainerLow,
        shape = MaterialTheme.shapes.small,
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 14.dp, vertical = 11.dp),
            horizontalArrangement = Arrangement.spacedBy(10.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                imageVector = icon,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Text(
                text = label,
                style = MaterialTheme.typography.bodyLarge,
                color = MaterialTheme.colorScheme.onSurface,
            )
        }
    }
}

@Composable
private fun AddArticleDialog(
    uiState: CompanionUiState,
    onDismiss: () -> Unit,
    onTitleChange: (String) -> Unit,
    onSourceChange: (String) -> Unit,
    onBodyChange: (String) -> Unit,
    onSaveText: () -> Unit,
    onSaveLink: () -> Unit,
) {
    var showBody by remember(uiState.editingDraftId) { mutableStateOf(uiState.draftBody.isNotBlank()) }
    val hasUrl = uiState.draftSourceUrl.trim().isNotEmpty()
    val canSaveLink = uiState.draftSourceUrl.trim().let { it.startsWith("http://") || it.startsWith("https://") }
    val canSaveText = uiState.draftTitle.trim().isNotEmpty() && uiState.draftBody.trim().isNotEmpty()
    AlertDialog(
        onDismissRequest = onDismiss,
        icon = {
            Icon(imageVector = Icons.Outlined.Newspaper, contentDescription = null)
        },
        title = { Text(if (uiState.editingDraftId == null) "Add article" else "Edit article") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                OutlinedTextField(
                    value = uiState.draftSourceUrl,
                    onValueChange = onSourceChange,
                    label = { Text("Article URL") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = uiState.draftTitle,
                    onValueChange = onTitleChange,
                    label = { Text("Title") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                TextButton(onClick = { showBody = !showBody }) {
                    Icon(imageVector = Icons.Outlined.Edit, contentDescription = null)
                    Text(if (showBody) "Hide body" else "Edit body")
                }
                if (showBody) {
                    OutlinedTextField(
                        value = uiState.draftBody,
                        onValueChange = onBodyChange,
                        label = { Text("Article body") },
                        minLines = 5,
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
            }
        },
        confirmButton = {
            Button(
                onClick = {
                    if (showBody && uiState.draftBody.trim().isNotEmpty()) {
                        onSaveText()
                    } else {
                        onSaveLink()
                    }
                },
                enabled = if (showBody && uiState.draftBody.trim().isNotEmpty()) canSaveText else hasUrl && canSaveLink,
            ) {
                Text("Save")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel")
            }
        },
    )
}

@Composable
private fun RssFeedsDialog(
    uiState: CompanionUiState,
    onDismiss: () -> Unit,
    onFeedChange: (String) -> Unit,
    onAddFeed: () -> Unit,
    onSyncFeeds: () -> Unit,
    onDeleteFeed: (String) -> Unit,
) {
    var feedToDelete by remember { mutableStateOf<String?>(null) }
    AlertDialog(
        onDismissRequest = onDismiss,
        icon = {
            Icon(imageVector = Icons.Outlined.RssFeed, contentDescription = null)
        },
        title = { Text("RSS feeds") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                OutlinedTextField(
                    value = uiState.rssFeedDraft,
                    onValueChange = onFeedChange,
                    label = { Text("Feed URL") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = onAddFeed) {
                        Icon(imageVector = Icons.Outlined.RssFeed, contentDescription = null)
                        Text("Add")
                    }
                    TextButton(onClick = onSyncFeeds, enabled = uiState.isConnected) {
                        Icon(imageVector = Icons.Outlined.Sync, contentDescription = null)
                        Text("Sync")
                    }
                }
                if (uiState.rssFeeds.isEmpty()) {
                    Text("No RSS feeds saved.", style = MaterialTheme.typography.bodyMedium)
                } else {
                    uiState.rssFeeds.forEach { feed ->
                        Surface(
                            modifier = Modifier.fillMaxWidth(),
                            color = MaterialTheme.colorScheme.surfaceContainerLow,
                            shape = MaterialTheme.shapes.small,
                        ) {
                            Row(
                                modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp),
                                horizontalArrangement = Arrangement.spacedBy(8.dp),
                                verticalAlignment = Alignment.CenterVertically,
                            ) {
                                Icon(imageVector = Icons.Outlined.RssFeed, contentDescription = null)
                                Text(
                                    text = feed,
                                    modifier = Modifier.weight(1f),
                                    style = MaterialTheme.typography.bodySmall,
                                )
                                DestructiveIconButton(
                                    contentDescription = "Delete feed",
                                    onClick = { feedToDelete = feed },
                                )
                            }
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text("Done")
            }
        },
    )

    feedToDelete?.let { feed ->
        AlertDialog(
            onDismissRequest = { feedToDelete = null },
            title = { Text("Delete RSS feed?") },
            text = { Text(feed) },
            confirmButton = {
                FilledTonalButton(
                    onClick = {
                        feedToDelete = null
                        onDeleteFeed(feed)
                    },
                    colors = ButtonDefaults.filledTonalButtonColors(
                        containerColor = MaterialTheme.colorScheme.errorContainer,
                        contentColor = MaterialTheme.colorScheme.onErrorContainer,
                    ),
                ) {
                    Text("Delete")
                }
            },
            dismissButton = {
                TextButton(onClick = { feedToDelete = null }) {
                    Text("Cancel")
                }
            },
        )
    }
}

@Composable
private fun ConnectionBar(
    uiState: CompanionUiState,
    onOpenWifiSettings: () -> Unit,
    onConnect: () -> Unit,
    onShowAddressEntry: () -> Unit,
    onAddressChange: (String) -> Unit,
    onConnectCustom: () -> Unit,
    hasPermissions: Boolean,
) {
    val containerColor by animateColorAsState(
        targetValue = if (uiState.isConnected) {
            MaterialTheme.colorScheme.primaryContainer
        } else {
            MaterialTheme.colorScheme.surfaceContainerHigh
        },
        animationSpec = tween(durationMillis = 320),
        label = "ConnectionBarColor",
    )
    Surface(color = containerColor) {
        if (uiState.isConnected) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 4.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Icon(
                    imageVector = Icons.Outlined.CheckCircle,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.primary,
                )
                Text(
                    text = "Connected",
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.onPrimaryContainer,
                    maxLines = 1,
                )
            }
        } else {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 10.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                DisconnectedConnectionBarContent(
                    uiState = uiState,
                    onOpenWifiSettings = onOpenWifiSettings,
                    onConnect = onConnect,
                    onShowAddressEntry = onShowAddressEntry,
                    onAddressChange = onAddressChange,
                    onConnectCustom = onConnectCustom,
                    hasPermissions = hasPermissions,
                )
            }
        }
    }
}

@Composable
private fun DisconnectedConnectionBarContent(
    uiState: CompanionUiState,
    onOpenWifiSettings: () -> Unit,
    onConnect: () -> Unit,
    onShowAddressEntry: () -> Unit,
    onAddressChange: (String) -> Unit,
    onConnectCustom: () -> Unit,
    hasPermissions: Boolean,
) {
    val statusLabel = when {
        uiState.isRequestingNanoNetwork -> "Connecting"
        uiState.isCheckingReader -> "Checking reader"
        else -> "Disconnected"
    }
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(
            imageVector = Icons.Outlined.WarningAmber,
            contentDescription = null,
            tint = MaterialTheme.colorScheme.tertiary,
        )
        Text(
            text = statusLabel,
            modifier = Modifier.weight(1f),
            style = MaterialTheme.typography.titleSmall,
            maxLines = 1,
            color = MaterialTheme.colorScheme.onSurface,
        )
        if (hasPermissions) {
            Button(onClick = onConnect, enabled = !uiState.isRequestingNanoNetwork && !uiState.isCheckingReader) {
                Icon(imageVector = Icons.Outlined.Wifi, contentDescription = null)
                Text(text = "Connect")
            }
        } else {
            Button(onClick = onOpenWifiSettings) {
                Icon(imageVector = Icons.Outlined.Wifi, contentDescription = null)
                Text(text = "Wi-Fi")
            }
        }
    }

    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        TextButton(onClick = onShowAddressEntry) {
            Text(text = if (uiState.showAddressEntry) "Hide manual connection" else "Connect manually")
        }
    }
    if (uiState.showAddressEntry) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            OutlinedTextField(
                value = uiState.address,
                onValueChange = onAddressChange,
                label = { Text("Reader address") },
                singleLine = true,
                modifier = Modifier.weight(1f),
            )
            Button(onClick = onConnectCustom) {
                Text(text = "Connect")
            }
        }
    }
}

@OptIn(ExperimentalMaterialApi::class)
@Composable
private fun LibraryTab(
    uiState: CompanionUiState,
    onRefresh: () -> Unit,
    needsArticleFetch: (PendingUpload) -> Boolean,
    onEditDraft: (PendingUpload) -> Unit,
    onDeleteDraft: (PendingUpload) -> Unit,
    onSyncArticles: () -> Unit,
    onDeleteBook: (NanoBook) -> Unit,
    onShowUpload: () -> Unit,
) {
    var searchQuery by remember { mutableStateOf("") }
    var filter by remember { mutableStateOf(LibraryFilter.All) }
    var selectedBook by remember { mutableStateOf<NanoBook?>(null) }
    var bookToDelete by remember { mutableStateOf<NanoBook?>(null) }
    var draftToDelete by remember { mutableStateOf<PendingUpload?>(null) }
    val visibleDrafts = uiState.drafts.filter { draft ->
        val query = searchQuery.trim()
        filter != LibraryFilter.Books &&
            (
                query.isEmpty() ||
                    draft.title.contains(query, ignoreCase = true) ||
                    draft.sourceUrl.orEmpty().contains(query, ignoreCase = true)
                )
    }
    val visibleBooks = uiState.books.filter { book ->
        val isArticle = book.isArticle
        val matchesFilter = when (filter) {
            LibraryFilter.All -> true
            LibraryFilter.Books -> !isArticle
            LibraryFilter.Articles -> isArticle
        }
        val query = searchQuery.trim()
        val matchesQuery = query.isEmpty() ||
            book.displayTitle.contains(query, ignoreCase = true) ||
            book.author.orEmpty().contains(query, ignoreCase = true) ||
            book.id.contains(query, ignoreCase = true)
        matchesFilter && matchesQuery
    }
    PullRefreshBox(
        isRefreshing = uiState.isRefreshing,
        onRefresh = onRefresh,
    ) {
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            contentPadding = PaddingValues(bottom = 18.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            item {
                Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    if (uiState.isConnected) {
                        UploadLibraryRow(onClick = onShowUpload)
                    }
                    OutlinedTextField(
                        value = searchQuery,
                        onValueChange = { searchQuery = it },
                        label = { Text("Search library") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        LibraryFilter.entries.forEach { option ->
                            FilterChip(
                                selected = filter == option,
                                onClick = { filter = option },
                                label = { Text(option.label) },
                            )
                        }
                    }
                    HorizontalDivider()
                }
            }

            if (visibleDrafts.isNotEmpty()) {
                item {
                    Text(
                        text = "Pending articles",
                        style = MaterialTheme.typography.titleSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                items(visibleDrafts, key = { draft -> draft.id }) { draft ->
                    PendingArticleRow(
                        draft = draft,
                        needsFetch = needsArticleFetch(draft),
                        onEdit = { onEditDraft(draft) },
                        onDelete = { draftToDelete = draft },
                    )
                }
                item {
                    Button(
                        onClick = onSyncArticles,
                        enabled = uiState.isConnected && uiState.drafts.any { !needsArticleFetch(it) },
                    ) {
                        Icon(imageVector = Icons.Outlined.Sync, contentDescription = null)
                        Text("Sync ready articles")
                    }
                }
            }

            if (visibleBooks.isEmpty()) {
                item {
                    EmptyCard(
                        text = when {
                            !uiState.isConnected -> "Reader library unavailable while disconnected."
                            visibleDrafts.isNotEmpty() -> "No matching reader items."
                            else -> "No library items on the reader."
                        },
                    )
                }
            } else {
                item {
                    Text(
                        text = "Library",
                        style = MaterialTheme.typography.titleSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                items(visibleBooks, key = { book -> book.id }) { book ->
                    LibraryBookRow(
                        book = book,
                        onOpenBook = { selectedBook = book },
                        onDeleteBook = { bookToDelete = book },
                    )
                }
            }
        }
    }

    selectedBook?.let { book ->
        LibraryBookDialog(
            book = book,
            onDismiss = { selectedBook = null },
        )
    }

    bookToDelete?.let { book ->
        AlertDialog(
            onDismissRequest = { bookToDelete = null },
            icon = {
                Icon(imageVector = Icons.Outlined.Delete, contentDescription = null)
            },
            title = { Text("Delete from reader?") },
            text = { Text(book.displayTitle) },
            confirmButton = {
                FilledTonalButton(
                    onClick = {
                        bookToDelete = null
                        onDeleteBook(book)
                    },
                    colors = ButtonDefaults.filledTonalButtonColors(
                        containerColor = MaterialTheme.colorScheme.errorContainer,
                        contentColor = MaterialTheme.colorScheme.onErrorContainer,
                    ),
                ) {
                    Text("Delete")
                }
            },
            dismissButton = {
                TextButton(onClick = { bookToDelete = null }) {
                    Text("Cancel")
                }
            },
        )
    }

    draftToDelete?.let { draft ->
        AlertDialog(
            onDismissRequest = { draftToDelete = null },
            icon = {
                Icon(imageVector = Icons.Outlined.Delete, contentDescription = null)
            },
            title = { Text("Delete saved article?") },
            text = { Text(draft.title) },
            confirmButton = {
                FilledTonalButton(
                    onClick = {
                        draftToDelete = null
                        onDeleteDraft(draft)
                    },
                    colors = ButtonDefaults.filledTonalButtonColors(
                        containerColor = MaterialTheme.colorScheme.errorContainer,
                        contentColor = MaterialTheme.colorScheme.onErrorContainer,
                    ),
                ) {
                    Text("Delete")
                }
            },
            dismissButton = {
                TextButton(onClick = { draftToDelete = null }) {
                    Text("Cancel")
                }
            },
        )
    }
}

@Composable
private fun UploadLibraryRow(
    onClick: () -> Unit,
) {
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick),
        color = MaterialTheme.colorScheme.primaryContainer,
        shape = MaterialTheme.shapes.small,
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 14.dp, vertical = 10.dp),
            horizontalArrangement = Arrangement.spacedBy(10.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                imageVector = Icons.Outlined.UploadFile,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.primary,
            )
            Text(
                text = "Upload",
                style = MaterialTheme.typography.titleSmall,
                color = MaterialTheme.colorScheme.onPrimaryContainer,
            )
        }
    }
}

@Composable
private fun PendingArticleRow(
    draft: PendingUpload,
    needsFetch: Boolean,
    onEdit: () -> Unit,
    onDelete: () -> Unit,
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow),
    ) {
        Column(
            modifier = Modifier.padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                Icon(
                    imageVector = Icons.Outlined.Newspaper,
                    contentDescription = null,
                    tint = if (needsFetch) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.primary,
                )
                Column(modifier = Modifier.weight(1f)) {
                    Text(text = draft.title, style = MaterialTheme.typography.titleSmall)
                    Text(
                        text = pendingArticleMeta(draft = draft, needsFetch = needsFetch),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                TextButton(onClick = onEdit) {
                    Icon(imageVector = Icons.Outlined.Edit, contentDescription = null)
                    Text("Edit")
                }
                FilledTonalButton(
                    onClick = onDelete,
                    colors = ButtonDefaults.filledTonalButtonColors(
                        containerColor = MaterialTheme.colorScheme.errorContainer,
                        contentColor = MaterialTheme.colorScheme.onErrorContainer,
                    ),
                ) {
                    Text("Delete")
                }
            }
        }
    }
}

@Composable
private fun LibraryBookRow(
    book: NanoBook,
    onOpenBook: () -> Unit,
    onDeleteBook: (NanoBook) -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onOpenBook)
            .padding(vertical = 10.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Icon(
                imageVector = if (book.isArticle) Icons.Outlined.Newspaper else Icons.AutoMirrored.Outlined.MenuBook,
                contentDescription = null,
                tint = if (book.isArticle) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.secondary,
            )
            Column(modifier = Modifier.weight(1f)) {
                Text(text = book.displayTitle, style = MaterialTheme.typography.titleSmall)
                Text(
                    text = book.libraryMetaLabel,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            DestructiveIconButton(
                contentDescription = "Delete",
                onClick = { onDeleteBook(book) },
            )
        }
        book.progressPercent?.let { progress ->
            LinearProgressIndicator(
                progress = { (progress.coerceIn(0, 100) / 100f) },
                modifier = Modifier.fillMaxWidth(),
            )
            Text(text = "$progress% read", style = MaterialTheme.typography.labelSmall)
        }
        HorizontalDivider()
    }
}

@Composable
private fun LibraryBookDialog(
    book: NanoBook,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        icon = {
            Icon(
                imageVector = if (book.isArticle) Icons.Outlined.Newspaper else Icons.AutoMirrored.Outlined.MenuBook,
                contentDescription = null,
            )
        },
        title = { Text(text = book.displayTitle) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                if (!book.author.isNullOrBlank()) {
                    Text(text = "Author: ${book.author}")
                }
                Text(text = "Size: ${book.byteLabel}")
                Text(text = "Path: ${book.id}")
                Text(text = "Type: ${if (book.isArticle) "Article" else "Book"}")
                book.progressPercent?.let { progress ->
                    Text(text = "Progress: ${progress.coerceIn(0, 100)}%")
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text(text = "Close")
            }
        },
    )
}

@Composable
private fun ArticlesTab(
    uiState: CompanionUiState,
    needsArticleFetch: (PendingUpload) -> Boolean,
    onTitleChange: (String) -> Unit,
    onSourceChange: (String) -> Unit,
    onBodyChange: (String) -> Unit,
    onSaveText: () -> Unit,
    onSaveLink: () -> Unit,
    onCancelEdit: () -> Unit,
    onEditDraft: (PendingUpload) -> Unit,
    onDeleteDraft: (PendingUpload) -> Unit,
    onSyncArticles: () -> Unit,
) {
    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(bottom = 18.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        item {
            SectionCard(title = "Saved Articles") {
                Text(
                    text = "Share articles while online, then connect to the Nano to sync them.",
                    style = MaterialTheme.typography.bodyMedium,
                )
                OutlinedTextField(
                    value = uiState.draftTitle,
                    onValueChange = onTitleChange,
                    label = { Text("Article title") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = uiState.draftSourceUrl,
                    onValueChange = onSourceChange,
                    label = { Text("Source URL") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = uiState.draftBody,
                    onValueChange = onBodyChange,
                    label = { Text("Article text") },
                    minLines = 3,
                    modifier = Modifier.fillMaxWidth(),
                )
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = onSaveText) {
                        Text(text = if (uiState.editingDraftId == null) "Save text" else "Update")
                    }
                    TextButton(onClick = onSaveLink) {
                        Text(text = if (uiState.editingDraftId == null) "Save link" else "Update link")
                    }
                    if (uiState.editingDraftId != null) {
                        TextButton(onClick = onCancelEdit) {
                            Text(text = "Cancel")
                        }
                    }
                }
            }
        }

        if (uiState.drafts.isEmpty()) {
            item { EmptyCard(text = "No saved articles yet.") }
        } else {
            items(uiState.drafts) { draft ->
                val urlOnly = needsArticleFetch(draft)
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow),
                ) {
                    Column(
                        modifier = Modifier.padding(14.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Text(
                            text = draft.title + if (urlOnly) " (URL only)" else "",
                            style = MaterialTheme.typography.titleSmall,
                        )
                        if (urlOnly) {
                            Text(
                                text = "Needs article text before sync.",
                                style = MaterialTheme.typography.bodySmall,
                            )
                        }
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            TextButton(onClick = { onEditDraft(draft) }) {
                                Text(text = "Edit")
                            }
                            FilledTonalButton(
                                onClick = { onDeleteDraft(draft) },
                                colors = ButtonDefaults.filledTonalButtonColors(
                                    containerColor = MaterialTheme.colorScheme.errorContainer,
                                    contentColor = MaterialTheme.colorScheme.onErrorContainer,
                                ),
                            ) {
                                Text(text = "Delete")
                            }
                        }
                    }
                }
            }
            item {
                Button(onClick = onSyncArticles, enabled = uiState.isConnected) {
                    Text(text = "Sync ready articles")
                }
            }
        }

    }
}

@Composable
private fun RssTab(
    uiState: CompanionUiState,
    onRefresh: () -> Unit,
    onFeedChange: (String) -> Unit,
    onAddFeed: () -> Unit,
    onSyncFeeds: () -> Unit,
    onDeleteFeed: (String) -> Unit,
) {
    var feedToDelete by remember { mutableStateOf<String?>(null) }
    PullRefreshBox(
        isRefreshing = uiState.isRefreshing,
        onRefresh = onRefresh,
    ) {
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            contentPadding = PaddingValues(bottom = 18.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            item {
                SectionCard(title = "RSS Feeds") {
                    Text(
                        text = "Feed URLs are saved locally, then synced to the reader for RSS downloads.",
                        style = MaterialTheme.typography.bodyMedium,
                    )
                    OutlinedTextField(
                        value = uiState.rssFeedDraft,
                        onValueChange = onFeedChange,
                        label = { Text("Feed URL") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = onAddFeed) {
                            Icon(imageVector = Icons.Outlined.RssFeed, contentDescription = null)
                            Text(text = "Add")
                        }
                        TextButton(onClick = onSyncFeeds, enabled = uiState.isConnected) {
                            Icon(imageVector = Icons.Outlined.Sync, contentDescription = null)
                            Text(text = "Sync")
                        }
                    }
                }
            }

            if (uiState.rssFeeds.isEmpty()) {
                item { EmptyCard(text = "No RSS feeds saved.") }
            } else {
                items(uiState.rssFeeds) { feed ->
                    Card(
                        modifier = Modifier.fillMaxWidth(),
                        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow),
                    ) {
                        Row(
                            modifier = Modifier.padding(14.dp),
                            horizontalArrangement = Arrangement.spacedBy(10.dp),
                        ) {
                            Icon(imageVector = Icons.Outlined.RssFeed, contentDescription = null)
                            Column(modifier = Modifier.weight(1f)) {
                                Text(text = feed, style = MaterialTheme.typography.bodyMedium)
                                Text(
                                    text = if (uiState.isConnected) "Ready to sync" else "Saved locally",
                                    style = MaterialTheme.typography.bodySmall,
                                )
                            }
                            DestructiveIconButton(
                                contentDescription = "Delete feed",
                                onClick = { feedToDelete = feed },
                            )
                        }
                    }
                }
            }
        }
    }

    feedToDelete?.let { feed ->
        AlertDialog(
            onDismissRequest = { feedToDelete = null },
            title = { Text("Delete RSS feed?") },
            text = { Text(feed) },
            confirmButton = {
                FilledTonalButton(
                    onClick = {
                    feedToDelete = null
                    onDeleteFeed(feed)
                    },
                    colors = ButtonDefaults.filledTonalButtonColors(
                        containerColor = MaterialTheme.colorScheme.errorContainer,
                        contentColor = MaterialTheme.colorScheme.onErrorContainer,
                    ),
                ) {
                    Text("Delete")
                }
            },
            dismissButton = {
                TextButton(onClick = { feedToDelete = null }) {
                    Text("Cancel")
                }
            },
        )
    }
}

@Composable
private fun SettingsTab(
    uiState: CompanionUiState,
    onRefresh: () -> Unit,
    onUpdateSettings: ((NanoSettings) -> NanoSettings) -> Unit,
    onAddressChange: (String) -> Unit,
    onConnectDefault: () -> Unit,
    onWifiSsidChange: (String) -> Unit,
    onWifiPasswordChange: (String) -> Unit,
    onSaveWifi: () -> Unit,
    onClearWifi: () -> Unit,
    onForgetRememberedNano: () -> Unit,
    hasPermissions: Boolean,
    onGrantPermissions: () -> Unit,
) {
    PullRefreshBox(
        isRefreshing = uiState.isRefreshing,
        onRefresh = onRefresh,
    ) {
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            contentPadding = PaddingValues(bottom = 18.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            val settings = uiState.settings
            item {
                SectionCard(title = "Reader Connection") {
                    if (!hasPermissions) {
                        Text(
                            text = "Wi-Fi permissions are required to scan for the RSVP Nano.",
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.tertiary,
                        )
                        Button(onClick = onGrantPermissions) {
                            Icon(imageVector = Icons.Outlined.Wifi, contentDescription = null)
                            Text(text = "Grant Wi-Fi permissions")
                        }
                        HorizontalDivider()
                    }

                    if (uiState.rememberedNano != null) {
                        val remembered = uiState.rememberedNano
                        Text(
                            text = "Remembered Nano: ${remembered.ssid}",
                            style = MaterialTheme.typography.bodyMedium,
                        )
                        TextButton(onClick = onForgetRememberedNano) {
                            Icon(imageVector = Icons.Outlined.Delete, contentDescription = null)
                            Text(text = "Forget this Nano")
                        }
                    } else {
                        Text(
                            text = if (uiState.isConnected) {
                                "No Nano remembered. Connected, but Android did not expose a Wi-Fi identity to remember."
                            } else {
                                "No Nano remembered. Use Connect to let the app find and remember a Nano."
                            },
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    HorizontalDivider()
                    Text(
                        text = "Default address used when checking for the Nano.",
                        style = MaterialTheme.typography.bodySmall,
                    )
                    OutlinedTextField(
                        value = uiState.address,
                        onValueChange = onAddressChange,
                        label = { Text("Reader address") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                    )
                    TextButton(onClick = onConnectDefault) {
                        Icon(imageVector = Icons.Outlined.Wifi, contentDescription = null)
                        Text(text = "Reset to 192.168.4.1")
                    }
                }
            }
        if (settings == null) {
            item {
                EmptyCard(text = if (uiState.isConnected) "Settings unavailable." else "Connect to load settings.")
            }
        } else {
            item {
                SectionCard(title = "Word Pacing") {
                    ChoiceRow(
                        label = "Reading mode",
                        selected = settings.reading.readerMode,
                        options = listOf("rsvp" to "One word", "scroll" to "Scroll"),
                        onSelected = { mode -> onUpdateSettings { it.withReaderMode(mode) } },
                    )
                    ChoiceRow(
                        label = "Pause behavior",
                        selected = settings.reading.pauseMode,
                        options = listOf("sentence_end" to "Sentence end", "instant" to "Immediate"),
                        onSelected = { mode -> onUpdateSettings { it.withPauseMode(mode) } },
                    )
                    SwitchRow(
                        label = "Accurate time estimate",
                        checked = settings.reading.accurateTimeEstimate,
                        onCheckedChange = { checked -> onUpdateSettings { it.withAccurateTimeEstimate(checked) } },
                    )
                    SliderRow(
                        label = "Base speed",
                        valueLabel = "${settings.reading.wpm} WPM",
                        value = settings.reading.wpm.toFloat(),
                        valueRange = 10f..1000f,
                        steps = 0,
                        onValueChangeFinished = { value -> onUpdateSettings { it.withWpm(snappedWpm(value.toInt())) } },
                    )
                    SliderRow(
                        label = "Long words",
                        valueLabel = "${settings.reading.pacing.longWordMs} ms",
                        value = settings.reading.pacing.longWordMs.toFloat(),
                        valueRange = 0f..600f,
                        steps = 11,
                        onValueChangeFinished = { value -> onUpdateSettings { it.withPacingLongWordMs(value.toInt().snapToStep(50).coerceIn(0, 600)) } },
                    )
                    SliderRow(
                        label = "Complexity",
                        valueLabel = "${settings.reading.pacing.complexWordMs} ms",
                        value = settings.reading.pacing.complexWordMs.toFloat(),
                        valueRange = 0f..600f,
                        steps = 11,
                        onValueChangeFinished = { value -> onUpdateSettings { it.withPacingComplexWordMs(value.toInt().snapToStep(50).coerceIn(0, 600)) } },
                    )
                    SliderRow(
                        label = "Punctuation",
                        valueLabel = "${settings.reading.pacing.punctuationMs} ms",
                        value = settings.reading.pacing.punctuationMs.toFloat(),
                        valueRange = 0f..600f,
                        steps = 11,
                        onValueChangeFinished = { value -> onUpdateSettings { it.withPacingPunctuationMs(value.toInt().snapToStep(50).coerceIn(0, 600)) } },
                    )
                    TextButton(onClick = {
                        onUpdateSettings {
                            it.withPacingLongWordMs(0)
                                .withPacingComplexWordMs(0)
                                .withPacingPunctuationMs(0)
                        }
                    }) {
                        Text(text = "Reset pacing")
                    }
                }
            }

            item {
                SectionCard(title = "Display") {
                    ChoiceRow(
                        label = "Display mode",
                        selected = when {
                            settings.display.nightMode -> "night"
                            settings.display.darkMode -> "dark"
                            else -> "light"
                        },
                        options = listOf("light" to "Light", "dark" to "Dark", "night" to "Night"),
                        onSelected = { mode ->
                            onUpdateSettings {
                                it.withAppearance(
                                    darkMode = mode == "dark" || mode == "night",
                                    nightMode = mode == "night",
                                )
                            }
                        },
                    )
                    SliderRow(
                        label = "Brightness",
                        valueLabel = "${settings.display.brightnessIndex + 1} / 5",
                        value = settings.display.brightnessIndex.toFloat(),
                        valueRange = 0f..4f,
                        steps = 3,
                        onValueChangeFinished = { value -> onUpdateSettings { it.withBrightnessIndex(value.toInt().coerceIn(0, 4)) } },
                    )
                    ChoiceRow(
                        label = "Reader hand",
                        selected = settings.display.handedness,
                        options = listOf("left" to "Left", "right" to "Right"),
                        onSelected = { hand -> onUpdateSettings { it.withHandedness(hand) } },
                    )
                    ChoiceRow(
                        label = "Footer label",
                        selected = settings.display.footerMetric,
                        options = listOf(
                            "percentage" to "Percent",
                            "chapter_time" to "Chapter time",
                            "book_time" to "Book time",
                        ),
                        onSelected = { metric -> onUpdateSettings { it.withFooterMetric(metric) } },
                    )
                    ChoiceRow(
                        label = "Battery label",
                        selected = settings.display.batteryLabel,
                        options = listOf("percent" to "Percent", "time_remaining" to "Time left", "voltage" to "Voltage"),
                        onSelected = { label -> onUpdateSettings { it.withBatteryLabel(label) } },
                    )
                    SwitchRow(
                        label = "Show battery while reading",
                        checked = settings.display.readingBattery,
                        onCheckedChange = { checked -> onUpdateSettings { it.withReadingBattery(checked) } },
                    )
                    SwitchRow(
                        label = "Show chapter while reading",
                        checked = settings.display.readingChapter,
                        onCheckedChange = { checked -> onUpdateSettings { it.withReadingChapter(checked) } },
                    )
                    SwitchRow(
                        label = "Show book percent while reading",
                        checked = settings.display.readingProgress,
                        onCheckedChange = { checked -> onUpdateSettings { it.withReadingProgress(checked) } },
                    )
                }
            }

            item {
                SectionCard(title = "Typography") {
                    ChoiceRow(
                        label = "Typeface",
                        selected = settings.typography.typeface,
                        options = listOf(
                            "standard" to "Standard",
                            "atkinson" to "Atkinson",
                            "open_dyslexic" to "OpenDyslexic",
                        ),
                        onSelected = { typeface -> onUpdateSettings { it.withTypeface(typeface) } },
                    )
                    SwitchRow(
                        label = "Focus highlight",
                        checked = settings.typography.focusHighlight,
                        onCheckedChange = { checked -> onUpdateSettings { it.withFocusHighlight(checked) } },
                    )
                    SwitchRow(
                        label = "Phantom words",
                        checked = settings.display.phantomWords,
                        onCheckedChange = { checked -> onUpdateSettings { it.withPhantomWords(checked) } },
                    )
                    SliderRow(
                        label = "Font size",
                        valueLabel = "${settings.display.fontSizeIndex + 1} / 3",
                        value = settings.display.fontSizeIndex.toFloat(),
                        valueRange = 0f..2f,
                        steps = 1,
                        onValueChangeFinished = { value -> onUpdateSettings { it.withFontSizeIndex(value.toInt().coerceIn(0, 2)) } },
                    )
                    SliderRow(
                        label = "Tracking",
                        valueLabel = "${settings.typography.tracking}",
                        value = settings.typography.tracking.toFloat(),
                        valueRange = -2f..3f,
                        steps = 4,
                        onValueChangeFinished = { value -> onUpdateSettings { it.withTracking(value.toInt().coerceIn(-2, 3)) } },
                    )
                    SliderRow(
                        label = "Anchor",
                        valueLabel = "${settings.typography.anchorPercent}%",
                        value = settings.typography.anchorPercent.toFloat(),
                        valueRange = 30f..40f,
                        steps = 9,
                        onValueChangeFinished = { value -> onUpdateSettings { it.withAnchorPercent(value.toInt().coerceIn(30, 40)) } },
                    )
                    SliderRow(
                        label = "Guide width",
                        valueLabel = "${settings.typography.guideWidth}",
                        value = settings.typography.guideWidth.toFloat(),
                        valueRange = 12f..30f,
                        steps = 8,
                        onValueChangeFinished = { value -> onUpdateSettings { it.withGuideWidth(value.toInt().snapToStep(2).coerceIn(12, 30)) } },
                    )
                    SliderRow(
                        label = "Guide gap",
                        valueLabel = "${settings.typography.guideGap}",
                        value = settings.typography.guideGap.toFloat(),
                        valueRange = 2f..8f,
                        steps = 5,
                        onValueChangeFinished = { value -> onUpdateSettings { it.withGuideGap(value.toInt().coerceIn(2, 8)) } },
                    )
                }
            }
        }

        if (settings != null && uiState.isConnected) {
            item {
                SectionCard(title = "Wi-Fi") {
                    val wifiStatus = uiState.wifiSettings?.let { wifi ->
                        if (wifi.configured) "Configured for ${wifi.ssid}" else "Not configured"
                    } ?: "Wi-Fi settings unavailable."
                    Text(text = wifiStatus)
                    OutlinedTextField(
                        value = uiState.wifiSsidDraft,
                        onValueChange = onWifiSsidChange,
                        label = { Text("Network name") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                    )
                    OutlinedTextField(
                        value = uiState.wifiPasswordDraft,
                        onValueChange = onWifiPasswordChange,
                        label = { Text("Password") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = onSaveWifi) {
                            Text(text = "Save")
                        }
                        FilledTonalButton(
                            onClick = onClearWifi,
                            colors = ButtonDefaults.filledTonalButtonColors(
                                containerColor = MaterialTheme.colorScheme.errorContainer,
                                contentColor = MaterialTheme.colorScheme.onErrorContainer,
                            ),
                        ) {
                            Text(text = "Forget")
                        }
                    }
                }
            }
        }
    }
}
}

@Composable
private fun ChoiceRow(
    label: String,
    selected: String,
    options: List<Pair<String, String>>,
    onSelected: (String) -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Text(text = label, style = MaterialTheme.typography.labelLarge)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            options.forEach { (value, title) ->
                if (value == selected) {
                    Button(onClick = { onSelected(value) }) {
                        Text(text = title)
                    }
                } else {
                    TextButton(onClick = { onSelected(value) }) {
                        Text(text = title)
                    }
                }
            }
        }
    }
}

@Composable
private fun SliderRow(
    label: String,
    valueLabel: String,
    value: Float,
    valueRange: ClosedFloatingPointRange<Float>,
    steps: Int,
    onValueChangeFinished: (Float) -> Unit,
) {
    var sliderValue by remember(value) { mutableStateOf(value) }
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text(text = label, modifier = Modifier.weight(1f), style = MaterialTheme.typography.labelLarge)
            Text(text = valueLabel, style = MaterialTheme.typography.bodyMedium)
        }
        Slider(
            value = sliderValue.coerceIn(valueRange.start, valueRange.endInclusive),
            onValueChange = { sliderValue = it },
            valueRange = valueRange,
            steps = steps,
            onValueChangeFinished = { onValueChangeFinished(sliderValue) },
        )
    }
}

@Composable
private fun StepperRow(
    label: String,
    valueLabel: String,
    onDecrease: () -> Unit,
    onIncrease: () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(text = label, style = MaterialTheme.typography.labelLarge)
            Text(text = valueLabel, style = MaterialTheme.typography.bodyMedium)
        }
        TextButton(onClick = onDecrease) {
            Text(text = "-")
        }
        Button(onClick = onIncrease) {
            Text(text = "+")
        }
    }
}

@Composable
private fun SwitchRow(
    label: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(
            text = label,
            modifier = Modifier.weight(1f),
            style = MaterialTheme.typography.labelLarge,
        )
        Switch(checked = checked, onCheckedChange = onCheckedChange)
    }
}

@Composable
private fun EmptyCard(text: String) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow),
    ) {
        Text(
            text = text,
            modifier = Modifier.padding(16.dp),
            style = MaterialTheme.typography.bodyMedium,
        )
    }
}

@OptIn(ExperimentalMaterialApi::class)
@Composable
private fun PullRefreshBox(
    isRefreshing: Boolean,
    onRefresh: () -> Unit,
    content: @Composable () -> Unit,
) {
    val pullRefreshState = rememberPullRefreshState(
        refreshing = isRefreshing,
        onRefresh = onRefresh,
    )
    Box(
        modifier = Modifier
            .fillMaxSize()
            .pullRefresh(pullRefreshState),
    ) {
        content()
        PullRefreshIndicator(
            refreshing = isRefreshing,
            state = pullRefreshState,
            modifier = Modifier.align(Alignment.TopCenter),
            backgroundColor = MaterialTheme.colorScheme.surface,
            contentColor = MaterialTheme.colorScheme.primary,
        )
    }
}

@Composable
private fun DestructiveIconButton(
    contentDescription: String,
    onClick: () -> Unit,
) {
    Surface(
        color = MaterialTheme.colorScheme.errorContainer,
        contentColor = MaterialTheme.colorScheme.onErrorContainer,
        shape = MaterialTheme.shapes.small,
    ) {
        IconButton(onClick = onClick) {
            Icon(imageVector = Icons.Outlined.Delete, contentDescription = contentDescription)
        }
    }
}

@Composable
private fun SectionCard(
    title: String,
    content: @Composable () -> Unit,
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(bottom = 14.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Text(text = title, style = MaterialTheme.typography.titleMedium)
            content()
        }
    }
}

private data class SelectedFile(
    val displayName: String,
    val data: ByteArray,
)

private val NanoBook.isArticle: Boolean
    get() = category == "article" || id.lowercase().startsWith("articles/")

private val NanoBook.libraryMetaLabel: String
    get() = listOfNotNull(
        author?.takeIf { it.isNotBlank() },
        byteLabel,
        id.takeIf { displayTitle != id.substringAfterLast('/') },
    ).joinToString(" · ").ifBlank { id }

private val NanoBook.byteLabel: String
    get() = bytes.toByteLabel()

private fun pendingArticleMeta(draft: PendingUpload, needsFetch: Boolean): String {
    val state = if (needsFetch) "Needs article text" else "Ready to sync"
    val source = draft.sourceUrl?.takeIf { it.isNotBlank() }?.substringAfter("://")?.substringBefore("/")
    return listOfNotNull(state, draft.body.encodeToByteArray().size.toByteLabel(), source).joinToString(" · ")
}

private fun Int.toByteLabel(): String {
    return when {
        this < 1024 -> "$this B"
        this < 1024 * 1024 -> String.format("%.1f KB", this / 1024.0)
        else -> String.format("%.1f MB", this / (1024.0 * 1024.0))
    }
}

private fun Int.snapToStep(step: Int): Int = ((this + step / 2) / step) * step

private fun snappedWpm(value: Int): Int {
    val clamped = value.coerceIn(10, 1000)
    return if (clamped <= 100) {
        clamped.snapToStep(10).coerceIn(10, 100)
    } else {
        (100 + (clamped - 100).snapToStep(25)).coerceIn(100, 1000)
    }
}

private fun Context.readSelectedFile(uri: Uri): SelectedFile? {
    val displayName = displayNameFor(uri) ?: "selected-book"
    val data = contentResolver.openInputStream(uri)?.use { it.readBytes() } ?: return null
    return SelectedFile(displayName = displayName, data = data)
}

private fun Context.sharedImportsFrom(intent: Intent): List<SharedImport> {
    if (!intent.isAndroidShareIntent()) {
        return emptyList()
    }

    val preferredTitle = intent.sharedTitle()
    val imports = mutableListOf<SharedImport>()
    val sharedText = intent.getCharSequenceExtra(Intent.EXTRA_TEXT)?.toString()?.trim()
    if (!sharedText.isNullOrEmpty()) {
        imports += SharedImport(
            title = preferredTitle,
            text = sharedText,
            source = sharedText.takeIf { it.isHttpUrl() }.orEmpty(),
        )
    }

    intent.sharedStreamUris().forEach { uri ->
        readSharedText(uri, preferredTitle)?.let(imports::add)
    }
    return imports
}

private fun Context.readSharedText(uri: Uri, preferredTitle: String): SharedImport? {
    val displayName = displayNameFor(uri) ?: preferredTitle.ifEmpty { "Shared Text" }
    val mimeType = contentResolver.getType(uri).orEmpty()
    if (!mimeType.isTextMimeType() && !displayName.isTextFileName()) {
        return null
    }
    val text = contentResolver.openInputStream(uri)?.use { it.readBytes().decodeToString() } ?: return null
    return SharedImport(
        title = preferredTitle.ifEmpty { displayName.substringBeforeLast('.', displayName) },
        text = text,
        source = uri.toString(),
    )
}

private fun Context.displayNameFor(uri: Uri): String? {
    contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
        if (cursor.moveToFirst()) {
            val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (index >= 0) {
                return cursor.getString(index)
            }
        }
    }
    return uri.lastPathSegment?.substringAfterLast('/')
}

private fun Intent.isAndroidShareIntent(): Boolean {
    return action == Intent.ACTION_SEND || action == Intent.ACTION_SEND_MULTIPLE
}

private fun Intent.sharedTitle(): String {
    return getStringExtra(Intent.EXTRA_TITLE)
        ?: getStringExtra(Intent.EXTRA_SUBJECT)
        ?: "Shared Text"
}

private fun Intent.sharedStreamUris(): List<Uri> {
    val uris = mutableListOf<Uri>()
    clipData?.let { data ->
        for (index in 0 until data.itemCount) {
            data.getItemAt(index).uri?.let(uris::add)
        }
    }
    extraStreamUri()?.let(uris::add)
    extraStreamUris().forEach(uris::add)
    return uris.distinctBy { it.toString() }
}

@Suppress("DEPRECATION")
private fun Intent.extraStreamUri(): Uri? {
    return runCatching { getParcelableExtra<Uri>(Intent.EXTRA_STREAM) }.getOrNull()
}

@Suppress("DEPRECATION")
private fun Intent.extraStreamUris(): List<Uri> {
    return runCatching { getParcelableArrayListExtra<Uri>(Intent.EXTRA_STREAM).orEmpty() }.getOrDefault(emptyList())
}

private fun String.isHttpUrl(): Boolean {
    val value = trim()
    return value.startsWith("http://") || value.startsWith("https://")
}

private fun String.isTextMimeType(): Boolean = startsWith("text/")

private fun String.isTextFileName(): Boolean {
    return RsvpSupportedFileTypes.isTextLike(this)
}

private fun Context.openWifiSettings() {
    val intent = Intent(Settings.Panel.ACTION_INTERNET_CONNECTIVITY)
        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
    runCatching { startActivity(intent) }
        .recover {
            startActivity(
                Intent(Settings.ACTION_WIFI_SETTINGS)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
            )
        }
}

private fun nanoWifiPermissions(): Array<String> {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        arrayOf(
            android.Manifest.permission.ACCESS_FINE_LOCATION,
            android.Manifest.permission.NEARBY_WIFI_DEVICES,
        )
    } else {
        arrayOf(android.Manifest.permission.ACCESS_FINE_LOCATION)
    }
}
