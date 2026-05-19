package com.rsvpnano.android.ui

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.OpenableColumns
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.viewmodel.compose.viewModel
import com.rsvpnano.app.RsvpSharedApp
import com.rsvpnano.converters.RsvpSupportedFileTypes
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

@Composable
fun CompanionApp(
    sharedApp: RsvpSharedApp,
    shareIntent: Intent? = null,
    onShareIntentHandled: () -> Unit = {},
) {
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current
    val viewModel: CompanionViewModel = viewModel(factory = CompanionViewModel.Factory(sharedApp))
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
    DisposableEffect(lifecycleOwner, viewModel) {
        val observer = LifecycleEventObserver { _, event ->
            if (event == Lifecycle.Event.ON_RESUME) {
                viewModel.recheckConnectionAfterResume()
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
    }
    LaunchedEffect(shareIntent) {
        val intent = shareIntent ?: return@LaunchedEffect
        val imports = withContext(Dispatchers.IO) { context.sharedImportsFrom(intent) }
        if (imports.isNotEmpty() || intent.isAndroidShareIntent()) {
            viewModel.saveSharedImports(imports)
        }
        onShareIntentHandled()
    }

    MaterialTheme {
        Surface(modifier = Modifier.fillMaxSize()) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(24.dp),
                verticalArrangement = Arrangement.spacedBy(16.dp),
            ) {
                Text(text = "RSVP Nano Companion", style = MaterialTheme.typography.headlineSmall)
                Text(text = uiState.status, style = MaterialTheme.typography.bodyMedium)

                if (!uiState.isConnected) {
                    ConnectionPanel(
                        uiState = uiState,
                        onOpenWifiSettings = context::openWifiSettings,
                        onConnectDefault = viewModel::connectDefault,
                        onShowAddressEntry = viewModel::showAddressEntry,
                        onAddressChange = viewModel::setAddress,
                        onConnectCustom = viewModel::connect,
                    )
                } else {
                    Button(onClick = viewModel::connect) {
                        Text(text = "Reconnect")
                    }
                }

                Button(onClick = viewModel::refresh) {
                    Text(text = "Refresh local")
                }
                Button(
                    onClick = {
                        filePicker.launch(
                            arrayOf(
                                "application/epub+zip",
                                "text/*",
                                "text/html",
                                "application/octet-stream",
                            ),
                        )
                    },
                ) {
                    Text(text = "Upload file")
                }

                Spacer(modifier = Modifier.height(8.dp))

                Text(text = "Device Library", style = MaterialTheme.typography.titleMedium)
                LazyColumn(modifier = Modifier.weight(1f, fill = true)) {
                    if (uiState.books.isEmpty()) {
                        item { Text(text = if (uiState.isConnected) "No books on device." else "Connect to load books.") }
                    } else {
                        items(uiState.books) { book ->
                            Column {
                                Text(text = book.displayTitle)
                                Button(onClick = { viewModel.deleteDeviceBook(book) }) {
                                    Text(text = "Delete from reader")
                                }
                            }
                        }
                    }

                    item {
                        Spacer(modifier = Modifier.height(16.dp))
                        Text(text = "Reader Settings", style = MaterialTheme.typography.titleMedium)
                    }
                    item {
                        if (uiState.settings == null) {
                            Text(text = if (uiState.isConnected) "Reader settings unavailable." else "Connect to load settings.")
                        } else {
                            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                                OutlinedTextField(
                                    value = uiState.settingsWpmDraft,
                                    onValueChange = viewModel::setSettingsWpmDraft,
                                    label = { Text("Words per minute") },
                                    singleLine = true,
                                )
                                OutlinedTextField(
                                    value = uiState.settingsBrightnessDraft,
                                    onValueChange = viewModel::setSettingsBrightnessDraft,
                                    label = { Text("Brightness index") },
                                    singleLine = true,
                                )
                                Button(onClick = viewModel::saveDeviceSettings) {
                                    Text(text = "Save reader settings")
                                }
                            }
                        }
                    }

                    item {
                        Spacer(modifier = Modifier.height(16.dp))
                        Text(text = "Wi-Fi", style = MaterialTheme.typography.titleMedium)
                    }
                    item {
                        val wifiStatus = uiState.wifiSettings?.let { wifi ->
                            if (wifi.configured) {
                                "Configured for ${wifi.ssid}"
                            } else {
                                "Not configured"
                            }
                        } ?: if (uiState.isConnected) {
                            "Wi-Fi settings unavailable."
                        } else {
                            "Connect to load Wi-Fi settings."
                        }
                        Text(text = wifiStatus)
                    }
                    item {
                        OutlinedTextField(
                            value = uiState.wifiSsidDraft,
                            onValueChange = viewModel::setWifiSsidDraft,
                            label = { Text("Wi-Fi SSID") },
                            singleLine = true,
                        )
                    }
                    item {
                        OutlinedTextField(
                            value = uiState.wifiPasswordDraft,
                            onValueChange = viewModel::setWifiPasswordDraft,
                            label = { Text("Wi-Fi password") },
                            singleLine = true,
                        )
                    }
                    item {
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Button(onClick = viewModel::saveWifiSettings) {
                                Text(text = "Save Wi-Fi")
                            }
                            Button(onClick = viewModel::clearWifiSettings) {
                                Text(text = "Forget Wi-Fi")
                            }
                        }
                    }

                    item {
                        Spacer(modifier = Modifier.height(16.dp))
                        Text(text = "Saved Articles", style = MaterialTheme.typography.titleMedium)
                    }
                    if (uiState.editingDraftId != null) {
                        item {
                            Text(text = "Editing saved article", style = MaterialTheme.typography.bodyMedium)
                        }
                    }

                    item {
                        OutlinedTextField(
                            value = uiState.draftTitle,
                            onValueChange = viewModel::setDraftTitle,
                            label = { Text("Article title") },
                            singleLine = true,
                        )
                    }
                    item {
                        OutlinedTextField(
                            value = uiState.draftSourceUrl,
                            onValueChange = viewModel::setDraftSourceUrl,
                            label = { Text("Source URL") },
                            singleLine = true,
                        )
                    }
                    item {
                        OutlinedTextField(
                            value = uiState.draftBody,
                            onValueChange = viewModel::setDraftBody,
                            label = { Text("Article text") },
                            minLines = 3,
                        )
                    }
                    item {
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Button(onClick = viewModel::saveTextDraft) {
                                Text(text = if (uiState.editingDraftId == null) "Save text" else "Update text")
                            }
                            Button(onClick = viewModel::saveLinkDraft) {
                                Text(text = if (uiState.editingDraftId == null) "Save link" else "Update link")
                            }
                            if (uiState.editingDraftId != null) {
                                Button(onClick = viewModel::cancelDraftEdit) {
                                    Text(text = "Cancel")
                                }
                            }
                        }
                    }

                    if (uiState.drafts.isEmpty()) {
                        item {
                            Text(text = "No drafts yet.")
                        }
                    } else {
                        items(uiState.drafts) { draft ->
                            val suffix = if (viewModel.needsArticleFetch(draft)) " (URL only)" else ""
                            Column {
                                Text(text = draft.title + suffix)
                                if (viewModel.needsArticleFetch(draft)) {
                                    Text(
                                        text = "Open this draft while you still have internet and add article text before syncing.",
                                        style = MaterialTheme.typography.bodySmall,
                                    )
                                }
                                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                    Button(onClick = { viewModel.editDraft(draft) }) {
                                        Text(text = "Edit")
                                    }
                                    Button(onClick = { viewModel.deleteDraft(draft) }) {
                                        Text(text = "Delete")
                                    }
                                }
                            }
                        }
                        item {
                            Button(onClick = viewModel::syncSavedArticles) {
                                Text(text = "Sync saved articles")
                            }
                        }
                    }

                    item {
                        Spacer(modifier = Modifier.height(16.dp))
                        Text(text = "RSS Feeds", style = MaterialTheme.typography.titleMedium)
                    }

                    if (uiState.rssFeeds.isEmpty()) {
                        item { Text(text = "No feeds saved.") }
                    } else {
                        items(uiState.rssFeeds) { feed ->
                            Text(text = feed)
                        }
                    }

                    item {
                        OutlinedTextField(
                            value = uiState.rssFeedDraft,
                            onValueChange = viewModel::setRssFeedDraft,
                            label = { Text("RSS feed URL") },
                            singleLine = true,
                        )
                    }

                    item {
                        Button(onClick = viewModel::addRssFeed) {
                            Text(text = "Add RSS feed")
                        }
                    }

                    item {
                        Button(onClick = viewModel::syncRssFeeds) {
                            Text(text = "Sync RSS feeds")
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun ConnectionPanel(
    uiState: CompanionUiState,
    onOpenWifiSettings: () -> Unit,
    onConnectDefault: () -> Unit,
    onShowAddressEntry: () -> Unit,
    onAddressChange: (String) -> Unit,
    onConnectCustom: () -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Text(text = "Connect to RSVP Nano", style = MaterialTheme.typography.titleMedium)
        Text(
            text = "Open Companion Sync on the reader, then join the RSVP-Nano Wi-Fi. The app checks the reader automatically when you return.",
            style = MaterialTheme.typography.bodyMedium,
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = onOpenWifiSettings) {
                Text(text = "Join Nano Wi-Fi")
            }
            Button(onClick = onConnectDefault) {
                Text(text = "Check Now")
            }
        }

        Text(text = "Fallback", style = MaterialTheme.typography.titleSmall)
        Text(
            text = "If Android does not show the network picker, open Wi-Fi settings manually, join the network shown on the reader, then return to the app.",
            style = MaterialTheme.typography.bodyMedium,
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = onOpenWifiSettings) {
                Text(text = "Open Wi-Fi Settings")
            }
            Button(onClick = onShowAddressEntry) {
                Text(text = "Enter IP Address")
            }
        }

        if (uiState.showAddressEntry) {
            Text(
                text = "RSVP Nano was not found at http://192.168.4.1. If the reader shows a different address, enter it here.",
                style = MaterialTheme.typography.bodyMedium,
            )
            OutlinedTextField(
                value = uiState.address,
                onValueChange = onAddressChange,
                label = { Text("Reader address or IP") },
                singleLine = true,
            )
            Button(onClick = onConnectCustom) {
                Text(text = "Connect to This Address")
            }
        }
    }
}

private data class SelectedFile(
    val displayName: String,
    val data: ByteArray,
)

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
