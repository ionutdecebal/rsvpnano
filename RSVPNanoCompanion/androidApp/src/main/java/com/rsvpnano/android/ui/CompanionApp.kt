package com.rsvpnano.android.ui

import android.content.Context
import android.content.Intent
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.viewmodel.compose.viewModel
import com.rsvpnano.android.net.AndroidNanoNetworkController
import com.rsvpnano.app.RsvpSharedApp
import com.rsvpnano.ui.RsvpNanoSharedApp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

private enum class PermissionFallback {
    WifiSettings,
    AppSettings,
}

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
            if (event == Lifecycle.Event.ON_RESUME) {
                nanoNetworkController.refreshSnapshot()
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

    RsvpNanoSharedApp(
        uiState = uiState,
        hasPermissions = nanoNetworkController.hasRequiredPermissions(),
        onRefresh = viewModel::refresh,
        onConnect = { connectNanoFromApp(openWifiSettingsOnBlocked = true) },
        onShowHelp = { viewModel.showHelpNotice() },
        onUpdateSettings = viewModel::updateSettings,
        onAddressChange = viewModel::setAddress,
        onConnectDefault = viewModel::connectDefault,
        onWifiSsidChange = viewModel::setWifiSsidDraft,
        onWifiPasswordChange = viewModel::setWifiPasswordDraft,
        onSaveWifi = viewModel::saveWifiSettings,
        onClearWifi = viewModel::clearWifiSettings,
        onForgetRememberedNano = viewModel::forgetRememberedNano,
        onGrantPermissions = { connectNanoFromApp(openWifiSettingsOnBlocked = false) },
        needsArticleFetch = viewModel::needsArticleFetch,
        onEditDraft = viewModel::editDraft,
        onCancelDraftEdit = viewModel::cancelDraftEdit,
        onDraftTitleChange = viewModel::setDraftTitle,
        onDraftSourceChange = viewModel::setDraftSourceUrl,
        onDraftBodyChange = viewModel::setDraftBody,
        onSaveTextDraft = viewModel::saveTextDraft,
        onSaveLinkDraft = viewModel::saveLinkDraft,
        onDeleteDraft = viewModel::deleteDraft,
        onSyncArticles = viewModel::syncSavedArticles,
        onDeleteBook = viewModel::deleteDeviceBook,
        onPickBook = viewModel::uploadSelectedFile,
        onRssFeedChange = viewModel::setRssFeedDraft,
        onAddRssFeed = viewModel::addRssFeed,
        onRefreshRssFeeds = viewModel::refreshRssFeeds,
        onDeleteFeed = viewModel::deleteRssFeed,
    )
}
