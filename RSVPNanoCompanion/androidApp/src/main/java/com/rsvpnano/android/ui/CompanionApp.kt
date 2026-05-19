package com.rsvpnano.android.ui

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.OpenableColumns
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.Article
import androidx.compose.material.icons.automirrored.outlined.LibraryBooks
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CenterAlignedTopAppBar
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.viewmodel.compose.viewModel
import com.rsvpnano.app.RsvpSharedApp
import com.rsvpnano.converters.RsvpSupportedFileTypes
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.PendingUpload
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

private enum class CompanionTab(val label: String, val icon: ImageVector) {
    Library("Library", Icons.AutoMirrored.Outlined.LibraryBooks),
    Articles("Articles", Icons.AutoMirrored.Outlined.Article),
    Settings("Settings", Icons.Outlined.Settings),
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

@OptIn(ExperimentalMaterial3Api::class)
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

    val colorScheme = if (isSystemInDarkTheme()) CompanionDarkColors else CompanionLightColors
    MaterialTheme(colorScheme = colorScheme) {
        Surface(
            modifier = Modifier.fillMaxSize(),
            color = MaterialTheme.colorScheme.background,
        ) {
            var selectedTab by remember { mutableStateOf(CompanionTab.Library) }
            Scaffold(
                topBar = {
                    CenterAlignedTopAppBar(
                        title = {
                            Column {
                                Text(text = "RSVP Nano", style = MaterialTheme.typography.titleMedium)
                                Text(
                                    text = if (uiState.isConnected) "Connected" else "Disconnected",
                                    style = MaterialTheme.typography.labelSmall,
                                )
                            }
                        },
                        actions = {
                            if (uiState.isConnected) {
                                TextButton(onClick = viewModel::connect) {
                                    Text(text = "Reconnect")
                                }
                            }
                        },
                        colors = TopAppBarDefaults.centerAlignedTopAppBarColors(
                            containerColor = MaterialTheme.colorScheme.surface,
                        ),
                    )
                },
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
                            onOpenWifiSettings = context::openWifiSettings,
                            onConnectDefault = viewModel::connectDefault,
                            onShowAddressEntry = viewModel::showAddressEntry,
                            onAddressChange = viewModel::setAddress,
                            onConnectCustom = viewModel::connect,
                            onRefresh = viewModel::refresh,
                            onUpload = {
                                filePicker.launch(
                                    arrayOf(
                                        "application/epub+zip",
                                        "text/*",
                                        "text/html",
                                        "application/octet-stream",
                                    ),
                                )
                            },
                            onDeleteBook = viewModel::deleteDeviceBook,
                        )
                        CompanionTab.Articles -> ArticlesTab(
                            uiState = uiState,
                            needsArticleFetch = viewModel::needsArticleFetch,
                            onTitleChange = viewModel::setDraftTitle,
                            onSourceChange = viewModel::setDraftSourceUrl,
                            onBodyChange = viewModel::setDraftBody,
                            onSaveText = viewModel::saveTextDraft,
                            onSaveLink = viewModel::saveLinkDraft,
                            onCancelEdit = viewModel::cancelDraftEdit,
                            onEditDraft = viewModel::editDraft,
                            onDeleteDraft = viewModel::deleteDraft,
                            onSyncArticles = viewModel::syncSavedArticles,
                            onFeedChange = viewModel::setRssFeedDraft,
                            onAddFeed = viewModel::addRssFeed,
                            onSyncFeeds = viewModel::syncRssFeeds,
                        )
                        CompanionTab.Settings -> SettingsTab(
                            uiState = uiState,
                            onUpdateSettings = viewModel::updateSettings,
                            onWifiSsidChange = viewModel::setWifiSsidDraft,
                            onWifiPasswordChange = viewModel::setWifiPasswordDraft,
                            onSaveWifi = viewModel::saveWifiSettings,
                            onClearWifi = viewModel::clearWifiSettings,
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun LibraryTab(
    uiState: CompanionUiState,
    onOpenWifiSettings: () -> Unit,
    onConnectDefault: () -> Unit,
    onShowAddressEntry: () -> Unit,
    onAddressChange: (String) -> Unit,
    onConnectCustom: () -> Unit,
    onRefresh: () -> Unit,
    onUpload: () -> Unit,
    onDeleteBook: (NanoBook) -> Unit,
) {
    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(bottom = 18.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        if (!uiState.isConnected) {
            item {
                ConnectionPanel(
                    uiState = uiState,
                    onOpenWifiSettings = onOpenWifiSettings,
                    onConnectDefault = onConnectDefault,
                    onShowAddressEntry = onShowAddressEntry,
                    onAddressChange = onAddressChange,
                    onConnectCustom = onConnectCustom,
                )
            }
        }

        item {
            SectionCard(title = "Device Library") {
                Text(text = uiState.status, style = MaterialTheme.typography.bodySmall)
                HorizontalDivider()
                Text(
                    text = if (uiState.isConnected) {
                        "Books and synced articles currently on the reader."
                    } else {
                        "Connect to the reader to load its library."
                    },
                    style = MaterialTheme.typography.bodyMedium,
                )
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = onRefresh) {
                        Text(text = "Refresh")
                    }
                    Button(onClick = onUpload, enabled = uiState.isConnected) {
                        Text(text = "Upload")
                    }
                }
            }
        }

        if (uiState.books.isEmpty()) {
            item {
                EmptyCard(text = if (uiState.isConnected) "No books on device." else "Library unavailable while disconnected.")
            }
        } else {
            items(uiState.books) { book ->
                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(
                        modifier = Modifier.padding(14.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Text(text = book.displayTitle, style = MaterialTheme.typography.titleSmall)
                        TextButton(onClick = { onDeleteBook(book) }) {
                            Text(text = "Delete")
                        }
                    }
                }
            }
        }
    }
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
    onFeedChange: (String) -> Unit,
    onAddFeed: () -> Unit,
    onSyncFeeds: () -> Unit,
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
                Card(modifier = Modifier.fillMaxWidth()) {
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
                            TextButton(onClick = { onDeleteDraft(draft) }) {
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

        item {
            SectionCard(title = "RSS Feeds") {
                if (uiState.rssFeeds.isEmpty()) {
                    Text(text = "No feeds saved.")
                } else {
                    uiState.rssFeeds.forEach { feed ->
                        Text(text = feed, style = MaterialTheme.typography.bodySmall)
                    }
                }
                OutlinedTextField(
                    value = uiState.rssFeedDraft,
                    onValueChange = onFeedChange,
                    label = { Text("Feed URL") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = onAddFeed) {
                        Text(text = "Add")
                    }
                    TextButton(onClick = onSyncFeeds, enabled = uiState.isConnected) {
                        Text(text = "Sync")
                    }
                }
            }
        }
    }
}

@Composable
private fun SettingsTab(
    uiState: CompanionUiState,
    onUpdateSettings: ((NanoSettings) -> NanoSettings) -> Unit,
    onWifiSsidChange: (String) -> Unit,
    onWifiPasswordChange: (String) -> Unit,
    onSaveWifi: () -> Unit,
    onClearWifi: () -> Unit,
) {
    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(bottom = 18.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        val settings = uiState.settings
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
                    StepperRow(
                        label = "Base speed",
                        valueLabel = "${settings.reading.wpm} WPM",
                        onDecrease = { onUpdateSettings { it.withWpm((it.reading.wpm - 25).coerceIn(100, 1000)) } },
                        onIncrease = { onUpdateSettings { it.withWpm((it.reading.wpm + 25).coerceIn(100, 1000)) } },
                    )
                    StepperRow(
                        label = "Long words",
                        valueLabel = "${settings.reading.pacing.longWordMs} ms",
                        onDecrease = {
                            onUpdateSettings { it.withPacingLongWordMs((it.reading.pacing.longWordMs - 50).coerceIn(0, 600)) }
                        },
                        onIncrease = {
                            onUpdateSettings { it.withPacingLongWordMs((it.reading.pacing.longWordMs + 50).coerceIn(0, 600)) }
                        },
                    )
                    StepperRow(
                        label = "Complexity",
                        valueLabel = "${settings.reading.pacing.complexWordMs} ms",
                        onDecrease = {
                            onUpdateSettings { it.withPacingComplexWordMs((it.reading.pacing.complexWordMs - 50).coerceIn(0, 600)) }
                        },
                        onIncrease = {
                            onUpdateSettings { it.withPacingComplexWordMs((it.reading.pacing.complexWordMs + 50).coerceIn(0, 600)) }
                        },
                    )
                    StepperRow(
                        label = "Punctuation",
                        valueLabel = "${settings.reading.pacing.punctuationMs} ms",
                        onDecrease = {
                            onUpdateSettings { it.withPacingPunctuationMs((it.reading.pacing.punctuationMs - 50).coerceIn(0, 600)) }
                        },
                        onIncrease = {
                            onUpdateSettings { it.withPacingPunctuationMs((it.reading.pacing.punctuationMs + 50).coerceIn(0, 600)) }
                        },
                    )
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
                    StepperRow(
                        label = "Brightness",
                        valueLabel = "${settings.display.brightnessIndex + 1} / 5",
                        onDecrease = {
                            onUpdateSettings { it.withBrightnessIndex((it.display.brightnessIndex - 1).coerceIn(0, 4)) }
                        },
                        onIncrease = {
                            onUpdateSettings { it.withBrightnessIndex((it.display.brightnessIndex + 1).coerceIn(0, 4)) }
                        },
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
                        options = listOf("percent" to "Percent", "time_remaining" to "Time left"),
                        onSelected = { label -> onUpdateSettings { it.withBatteryLabel(label) } },
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
                    StepperRow(
                        label = "Font size",
                        valueLabel = "${settings.display.fontSizeIndex + 1} / 3",
                        onDecrease = {
                            onUpdateSettings { it.withFontSizeIndex((it.display.fontSizeIndex - 1).coerceIn(0, 2)) }
                        },
                        onIncrease = {
                            onUpdateSettings { it.withFontSizeIndex((it.display.fontSizeIndex + 1).coerceIn(0, 2)) }
                        },
                    )
                    StepperRow(
                        label = "Tracking",
                        valueLabel = "${settings.typography.tracking}",
                        onDecrease = { onUpdateSettings { it.withTracking((it.typography.tracking - 1).coerceIn(-2, 3)) } },
                        onIncrease = { onUpdateSettings { it.withTracking((it.typography.tracking + 1).coerceIn(-2, 3)) } },
                    )
                    StepperRow(
                        label = "Anchor",
                        valueLabel = "${settings.typography.anchorPercent}%",
                        onDecrease = {
                            onUpdateSettings { it.withAnchorPercent((it.typography.anchorPercent - 1).coerceIn(30, 40)) }
                        },
                        onIncrease = {
                            onUpdateSettings { it.withAnchorPercent((it.typography.anchorPercent + 1).coerceIn(30, 40)) }
                        },
                    )
                    StepperRow(
                        label = "Guide width",
                        valueLabel = "${settings.typography.guideWidth}",
                        onDecrease = {
                            onUpdateSettings { it.withGuideWidth((it.typography.guideWidth - 2).coerceIn(12, 30)) }
                        },
                        onIncrease = {
                            onUpdateSettings { it.withGuideWidth((it.typography.guideWidth + 2).coerceIn(12, 30)) }
                        },
                    )
                    StepperRow(
                        label = "Guide gap",
                        valueLabel = "${settings.typography.guideGap}",
                        onDecrease = { onUpdateSettings { it.withGuideGap((it.typography.guideGap - 1).coerceIn(2, 8)) } },
                        onIncrease = { onUpdateSettings { it.withGuideGap((it.typography.guideGap + 1).coerceIn(2, 8)) } },
                    )
                }
            }
        }

        item {
            SectionCard(title = "Wi-Fi") {
                val wifiStatus = uiState.wifiSettings?.let { wifi ->
                    if (wifi.configured) "Configured for ${wifi.ssid}" else "Not configured"
                } ?: if (uiState.isConnected) {
                    "Wi-Fi settings unavailable."
                } else {
                    "Connect to load Wi-Fi settings."
                }
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
                    TextButton(onClick = onClearWifi) {
                        Text(text = "Forget")
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
    Card(modifier = Modifier.fillMaxWidth()) {
        Text(
            text = text,
            modifier = Modifier.padding(16.dp),
            style = MaterialTheme.typography.bodyMedium,
        )
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

@Composable
private fun ConnectionPanel(
    uiState: CompanionUiState,
    onOpenWifiSettings: () -> Unit,
    onConnectDefault: () -> Unit,
    onShowAddressEntry: () -> Unit,
    onAddressChange: (String) -> Unit,
    onConnectCustom: () -> Unit,
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Text(text = "Connect", style = MaterialTheme.typography.titleMedium)
            Text(
                text = "Open Companion sync on the reader, join its Wi-Fi, then return here.",
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

            TextButton(onClick = onShowAddressEntry) {
                Text(text = "Reader not found?")
            }

            if (uiState.showAddressEntry) {
                Text(
                    text = "Enter the address shown on the reader if it is not 192.168.4.1.",
                    style = MaterialTheme.typography.bodySmall,
                )
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    TextButton(onClick = onOpenWifiSettings) {
                        Text(text = "Wi-Fi Settings")
                    }
                }

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
