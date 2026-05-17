package com.rsvpnano.android.ui

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
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
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.rsvpnano.app.NanoDeviceSyncService
import com.rsvpnano.app.RsvpSharedApp
import com.rsvpnano.converters.RsvpConverter
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoWifiSettings
import com.rsvpnano.models.PendingUpload
import java.time.Instant
import java.util.UUID
import kotlinx.coroutines.launch

private data class CompanionUiState(
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

@Composable
fun CompanionApp(sharedApp: RsvpSharedApp) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val deviceSyncService: NanoDeviceSyncService = sharedApp.deviceSyncService
    var uiState by remember { mutableStateOf(CompanionUiState(status = "Loading shared data...")) }

    fun refresh() {
        scope.launch {
            uiState = uiState.copy(status = "Refreshing...")
            val drafts = sharedApp.facade.loadDrafts()
            val feeds = sharedApp.facade.loadRssFeeds()
            uiState = uiState.copy(drafts = drafts, rssFeeds = feeds, status = "Loaded ${drafts.size} drafts.")
        }
    }

    fun connect() {
        scope.launch {
            uiState = uiState.copy(status = "Connecting...")
            runCatching { deviceSyncService.connect(uiState.address) }
                .onSuccess { snapshot ->
                    val deviceName = snapshot.info?.name ?: "RSVP Nano"
                    val mergedFeeds = sharedApp.facade.saveRssFeeds(
                        sharedApp.facade.mergeRssFeeds(
                            localFeeds = uiState.rssFeeds,
                            deviceFeeds = snapshot.rssFeeds?.feeds.orEmpty(),
                        )
                    )
                    uiState = uiState.copy(
                        books = snapshot.books,
                        settings = snapshot.settings,
                        wifiSettings = snapshot.wifiSettings,
                        settingsWpmDraft = snapshot.settings?.reading?.wpm?.toString().orEmpty(),
                        settingsBrightnessDraft = snapshot.settings?.display?.brightnessIndex?.toString().orEmpty(),
                        wifiSsidDraft = snapshot.wifiSettings?.ssid.orEmpty(),
                        wifiPasswordDraft = "",
                        rssFeeds = mergedFeeds,
                        isConnected = snapshot.info != null,
                        status = "Connected to $deviceName. Loaded ${snapshot.books.size} books.",
                    )
                }
                .onFailure { error ->
                    uiState = uiState.copy(isConnected = false, status = error.message ?: "Connection failed.")
                }
        }
    }

    fun saveDeviceSettings() {
        scope.launch {
            val currentSettings = uiState.settings
            if (!uiState.isConnected || currentSettings == null) {
                uiState = uiState.copy(status = "Connect to the reader before saving settings.")
                return@launch
            }
            val wpm = uiState.settingsWpmDraft.toIntOrNull()
            val brightness = uiState.settingsBrightnessDraft.toIntOrNull()
            if (wpm == null || brightness == null) {
                uiState = uiState.copy(status = "WPM and brightness must be whole numbers.")
                return@launch
            }
            val settings = currentSettings.copy(
                reading = currentSettings.reading.copy(wpm = wpm),
                display = currentSettings.display.copy(brightnessIndex = brightness),
            )
            uiState = uiState.copy(status = "Saving reader settings...")
            runCatching { deviceSyncService.saveSettings(uiState.address, settings) }
                .onSuccess { saved ->
                    uiState = uiState.copy(
                        settings = saved,
                        settingsWpmDraft = saved.reading.wpm.toString(),
                        settingsBrightnessDraft = saved.display.brightnessIndex.toString(),
                        status = "Reader settings saved.",
                    )
                }
                .onFailure { error ->
                    uiState = uiState.copy(status = error.message ?: "Settings save failed.")
                }
        }
    }

    fun saveWifiSettings() {
        scope.launch {
            if (!uiState.isConnected) {
                uiState = uiState.copy(status = "Connect to the reader before saving Wi-Fi.")
                return@launch
            }
            val ssid = uiState.wifiSsidDraft.trim()
            if (ssid.isEmpty()) {
                uiState = uiState.copy(status = "Wi-Fi SSID is required.")
                return@launch
            }
            uiState = uiState.copy(status = "Saving Wi-Fi settings...")
            runCatching { deviceSyncService.saveWifiSettings(uiState.address, ssid, uiState.wifiPasswordDraft) }
                .onSuccess { wifi ->
                    uiState = uiState.copy(
                        wifiSettings = wifi,
                        wifiSsidDraft = wifi.ssid,
                        wifiPasswordDraft = "",
                        status = "Wi-Fi settings saved.",
                    )
                }
                .onFailure { error ->
                    uiState = uiState.copy(status = error.message ?: "Wi-Fi save failed.")
                }
        }
    }

    fun clearWifiSettings() {
        scope.launch {
            if (!uiState.isConnected) {
                uiState = uiState.copy(status = "Connect to the reader before clearing Wi-Fi.")
                return@launch
            }
            uiState = uiState.copy(status = "Clearing Wi-Fi settings...")
            runCatching { deviceSyncService.clearWifiSettings(uiState.address) }
                .onSuccess { wifi ->
                    uiState = uiState.copy(
                        wifiSettings = wifi,
                        wifiSsidDraft = wifi.ssid,
                        wifiPasswordDraft = "",
                        status = "Wi-Fi settings cleared.",
                    )
                }
                .onFailure { error ->
                    uiState = uiState.copy(status = error.message ?: "Wi-Fi clear failed.")
                }
        }
    }

    fun addRssFeed() {
        scope.launch {
            val feed = uiState.rssFeedDraft.trim()
            if (!feed.startsWith("http://") && !feed.startsWith("https://")) {
                uiState = uiState.copy(status = "RSS feed URLs must start with http:// or https://.")
                return@launch
            }
            val feeds = sharedApp.facade.saveRssFeeds(uiState.rssFeeds + feed)
            uiState = uiState.copy(
                rssFeeds = feeds,
                rssFeedDraft = "",
                status = if (uiState.isConnected) "RSS feed saved locally. Sync to write it to the reader." else "RSS feed saved locally.",
            )
        }
    }

    fun clearDraftEditor(
        drafts: List<PendingUpload> = uiState.drafts,
        status: String,
    ): CompanionUiState {
        return uiState.copy(
            drafts = drafts,
            draftTitle = "",
            draftSourceUrl = "",
            draftBody = "",
            editingDraftId = null,
            status = status,
        )
    }

    fun saveTextDraft() {
        scope.launch {
            val title = uiState.draftTitle.trim()
            val body = uiState.draftBody.trim()
            if (title.isEmpty() || body.isEmpty()) {
                uiState = uiState.copy(status = "Text drafts need a title and body.")
                return@launch
            }
            val existing = uiState.editingDraftId?.let { id -> uiState.drafts.firstOrNull { it.id == id } }
            sharedApp.facade.saveDraft(
                PendingUpload(
                    id = existing?.id ?: UUID.randomUUID().toString(),
                    title = title,
                    sourceUrl = uiState.draftSourceUrl.trim().ifEmpty { null },
                    body = body,
                    createdAt = existing?.createdAt ?: Instant.now().toString(),
                )
            )
            uiState = clearDraftEditor(
                drafts = sharedApp.facade.loadDrafts(),
                status = if (existing == null) "Text draft saved locally." else "Text draft updated.",
            )
        }
    }

    fun saveLinkDraft() {
        scope.launch {
            val title = uiState.draftTitle.trim()
            val sourceUrl = uiState.draftSourceUrl.trim()
            if (title.isEmpty() || !sourceUrl.startsWith("http://") && !sourceUrl.startsWith("https://")) {
                uiState = uiState.copy(status = "Saved links need a title and http:// or https:// URL.")
                return@launch
            }
            val existing = uiState.editingDraftId?.let { id -> uiState.drafts.firstOrNull { it.id == id } }
            sharedApp.facade.saveDraft(
                PendingUpload(
                    id = existing?.id ?: UUID.randomUUID().toString(),
                    title = title,
                    sourceUrl = sourceUrl,
                    body = "",
                    createdAt = existing?.createdAt ?: Instant.now().toString(),
                )
            )
            uiState = clearDraftEditor(
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
        uiState = uiState.copy(
            draftTitle = draft.title,
            draftSourceUrl = draft.sourceUrl.orEmpty(),
            draftBody = draft.body,
            editingDraftId = draft.id,
            status = "Editing ${draft.title}.",
        )
    }

    fun deleteDraft(draft: PendingUpload) {
        scope.launch {
            sharedApp.facade.deleteDraft(draft)
            val editingDeletedDraft = uiState.editingDraftId == draft.id
            uiState = if (editingDeletedDraft) {
                clearDraftEditor(drafts = sharedApp.facade.loadDrafts(), status = "Draft deleted.")
            } else {
                uiState.copy(drafts = sharedApp.facade.loadDrafts(), status = "Draft deleted.")
            }
        }
    }

    fun syncRssFeeds() {
        scope.launch {
            if (!uiState.isConnected) {
                uiState = uiState.copy(status = "Connect to the reader before syncing RSS feeds.")
                return@launch
            }
            uiState = uiState.copy(status = "Syncing RSS feeds...")
            runCatching {
                val normalized = sharedApp.facade.saveRssFeeds(uiState.rssFeeds)
                deviceSyncService.saveRssFeeds(uiState.address, normalized).feeds
            }.onSuccess { deviceFeeds ->
                val merged = sharedApp.facade.saveRssFeeds(
                    sharedApp.facade.mergeRssFeeds(uiState.rssFeeds, deviceFeeds)
                )
                uiState = uiState.copy(rssFeeds = merged, status = "RSS feeds synced to the reader.")
            }.onFailure { error ->
                uiState = uiState.copy(status = error.message ?: "RSS sync failed.")
            }
        }
    }

    fun syncSavedArticles() {
        scope.launch {
            if (!uiState.isConnected) {
                uiState = uiState.copy(status = "Connect to the reader before syncing saved articles.")
                return@launch
            }
            val readyDrafts = uiState.drafts.filterNot(sharedApp.facade::needsArticleFetch)
            if (readyDrafts.isEmpty()) {
                uiState = uiState.copy(status = "No text drafts are ready to sync.")
                return@launch
            }
            val client = sharedApp.nanoClient ?: run {
                uiState = uiState.copy(status = "NanoClient not available.")
                return@launch
            }
            uiState = uiState.copy(status = "Syncing saved articles...")
            runCatching {
                val remaining = sharedApp.facade.syncPendingUploads(
                    client = client,
                    baseUrl = uiState.address,
                    items = readyDrafts,
                )
                val books = deviceSyncService.refreshBooks(uiState.address)
                remaining to books
            }.onSuccess { (remaining, books) ->
                uiState = uiState.copy(
                    drafts = remaining,
                    books = books,
                    status = "Synced ${readyDrafts.size} saved articles.",
                )
            }.onFailure { error ->
                uiState = uiState.copy(status = error.message ?: "Saved article sync failed.")
            }
        }
    }

    fun deleteDeviceBook(book: NanoBook) {
        scope.launch {
            if (!uiState.isConnected) {
                uiState = uiState.copy(status = "Connect to the reader before deleting books.")
                return@launch
            }
            val filename = book.apiName
            val title = book.displayTitle
            if (filename.isBlank()) {
                uiState = uiState.copy(status = "Cannot delete a book without a device filename.")
                return@launch
            }
            uiState = uiState.copy(status = "Deleting $title...")
            runCatching {
                deviceSyncService.deleteBook(uiState.address, filename)
                deviceSyncService.refreshBooks(uiState.address)
            }.onSuccess { books ->
                uiState = uiState.copy(books = books, status = "Deleted $title.")
            }.onFailure { error ->
                uiState = uiState.copy(status = error.message ?: "Book delete failed.")
            }
        }
    }

    fun uploadSelectedFile(uri: Uri) {
        scope.launch {
            if (!uiState.isConnected) {
                uiState = uiState.copy(status = "Connect to the reader before uploading files.")
                return@launch
            }
            val displayName = context.displayNameFor(uri) ?: "selected-book"
            uiState = uiState.copy(status = "Uploading $displayName...")
            runCatching {
                val bytes = context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
                    ?: error("Could not read selected file.")
                val file = RsvpConverter.bookFile(data = bytes, filename = displayName)
                deviceSyncService.uploadBook(
                    baseUrl = uiState.address,
                    filename = file.filename,
                    data = file.data,
                    category = "book",
                )
                deviceSyncService.refreshBooks(uiState.address)
            }.onSuccess { books ->
                uiState = uiState.copy(books = books, status = "Uploaded $displayName.")
            }.onFailure { error ->
                uiState = uiState.copy(status = error.message ?: "File upload failed.")
            }
        }
    }

    val filePicker = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument(),
    ) { uri ->
        if (uri != null) {
            uploadSelectedFile(uri)
        }
    }

    fun fetchMissingArticles() {
        scope.launch {
            val missing = uiState.drafts.filter(sharedApp.facade::needsArticleFetch)
            if (missing.isEmpty()) {
                uiState = uiState.copy(status = "No saved links need fetching.")
                return@launch
            }
            uiState = uiState.copy(status = "Fetching ${missing.size} saved links...")
            runCatching {
                missing.forEach { draft ->
                    val article = sharedApp.facade.fetchArticle(title = draft.title, source = draft.sourceUrl.orEmpty())
                    sharedApp.facade.updateDraft(draft, article.title, article.text)
                }
                sharedApp.facade.loadDrafts()
            }.onSuccess { drafts ->
                uiState = uiState.copy(drafts = drafts, status = "Fetched ${missing.size} saved links.")
            }.onFailure { error ->
                uiState = uiState.copy(status = error.message ?: "Article fetch failed.")
            }
        }
    }

    LaunchedEffect(Unit) {
        refresh()
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
                OutlinedTextField(
                    value = uiState.address,
                    onValueChange = { uiState = uiState.copy(address = it) },
                    label = { Text("Device address") },
                    singleLine = true,
                )
                Button(onClick = { connect() }) {
                    Text(text = if (uiState.isConnected) "Reconnect" else "Connect")
                }
                Button(onClick = { refresh() }) {
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
                                Button(onClick = { deleteDeviceBook(book) }) {
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
                                    onValueChange = { uiState = uiState.copy(settingsWpmDraft = it) },
                                    label = { Text("Words per minute") },
                                    singleLine = true,
                                )
                                OutlinedTextField(
                                    value = uiState.settingsBrightnessDraft,
                                    onValueChange = { uiState = uiState.copy(settingsBrightnessDraft = it) },
                                    label = { Text("Brightness index") },
                                    singleLine = true,
                                )
                                Button(onClick = { saveDeviceSettings() }) {
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
                            onValueChange = { uiState = uiState.copy(wifiSsidDraft = it) },
                            label = { Text("Wi-Fi SSID") },
                            singleLine = true,
                        )
                    }
                    item {
                        OutlinedTextField(
                            value = uiState.wifiPasswordDraft,
                            onValueChange = { uiState = uiState.copy(wifiPasswordDraft = it) },
                            label = { Text("Wi-Fi password") },
                            singleLine = true,
                        )
                    }
                    item {
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Button(onClick = { saveWifiSettings() }) {
                                Text(text = "Save Wi-Fi")
                            }
                            Button(onClick = { clearWifiSettings() }) {
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
                            onValueChange = { uiState = uiState.copy(draftTitle = it) },
                            label = { Text("Article title") },
                            singleLine = true,
                        )
                    }
                    item {
                        OutlinedTextField(
                            value = uiState.draftSourceUrl,
                            onValueChange = { uiState = uiState.copy(draftSourceUrl = it) },
                            label = { Text("Source URL") },
                            singleLine = true,
                        )
                    }
                    item {
                        OutlinedTextField(
                            value = uiState.draftBody,
                            onValueChange = { uiState = uiState.copy(draftBody = it) },
                            label = { Text("Article text") },
                            minLines = 3,
                        )
                    }
                    item {
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Button(onClick = { saveTextDraft() }) {
                                Text(text = if (uiState.editingDraftId == null) "Save text" else "Update text")
                            }
                            Button(onClick = { saveLinkDraft() }) {
                                Text(text = if (uiState.editingDraftId == null) "Save link" else "Update link")
                            }
                            if (uiState.editingDraftId != null) {
                                Button(onClick = { uiState = clearDraftEditor(status = "Edit cancelled.") }) {
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
                            val suffix = if (sharedApp.facade.needsArticleFetch(draft)) " (needs fetch)" else ""
                            Column {
                                Text(text = draft.title + suffix)
                                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                    Button(onClick = { editDraft(draft) }) {
                                        Text(text = "Edit")
                                    }
                                    Button(onClick = { deleteDraft(draft) }) {
                                        Text(text = "Delete")
                                    }
                                }
                            }
                        }
                        item {
                            Button(onClick = { syncSavedArticles() }) {
                                Text(text = "Sync saved articles")
                            }
                        }
                        item {
                            Button(onClick = { fetchMissingArticles() }) {
                                Text(text = "Fetch saved links")
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
                            onValueChange = { uiState = uiState.copy(rssFeedDraft = it) },
                            label = { Text("RSS feed URL") },
                            singleLine = true,
                        )
                    }

                    item {
                        Button(onClick = { addRssFeed() }) {
                            Text(text = "Add RSS feed")
                        }
                    }

                    item {
                        Button(onClick = { syncRssFeeds() }) {
                            Text(text = "Sync RSS feeds")
                        }
                    }
                }
            }
        }
    }
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
