package com.rsvpnano.android.ui

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.ContextWrapper
import android.os.Build
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.Uri
import android.provider.OpenableColumns
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
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
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.FabPosition
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
import androidx.compose.material3.Snackbar
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.SnackbarResult
import androidx.compose.material3.SnackbarDefaults
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
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
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
import com.rsvpnano.app.CompanionNotice
import com.rsvpnano.app.CompanionNotice.Attention
import com.rsvpnano.app.CompanionNotice.Error
import com.rsvpnano.app.CompanionNotice.Success
import com.rsvpnano.app.RsvpSharedApp
import com.rsvpnano.converters.RsvpSupportedFileTypes
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.PendingUpload
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

private enum class CompanionTab(val label: String, val icon: ImageVector) {
    Library("Library", Icons.AutoMirrored.Outlined.LibraryBooks),
    Settings("Settings", Icons.Outlined.Settings),
}

private enum class PermissionFallback {
    WifiSettings,
    AppSettings,
}

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
    var permissionRequestAttempted by remember { mutableStateOf(false) }
    var permissionBlockedFallback by remember { mutableStateOf(PermissionFallback.WifiSettings) }
    val nanoWifiPermissionLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.RequestPermission(),
    ) { permissionGranted ->
        val granted = permissionGranted || nanoNetworkController.hasRequiredPermissions()
        if (granted) {
            viewModel.connectNanoScan()
        } else {
            val permission = nanoWifiPermission()
            val canAskAgain = context.findActivity()?.shouldShowRequestPermissionRationale(permission) == true
            if (canAskAgain) {
                viewModel.scanPermissionDenied()
            } else if (permissionBlockedFallback == PermissionFallback.AppSettings) {
                viewModel.wifiPermissionsBlocked()
                context.openAppSettings()
            } else {
                viewModel.scanPermissionDenied()
                context.openWifiSettings()
            }
        }
    }
    fun connectNanoFromApp(openWifiSettingsOnBlocked: Boolean) {
        if (nanoNetworkController.hasRequiredPermissions()) {
            viewModel.connectNanoScan()
        } else {
            val permission = nanoWifiPermission()
            val canAskAgain = !permissionRequestAttempted ||
                context.findActivity()?.shouldShowRequestPermissionRationale(permission) == true
            viewModel.requestWifiPermissions()
            if (canAskAgain) {
                permissionRequestAttempted = true
                permissionBlockedFallback = if (openWifiSettingsOnBlocked) {
                    PermissionFallback.WifiSettings
                } else {
                    PermissionFallback.AppSettings
                }
                nanoWifiPermissionLauncher.launch(permission)
            } else if (openWifiSettingsOnBlocked) {
                viewModel.scanPermissionDenied()
                context.openWifiSettings()
            } else {
                viewModel.wifiPermissionsBlocked()
                context.openAppSettings()
            }
        }
    }
    DisposableEffect(lifecycleOwner, viewModel) {
        val observer = LifecycleEventObserver { _, event ->
            when (event) {
                Lifecycle.Event.ON_RESUME -> {
                    nanoNetworkController.refreshSnapshot()
                    viewModel.recheckConnectionAfterResume()
                }
                else -> Unit
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
            val snackbarNotices = remember { mutableStateMapOf<String, CompanionNotice>() }
            LaunchedEffect(uiState.notice) {
                if (uiState.notice.showTransient) {
                    snackbarNotices[uiState.status] = uiState.notice
                    snackbarHostState.showSnackbar(uiState.status)
                }
            }
            LaunchedEffect(uiState.canRememberCurrentNano, uiState.nanoSsid) {
                if (uiState.canRememberCurrentNano && !uiState.nanoSsid.isNullOrBlank()) {
                    val result = snackbarHostState.showSnackbar(
                        message = "Remember ${uiState.nanoSsid} for faster reconnects?",
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
                                            snackbarHostState.showSnackbar("Open Companion Sync on the reader, then connect from this app.")
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
                            onShowAddressEntry = viewModel::showAddressEntry,
                            onAddressChange = viewModel::setAddress,
                            onConnectCustom = viewModel::connect,
                        )
                    }
                },
                snackbarHost = {
                    SnackbarHost(hostState = snackbarHostState) { data ->
                        val snackbarNotice = snackbarNotices[data.visuals.message]
                            ?: CompanionNotice.Neutral(data.visuals.message)
                        Snackbar(
                            snackbarData = data,
                            containerColor = snackbarColor(snackbarNotice),
                            contentColor = snackbarContentColor(snackbarNotice),
                            actionColor = snackbarActionColor(snackbarNotice),
                            dismissActionContentColor = SnackbarDefaults.dismissActionContentColor,
                        )
                    }
                },
                floatingActionButton = {
                    NanoConnectFab(
                        uiState = uiState,
                        onConnect = { connectNanoFromApp(openWifiSettingsOnBlocked = true) },
                    )
                },
                floatingActionButtonPosition = FabPosition.End,
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
                            onGrantPermissions = { connectNanoFromApp(openWifiSettingsOnBlocked = false) },
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

