package com.rsvpnano.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.KeyboardArrowRight
import androidx.compose.material.icons.automirrored.outlined.MenuBook
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material.icons.outlined.CloudUpload
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material.icons.outlined.Brightness6
import androidx.compose.material.icons.outlined.Language
import androidx.compose.material.icons.outlined.Info
import androidx.compose.material.icons.outlined.Palette
import androidx.compose.material.icons.outlined.Sync
import androidx.compose.material.icons.outlined.SystemUpdate
import androidx.compose.material.icons.outlined.TextFields
import androidx.compose.material.icons.outlined.Timer
import androidx.compose.material.icons.outlined.UploadFile
import androidx.compose.material.icons.outlined.WarningAmber
import androidx.compose.material.icons.outlined.Wifi
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationRail
import androidx.compose.material3.NavigationRailItem
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.VerticalDivider
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.rsvpnano.models.NanoLocales
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoSettingsSchema
import com.rsvpnano.app.releaseSource

internal const val SETTINGS_INDEX_HELP = "Choose a section to configure your reader, its display, languages, or fonts."

internal enum class SettingsDestination(
    val label: String,
    val icon: ImageVector,
    val help: String,
) {
    Device("Reader & network", Icons.Outlined.Wifi, "Connect to your reader, configure its Wi-Fi, and choose its update source."),
    Reading("Reading", Icons.AutoMirrored.Outlined.MenuBook, "Set reading speed, pacing, pauses, footer information, and controls."),
    Typography("Typography", Icons.Outlined.TextFields, "Adjust reader text size, tracking, focus highlight, and guide placement."),
    FocusTimers("Focus timers", Icons.Outlined.Timer, "Create up to six focus and break routines for your reader."),
    Display("Display", Icons.Outlined.Brightness6, "Choose brightness, standby behavior, and screensaver settings."),
    Themes("Themes", Icons.Outlined.Palette, "Choose the active theme or install themes from the configured repository."),
    Locales("Languages", Icons.Outlined.Language, "Choose the interface language or install a locale pack. Reader language support comes from fonts."),
    Fonts("Fonts", Icons.Outlined.CloudUpload, "Choose the default reading typeface and install fonts for the scripts used by your books."),
    About("About", Icons.Outlined.Info, "Project links and creator credit for the RSVP Nano companion."),
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
internal fun SettingsScreen(
    uiState: CompanionUiState,
    presenter: CompanionPresenter,
    onFirmwareNotificationsChange: (Boolean) -> Unit,
    hasPermissions: Boolean,
    onGrantPermissions: () -> Unit,
    onUploadTheme: () -> Unit,
    onUploadFont: () -> Unit,
    onUploadLocalePack: () -> Unit,
    destination: SettingsDestination?,
    onDestinationSelected: (SettingsDestination) -> Unit,
) {
    val selected = destination ?: SettingsDestination.Device
    val refreshingResources = when (selected) {
        SettingsDestination.Device -> setOf(CompanionResource.Settings, CompanionResource.Wifi)
        SettingsDestination.Reading,
        SettingsDestination.Display,
        -> setOf(CompanionResource.Settings)
        SettingsDestination.Typography -> setOf(CompanionResource.Settings, CompanionResource.Fonts)
        SettingsDestination.FocusTimers -> setOf(CompanionResource.FocusTimers)
        SettingsDestination.Themes -> setOf(CompanionResource.Themes)
        SettingsDestination.Locales -> setOf(CompanionResource.Locales)
        SettingsDestination.Fonts -> setOf(CompanionResource.Fonts)
        SettingsDestination.About -> emptySet()
    }
    val content: @Composable (Modifier) -> Unit = { modifier ->
        SettingsContent(
            destination = selected,
            uiState = uiState,
            presenter = presenter,
            onFirmwareNotificationsChange = onFirmwareNotificationsChange,
            hasPermissions = hasPermissions,
            onGrantPermissions = onGrantPermissions,
            onUploadTheme = onUploadTheme,
            onUploadFont = onUploadFont,
            onUploadLocalePack = onUploadLocalePack,
            modifier = modifier,
        )
    }

    PullRefreshBox(
        isRefreshing = uiState.loadingResources.any(refreshingResources::contains),
        onRefresh = {
            when (selected) {
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
                    presenter.refreshThemes()
                    if (uiState.settings != null) presenter.refreshThemeCatalog()
                }
                SettingsDestination.Locales -> {
                    presenter.refreshLocales()
                    if (uiState.settings != null) presenter.refreshLocaleCatalog()
                }
                SettingsDestination.Fonts -> {
                    presenter.refreshFonts()
                    if (uiState.settings != null) presenter.refreshFontCatalog()
                }
                SettingsDestination.About -> Unit
            }
        },
    ) {
        BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
            if (maxWidth >= 720.dp) {
                Row(modifier = Modifier.fillMaxSize()) {
                    SettingsRail(
                        selected = selected,
                        onSelected = onDestinationSelected,
                    )
                    VerticalDivider()
                    content(Modifier.weight(1f))
                }
            } else {
                if (destination == null) {
                    SettingsIndex(
                        uiState = uiState,
                        onSelected = onDestinationSelected,
                    )
                } else {
                    content(Modifier.fillMaxSize())
                }
            }
        }
    }
}

@Composable
private fun SettingsRail(
    selected: SettingsDestination,
    onSelected: (SettingsDestination) -> Unit,
) {
    NavigationRail(modifier = Modifier.width(184.dp)) {
        SettingsDestination.entries.filterNot { it == SettingsDestination.About }.forEach { option ->
            NavigationRailItem(
                selected = selected == option,
                onClick = { onSelected(option) },
                icon = { Icon(imageVector = option.icon, contentDescription = null) },
                label = { Text(text = option.label) },
            )
        }
        HorizontalDivider(modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp))
        NavigationRailItem(
            selected = selected == SettingsDestination.About,
            onClick = { onSelected(SettingsDestination.About) },
            icon = { Icon(Icons.Outlined.Info, contentDescription = null) },
            label = { Text("About") },
        )
    }
}

@Composable
private fun SettingsContent(
    destination: SettingsDestination,
    uiState: CompanionUiState,
    presenter: CompanionPresenter,
    onFirmwareNotificationsChange: (Boolean) -> Unit,
    hasPermissions: Boolean,
    onGrantPermissions: () -> Unit,
    onUploadTheme: () -> Unit,
    onUploadFont: () -> Unit,
    onUploadLocalePack: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(modifier = modifier.fillMaxSize(), contentAlignment = Alignment.TopCenter) {
        when (destination) {
            SettingsDestination.Device -> DeviceSettings(
                uiState = uiState,
                onWifiSsidChange = presenter::setWifiSsidDraft,
                onWifiPasswordChange = presenter::setWifiPasswordDraft,
                onSaveWifi = presenter::saveWifiSettings,
                onClearWifi = presenter::clearWifiSettings,
                onForgetRememberedNano = presenter::forgetRememberedNano,
                onRepairStorage = presenter::repairStorage,
                onUpdateSettings = presenter::updateSettings,
                onFirmwareNotificationsChange = onFirmwareNotificationsChange,
                hasPermissions = hasPermissions,
                onGrantPermissions = onGrantPermissions,
            )

            SettingsDestination.Reading -> ReadingSettings(
                settings = uiState.settings,
                isConnected = uiState.isConnected,
                onUpdateSettings = presenter::updateSettings,
            )

            SettingsDestination.Display -> DisplaySettings(
                uiState = uiState,
                onUpdateSettings = presenter::updateSettings,
            )

            SettingsDestination.Themes -> ThemeSettings(
                uiState = uiState,
                onSelectTheme = presenter::selectTheme,
                onRefreshThemeCatalog = presenter::refreshThemeCatalog,
                onInstallOnlineTheme = presenter::installOnlineTheme,
                onUploadTheme = onUploadTheme,
                onRemoveTheme = presenter::removeTheme,
            )

            SettingsDestination.Typography -> TypographySettings(
                uiState = uiState,
                onUpdateSettings = presenter::updateSettings,
            )

            SettingsDestination.FocusTimers -> FocusTimersSettings(
                uiState = uiState,
                onSave = presenter::saveFocusTimers,
            )

            SettingsDestination.Locales -> LocaleSettings(
                uiState = uiState,
                onSelectLocale = presenter::selectLocale,
                onUploadLocalePack = onUploadLocalePack,
                onRemoveLocalePack = presenter::removeLocalePack,
                onRefreshLocaleCatalog = presenter::refreshLocaleCatalog,
                onInstallOnlineLocale = presenter::installOnlineLocalePack,
            )

            SettingsDestination.Fonts -> FontSettings(
                uiState = uiState,
                onSelectFont = presenter::selectFont,
                onRefreshFontCatalog = presenter::refreshFontCatalog,
                onInstallOnlineFont = presenter::installOnlineFont,
                onUploadFont = onUploadFont,
                onRemoveFont = presenter::removeFont,
            )

            SettingsDestination.About -> AboutPage()
        }
    }
}

@Composable
internal fun SettingsPage(
    content: @Composable () -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .widthIn(max = 760.dp)
            .verticalScroll(rememberScrollState())
            .padding(start = 20.dp, top = 8.dp, end = 20.dp, bottom = 24.dp),
    ) {
        content()
    }
}

@Composable
private fun DeviceSettings(
    uiState: CompanionUiState,
    onWifiSsidChange: (String) -> Unit,
    onWifiPasswordChange: (String) -> Unit,
    onSaveWifi: () -> Unit,
    onClearWifi: () -> Unit,
    onForgetRememberedNano: () -> Unit,
    onRepairStorage: () -> Unit,
    onUpdateSettings: ((NanoSettings) -> NanoSettings) -> Unit,
    onFirmwareNotificationsChange: (Boolean) -> Unit,
    hasPermissions: Boolean,
    onGrantPermissions: () -> Unit,
) {
    SettingsPage {
        SettingsSection(
            title = "Reader",
            subtitle = "Connection details for your RSVP Nano reader.",
        ) {
            if (!hasPermissions) {
                SettingsStatusRow(
                    icon = Icons.Outlined.WarningAmber,
                    title = "Wi-Fi permission needed",
                    body = "Allow nearby devices so the app can find your reader.",
                    action = {
                        TextButton(onClick = onGrantPermissions) {
                            Text(text = "Grant")
                        }
                    },
                )
            }

            val remembered = uiState.rememberedNano
            SettingsStatusRow(
                icon = if (remembered != null) Icons.Outlined.CheckCircle else Icons.Outlined.Wifi,
                title = if (remembered != null) "Remembered Nano" else "No Nano remembered",
                body = remembered?.ssid ?: if (uiState.isConnected) {
                    "Remember this reader to connect directly later."
                } else {
                    "Connect to a reader to remember it."
                },
                action = remembered?.let {
                    {
                        TextButton(onClick = onForgetRememberedNano) {
                            Text(text = "Forget")
                        }
                    }
                },
            )
        }

        if (uiState.settings != null && uiState.isConnected) {
            SettingsSection(
                title = "Internet Wi-Fi",
                subtitle = "Used by the reader for RSS feeds and updates.",
            ) {
                if (CompanionResource.Wifi !in uiState.loadedResources) {
                    if (CompanionResource.Wifi in uiState.loadingResources) {
                        LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                    } else {
                        Text(
                            "Network settings could not be loaded.",
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                } else {
                    val wifiStatus = uiState.wifiSettings?.ssid.orEmpty()
                        .takeIf(String::isNotBlank)
                        ?.let { "Saved network: $it" }
                        ?: "No saved network"
                    SettingsStatusRow(
                        icon = Icons.Outlined.Wifi,
                        title = "Nano internet network",
                        body = wifiStatus,
                    )
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
                        visualTransformation = PasswordVisualTransformation(),
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = onSaveWifi) {
                            Text(text = "Save Wi-Fi")
                        }
                        FilledTonalButton(
                            onClick = onClearWifi,
                            colors = ButtonDefaults.filledTonalButtonColors(
                                containerColor = MaterialTheme.colorScheme.errorContainer,
                                contentColor = MaterialTheme.colorScheme.onErrorContainer,
                            ),
                        ) {
                            Text(text = "Forget network")
                        }
                    }
                }
            }
        }

        SettingsSection(
            title = "Firmware updates",
            subtitle = "Choose where the reader gets firmware updates.",
        ) {
            val settings = uiState.settings
            if (settings != null && uiState.isConnected) {
                var ownerDraft by remember(settings.updates.repositoryOwner) {
                    mutableStateOf(settings.updates.repositoryOwner)
                }
                var tagDraft by remember(settings.updates.releaseTag) {
                    mutableStateOf(settings.updates.releaseTag)
                }
                SettingsStatusRow(
                    icon = Icons.Outlined.SystemUpdate,
                    title = "Installed firmware",
                    body = buildString {
                        append(uiState.firmwareVersion.ifBlank { "Version unavailable" })
                        if (uiState.otaAsset.isNotBlank()) append("\nOTA image: ${uiState.otaAsset}")
                    },
                )
                OutlinedTextField(
                    value = ownerDraft,
                    onValueChange = { ownerDraft = it.take(63) },
                    label = { Text("GitHub owner") },
                    supportingText = { Text("You can also use owner/repository.") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = tagDraft,
                    onValueChange = { tagDraft = it.take(63) },
                    label = { Text("Release tag") },
                    supportingText = { Text("Leave blank to follow the latest release.") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Button(
                    onClick = {
                        onUpdateSettings {
                            it.withUpdateOwner(ownerDraft.trim())
                                .withUpdateTag(tagDraft.trim())
                        }
                    },
                    enabled = ownerDraft.isNotBlank() &&
                        (ownerDraft.trim() != settings.updates.repositoryOwner ||
                            tagDraft.trim() != settings.updates.releaseTag),
                ) {
                    Text("Save release source")
                }
                SwitchRow(
                    label = "Check at startup",
                    description = "Check for updates when the reader starts with Wi-Fi.",
                    checked = settings.updates.checkOnStartup,
                    onCheckedChange = { enabled ->
                        onUpdateSettings { it.withUpdateChecksOnStartup(enabled) }
                    },
                )
            } else {
                SettingsStatusRow(
                    icon = Icons.Outlined.SystemUpdate,
                    title = "Connect to manage updates",
                    body = "Update settings come from the reader.",
                )
            }
            SwitchRow(
                label = "Update notifications",
                description = "Check daily and notify you when a new release is available.",
                checked = uiState.firmwareNotificationsEnabled,
                onCheckedChange = onFirmwareNotificationsChange,
            )
        }

        SettingsSection(
            title = "SD card",
            subtitle = "Check the card and organize supported files into the current layout.",
        ) {
            val report = uiState.storageRepair
            SettingsStatusRow(
                icon = Icons.Outlined.Sync,
                title = when {
                    uiState.isRepairingStorage -> "Repairing SD card"
                    report == null -> "Storage repair"
                    report.healthy -> "SD card ready"
                    else -> "SD card needs attention"
                },
                body = when {
                    report == null -> "Checks folders, interrupted files, books, themes, fonts, language packs, and settings."
                    report.issues.isNotEmpty() -> report.issues.joinToString("\n")
                    else -> "Checked ${report.checked} files, moved ${report.moved}, and cleaned ${report.removed}."
                },
            )
            if (uiState.isRepairingStorage) {
                LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
            } else {
                Button(onClick = onRepairStorage, enabled = uiState.isConnected) {
                    Text("Repair SD card")
                }
            }
        }
    }
}

@Composable
private fun ReadingSettings(
    settings: NanoSettings?,
    isConnected: Boolean,
    onUpdateSettings: ((NanoSettings) -> NanoSettings) -> Unit,
) {
    SettingsPage {
        if (settings == null) {
            UnavailableSettings(isConnected)
            return@SettingsPage
        }

        SettingsSection(title = "Speed and pauses") {
            SliderRow(
                label = "Words per minute",
                valueLabel = { value -> "${NanoSettingsSchema.snapWpm(value.toInt())} WPM" },
                value = settings.reading.wpm.toFloat(),
                valueRange = NanoSettingsSchema.WPM_MIN.toFloat()..NanoSettingsSchema.WPM_MAX.toFloat(),
                steps = 0,
                snapValue = { value -> NanoSettingsSchema.snapWpm(value.toInt()).toFloat() },
                onValueChangeFinished = { value -> onUpdateSettings { it.withWpm(value.toInt()) } },
                prominentHeader = true,
            )
            SegmentedChoiceRow(
                label = "Pause behavior",
                description = "Choose whether pause waits for the sentence to finish.",
                selected = settings.reading.pauseMode,
                options = listOf(
                    NanoSettingsSchema.PAUSE_MODE_SENTENCE_END to "Sentence end",
                    NanoSettingsSchema.PAUSE_MODE_INSTANT to "Immediately",
                ),
                onSelected = { mode -> onUpdateSettings { it.withPauseMode(mode) } },
            )
        }

        SettingsSection(title = "Timing adjustments") {
            PacingSlider(
                label = "Long words",
                value = settings.reading.pacing.longWordDelayMs,
                onChanged = { value -> onUpdateSettings { it.withPacingLongWordMs(value) } },
            )
            PacingSlider(
                label = "Complex words",
                value = settings.reading.pacing.complexWordDelayMs,
                onChanged = { value -> onUpdateSettings { it.withPacingComplexWordMs(value) } },
            )
            PacingSlider(
                label = "Punctuation",
                value = settings.reading.pacing.punctuationDelayMs,
                onChanged = { value -> onUpdateSettings { it.withPacingPunctuationMs(value) } },
            )
            TextButton(
                onClick = { onUpdateSettings(NanoSettings::withDefaultPacing) },
                enabled = settings.reading.pacing != NanoSettings.Pacing(),
            ) {
                Icon(imageVector = Icons.Outlined.Sync, contentDescription = null)
                Text(text = "Reset to defaults")
            }
        }
    }
}

@Composable
private fun PacingSlider(
    label: String,
    value: Int,
    onChanged: (Int) -> Unit,
) {
    SliderRow(
        label = label,
        valueLabel = { sliderValue -> "${NanoSettingsSchema.snapPacingMs(sliderValue.toInt())} ms" },
        value = value.toFloat(),
        valueRange = NanoSettingsSchema.PACING_MS_MIN.toFloat()..NanoSettingsSchema.PACING_MS_MAX.toFloat(),
        steps = 11,
        snapValue = { sliderValue -> NanoSettingsSchema.snapPacingMs(sliderValue.toInt()).toFloat() },
        onValueChangeFinished = { sliderValue -> onChanged(sliderValue.toInt()) },
        prominentHeader = true,
    )
}

@Composable
private fun DisplaySettings(
    uiState: CompanionUiState,
    onUpdateSettings: ((NanoSettings) -> NanoSettings) -> Unit,
) {
    SettingsPage {
        val settings = uiState.settings
        if (settings == null) {
            UnavailableSettings(uiState.isConnected)
            return@SettingsPage
        }

        SettingsSection(title = "Display") {
            SliderRow(
                label = "Brightness",
                description = "Applied immediately.",
                valueLabel = { value -> "${value.toInt()}%" },
                value = settings.`interface`.brightnessPercent.toFloat(),
                valueRange = NanoSettingsSchema.BRIGHTNESS_MIN.toFloat()..NanoSettingsSchema.BRIGHTNESS_MAX.toFloat(),
                steps = 18,
                onValueChangeFinished = { value -> onUpdateSettings { it.withBrightnessPercent(value.toInt()) } },
                prominentHeader = true,
            )
            SegmentedChoiceRow(
                label = "Reader hand",
                description = "Places the previous-sentence tap area on this side.",
                selected = if (settings.reading.leftHanded) {
                    NanoSettingsSchema.HANDEDNESS_LEFT
                } else {
                    NanoSettingsSchema.HANDEDNESS_RIGHT
                },
                options = listOf(
                    NanoSettingsSchema.HANDEDNESS_LEFT to "Left",
                    NanoSettingsSchema.HANDEDNESS_RIGHT to "Right",
                ),
                onSelected = { hand -> onUpdateSettings { it.withHandedness(hand) } },
            )
        }

        SettingsSection(
            title = "Reader status",
            subtitle = "Choose what the footer and reading screen show.",
        ) {
            ChoiceChipRow(
                label = "Footer label",
                selected = settings.reading.footerMetric,
                options = listOf(
                    NanoSettingsSchema.FOOTER_PERCENTAGE to "Book percent",
                    NanoSettingsSchema.FOOTER_CHAPTER_TIME to "Chapter time",
                    NanoSettingsSchema.FOOTER_BOOK_TIME to "Book time",
                ),
                onSelected = { metric -> onUpdateSettings { it.withFooterMetric(metric) } },
            )
            ChoiceChipRow(
                label = "Battery label",
                selected = settings.reading.batteryLabel,
                options = listOf(
                    NanoSettingsSchema.BATTERY_PERCENTAGE to "Percent",
                    NanoSettingsSchema.BATTERY_TIME_REMAINING to "Time left",
                    NanoSettingsSchema.BATTERY_VOLTAGE to "Voltage",
                ),
                onSelected = { label -> onUpdateSettings { it.withBatteryLabel(label) } },
            )
            SwitchRow(
                label = "Battery icon",
                checked = settings.reading.batteryIconVisible,
                onCheckedChange = { checked -> onUpdateSettings { it.withBatteryIconVisible(checked) } },
            )
            SwitchRow(
                label = "Battery while reading",
                checked = settings.reading.batteryVisibleWhileReading,
                onCheckedChange = { checked -> onUpdateSettings { it.withReadingBattery(checked) } },
            )
            SwitchRow(
                label = "Chapter while reading",
                checked = settings.reading.chapterVisibleWhileReading,
                onCheckedChange = { checked -> onUpdateSettings { it.withReadingChapter(checked) } },
            )
            SwitchRow(
                label = "Book progress while reading",
                checked = settings.reading.progressVisibleWhileReading,
                onCheckedChange = { checked -> onUpdateSettings { it.withReadingProgress(checked) } },
            )
        }

        SettingsSection(
            title = "Idle screen",
            subtitle = "What happens when the Nano is left alone.",
        ) {
            DropdownRow(
                label = "Screensaver",
                selected = settings.`interface`.screensaver,
                options = listOf(
                    NanoSettingsSchema.SCREENSAVER_LIFE to "Life",
                    NanoSettingsSchema.SCREENSAVER_MAZE to "Maze",
                    NanoSettingsSchema.SCREENSAVER_VORONOI to "Voronoi",
                    NanoSettingsSchema.SCREENSAVER_REACTION to "Reaction",
                    NanoSettingsSchema.SCREENSAVER_SCREEN_OFF to "Screen off",
                ),
                onSelected = { mode ->
                    onUpdateSettings { it.withScreensaver(mode) }
                },
            )
            DropdownRow(
                label = "Standby timer",
                selected = settings.`interface`.standbyTimerIndex.toString(),
                options = listOf(
                    NanoSettingsSchema.STANDBY_TIMER_NEVER.toString() to "Never",
                    NanoSettingsSchema.STANDBY_TIMER_1_MIN.toString() to "1 minute",
                    NanoSettingsSchema.STANDBY_TIMER_5_MIN.toString() to "5 minutes",
                    NanoSettingsSchema.STANDBY_TIMER_15_MIN.toString() to "15 minutes",
                    NanoSettingsSchema.STANDBY_TIMER_30_MIN.toString() to "30 minutes",
                ),
                onSelected = { index ->
                    onUpdateSettings {
                        it.withStandbyTimerIndex(index.toIntOrNull() ?: NanoSettingsSchema.STANDBY_TIMER_NEVER)
                    }
                },
            )
        }

    }
}

@Composable
private fun ThemeSettings(
    uiState: CompanionUiState,
    onSelectTheme: (String) -> Unit,
    onRefreshThemeCatalog: () -> Unit,
    onInstallOnlineTheme: (String) -> Unit,
    onUploadTheme: () -> Unit,
    onRemoveTheme: (String) -> Unit,
) {
    var pendingRemoval by remember { mutableStateOf<Pair<String, String>?>(null) }
    SettingsPage {
        val settings = uiState.settings
        if (settings == null) {
            UnavailableSettings(uiState.isConnected)
            return@SettingsPage
        }
        val install = uiState.catalogInstall
        val installedLoaded = CompanionResource.Themes in uiState.loadedResources
        val controlsEnabled = install == null && installedLoaded
        val installedById = uiState.availableThemes.associateBy { it.id }
        val catalogById = uiState.themeCatalog.associateBy { it.id }
        val themeIds = buildList {
            add(NanoSettingsSchema.THEME_DEFAULT)
            addAll(uiState.themeCatalog.map { it.id })
            addAll(uiState.availableThemes.map { it.id })
        }.distinct()
        SettingsSection(
            title = "Themes",
            subtitle = "Choose an installed theme as the default, or install one from ${catalogSource(settings)}.",
            action = {
                IconButton(onClick = onRefreshThemeCatalog, enabled = controlsEnabled) {
                    Icon(Icons.Outlined.Sync, contentDescription = "Refresh theme catalog")
                }
            },
        ) {
            if (!installedLoaded) {
                if (CompanionResource.Themes in uiState.loadingResources) {
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                } else {
                    Text("Installed themes could not be loaded.", color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
            if (installedLoaded) themeIds.forEach { id ->
                val installedTheme = installedById[id]
                val catalogTheme = catalogById[id]
                val installed = id == NanoSettingsSchema.THEME_DEFAULT || installedTheme != null
                val name = installedTheme?.name ?: catalogTheme?.name ?: "Default"
                CatalogAssetRow(
                    title = name,
                    subtitle = when {
                        id == NanoSettingsSchema.THEME_DEFAULT -> "Built in"
                        installed -> "Installed"
                        else -> "Available to install"
                    },
                    selected = installed && settings.`interface`.selectedThemeId == id,
                    enabled = controlsEnabled,
                    install = install?.takeIf { it.asset == CatalogAsset.Theme && it.id == id },
                    onSelect = if (installed) {
                        { onSelectTheme(id) }
                    } else null,
                    onDelete = if (installed && id != NanoSettingsSchema.THEME_DEFAULT) {
                        { pendingRemoval = id to name }
                    } else null,
                    onInstall = if (!installed && catalogTheme != null) {
                        { onInstallOnlineTheme(id) }
                    } else null,
                )
            }
            if (uiState.themeCatalog.isEmpty() && uiState.themeCatalogUrl.isNotBlank()) {
                Text(
                    "The online theme catalog is unavailable.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            UploadRow("Install local theme file", controlsEnabled, onUploadTheme)
        }
    }
    pendingRemoval?.let { (id, name) ->
        ConfirmCatalogRemoval(
            type = "theme",
            name = name,
            onDismiss = { pendingRemoval = null },
            onConfirm = {
                pendingRemoval = null
                onRemoveTheme(id)
            },
        )
    }
}

@Composable
private fun TypographySettings(
    uiState: CompanionUiState,
    onUpdateSettings: ((NanoSettings) -> NanoSettings) -> Unit,
) {
    BoxWithConstraints(
        modifier = Modifier
            .fillMaxSize()
            .widthIn(max = 760.dp)
            .padding(start = 20.dp, top = 8.dp, end = 20.dp, bottom = 16.dp),
    ) {
        val settings = uiState.settings
        if (settings == null) {
            UnavailableSettings(uiState.isConnected)
            return@BoxWithConstraints
        }

        var previewTypography by remember(settings.reading.typography) {
            mutableStateOf(settings.reading.typography)
        }
        var previewPhantomWords by remember(settings.reading.phantomWords) {
            mutableStateOf(settings.reading.phantomWords)
        }
        val previewHeight = if (maxHeight < 560.dp) 128.dp else 152.dp

        Column(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            TypographySizeSelector(
                selected = previewTypography.fontSizeIndex.toString(),
                onSelected = { value ->
                    val index = value.toInt()
                    previewTypography = previewTypography.copy(fontSizeIndex = index)
                    onUpdateSettings { it.withFontSizeIndex(index) }
                },
            )

            TypographyPreview(
                typography = previewTypography,
                phantomWords = previewPhantomWords,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(previewHeight),
            )

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                TypographyToggle(
                    label = "Focus highlight",
                    description = "Color the focus letter",
                    checked = previewTypography.focusHighlight,
                    onCheckedChange = { checked ->
                        previewTypography = previewTypography.copy(focusHighlight = checked)
                        onUpdateSettings { it.withFocusHighlight(checked) }
                    },
                    modifier = Modifier.weight(1f),
                )
                TypographyToggle(
                    label = "Phantom words",
                    description = "Show nearby words",
                    checked = previewPhantomWords,
                    onCheckedChange = { checked ->
                        previewPhantomWords = checked
                        onUpdateSettings { it.withPhantomWords(checked) }
                    },
                    modifier = Modifier.weight(1f),
                )
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(16.dp),
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    TypographySlider(
                        label = "Tracking",
                        valueLabel = { it.toInt().toString() },
                        value = previewTypography.tracking.toFloat(),
                        valueRange = NanoSettingsSchema.TRACKING_MIN.toFloat()..NanoSettingsSchema.TRACKING_MAX.toFloat(),
                        steps = 4,
                        onValueChange = { value ->
                            previewTypography = previewTypography.copy(tracking = value.toInt())
                        },
                        onValueChangeFinished = { value -> onUpdateSettings { it.withTracking(value.toInt()) } },
                    )
                    TypographySlider(
                        label = "Anchor",
                        valueLabel = { "${it.toInt()}%" },
                        value = previewTypography.anchor.toFloat(),
                        valueRange = NanoSettingsSchema.ANCHOR_PERCENT_MIN.toFloat()..NanoSettingsSchema.ANCHOR_PERCENT_MAX.toFloat(),
                        steps = 9,
                        onValueChange = { value ->
                            previewTypography = previewTypography.copy(anchor = value.toInt())
                        },
                        onValueChangeFinished = { value -> onUpdateSettings { it.withAnchorPercent(value.toInt()) } },
                    )
                }
                Column(modifier = Modifier.weight(1f)) {
                    TypographySlider(
                        label = "Guide width",
                        valueLabel = { "${it.toInt()} px" },
                        value = previewTypography.guideWidth.toFloat(),
                        valueRange = NanoSettingsSchema.GUIDE_WIDTH_MIN.toFloat()..NanoSettingsSchema.GUIDE_WIDTH_MAX.toFloat(),
                        steps = 8,
                        snapValue = { NanoSettingsSchema.snapGuideWidth(it.toInt()).toFloat() },
                        onValueChange = { value ->
                            previewTypography = previewTypography.copy(guideWidth = value.toInt())
                        },
                        onValueChangeFinished = { value -> onUpdateSettings { it.withGuideWidth(value.toInt()) } },
                    )
                    TypographySlider(
                        label = "Guide gap",
                        valueLabel = { "${it.toInt()} px" },
                        value = previewTypography.guideGap.toFloat(),
                        valueRange = NanoSettingsSchema.GUIDE_GAP_MIN.toFloat()..NanoSettingsSchema.GUIDE_GAP_MAX.toFloat(),
                        steps = 5,
                        onValueChange = { value ->
                            previewTypography = previewTypography.copy(guideGap = value.toInt())
                        },
                        onValueChangeFinished = { value -> onUpdateSettings { it.withGuideGap(value.toInt()) } },
                    )
                }
            }
        }
    }
}

@Composable
private fun TypographySizeSelector(
    selected: String,
    onSelected: (String) -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Text("Reading size", style = MaterialTheme.typography.labelLarge)
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            listOf("0" to "Large", "1" to "Medium", "2" to "Small", "3" to "Compact").forEach { (value, label) ->
                val isSelected = value == selected
                Surface(
                    modifier = Modifier
                        .weight(1f)
                        .selectable(
                            selected = isSelected,
                            role = Role.RadioButton,
                            onClick = { onSelected(value) },
                        ),
                    shape = MaterialTheme.shapes.small,
                    color = if (isSelected) MaterialTheme.colorScheme.primaryContainer
                    else MaterialTheme.colorScheme.surfaceContainer,
                    contentColor = if (isSelected) MaterialTheme.colorScheme.onPrimaryContainer
                    else MaterialTheme.colorScheme.onSurfaceVariant,
                ) {
                    Text(
                        text = label,
                        modifier = Modifier.padding(horizontal = 4.dp, vertical = 9.dp),
                        style = MaterialTheme.typography.labelMedium,
                        textAlign = TextAlign.Center,
                        maxLines = 1,
                    )
                }
            }
        }
    }
}

@Composable
private fun TypographyToggle(
    label: String,
    description: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
) {
    Surface(
        modifier = modifier.toggleable(
            value = checked,
            role = Role.Switch,
            onValueChange = onCheckedChange,
        ),
        shape = MaterialTheme.shapes.medium,
        color = MaterialTheme.colorScheme.surfaceContainer,
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(label, style = MaterialTheme.typography.labelLarge, maxLines = 2)
                Text(
                    description,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1,
                )
            }
            Switch(checked = checked, onCheckedChange = null)
        }
    }
}

@Composable
private fun TypographySlider(
    label: String,
    valueLabel: (Float) -> String,
    value: Float,
    valueRange: ClosedFloatingPointRange<Float>,
    steps: Int,
    onValueChange: (Float) -> Unit,
    onValueChangeFinished: (Float) -> Unit,
    snapValue: (Float) -> Float = { it },
) {
    var sliderValue by remember(value) { mutableStateOf(snapValue(value)) }
    Column {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(label, modifier = Modifier.weight(1f), style = MaterialTheme.typography.labelLarge, maxLines = 1)
            Text(
                valueLabel(sliderValue),
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        Slider(
            value = sliderValue.coerceIn(valueRange.start, valueRange.endInclusive),
            onValueChange = {
                sliderValue = snapValue(it).coerceIn(valueRange.start, valueRange.endInclusive)
                onValueChange(sliderValue)
            },
            valueRange = valueRange,
            steps = steps,
            onValueChangeFinished = { onValueChangeFinished(sliderValue) },
        )
    }
}

@Composable
fun TypographyPreview(
    typography: NanoSettings.Typography,
    phantomWords: Boolean,
    modifier: Modifier = Modifier,
) {
    val foreground = MaterialTheme.colorScheme.onSurface
    val muted = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.5f)
    val accent = MaterialTheme.colorScheme.primary
    val textMeasurer = rememberTextMeasurer()
    val wordStyle = TextStyle(
        color = foreground,
        fontSize = when (typography.fontSizeIndex) {
            0 -> 42.sp
            1 -> 36.sp
            2 -> 30.sp
            else -> 24.sp
        },
        letterSpacing = typography.tracking.sp,
    )
    val phantomStyle = wordStyle.copy(
        color = muted,
        fontSize = (wordStyle.fontSize.value * 0.58f).sp,
    )
    val focusWord = buildAnnotatedString {
        append("Re")
        withStyle(SpanStyle(color = if (typography.focusHighlight) accent else foreground)) {
            append("a")
        }
        append("ding")
    }
    val wordLayout = remember(focusWord, wordStyle) {
        textMeasurer.measure(text = focusWord, style = wordStyle, maxLines = 1, softWrap = false)
    }
    val beforeLayout = remember(phantomStyle) {
        textMeasurer.measure(text = "one word", style = phantomStyle, maxLines = 1, softWrap = false)
    }
    val afterLayout = remember(phantomStyle) {
        textMeasurer.measure(text = "at a time", style = phantomStyle, maxLines = 1, softWrap = false)
    }

    Surface(
        modifier = modifier,
        shape = MaterialTheme.shapes.large,
        color = MaterialTheme.colorScheme.surfaceContainer,
    ) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            val anchor = size.width * typography.anchor / 100f
            val wordStart = anchor - wordLayout.getBoundingBox(2).center.x
            val wordTop = (size.height - wordLayout.size.height) / 2f
            val guideWidth = typography.guideWidth.dp.toPx()
            val guideGap = typography.guideGap.dp.toPx()
            val guideTop = wordTop - 10.dp.toPx()
            val guideBottom = wordTop + wordLayout.size.height + 10.dp.toPx()
            val guideColor = foreground.copy(alpha = 0.38f)
            val markerColor = if (typography.focusHighlight) accent else guideColor
            val stroke = 1.dp.toPx()

            listOf(guideTop, guideBottom).forEach { y ->
                drawLine(
                    color = guideColor,
                    start = Offset(anchor - guideWidth, y),
                    end = Offset(anchor - guideGap, y),
                    strokeWidth = stroke,
                )
                drawLine(
                    color = guideColor,
                    start = Offset(anchor + guideGap, y),
                    end = Offset(anchor + guideWidth, y),
                    strokeWidth = stroke,
                )
            }
            drawLine(
                color = markerColor,
                start = Offset(anchor, guideTop),
                end = Offset(anchor, guideTop + 7.dp.toPx()),
                strokeWidth = stroke,
            )
            drawLine(
                color = markerColor,
                start = Offset(anchor, guideBottom - 7.dp.toPx()),
                end = Offset(anchor, guideBottom),
                strokeWidth = stroke,
            )

            drawText(wordLayout, topLeft = Offset(wordStart, wordTop))
            if (phantomWords) {
                val phantomGap = 22.dp.toPx()
                drawText(
                    beforeLayout,
                    topLeft = Offset(
                        wordStart - phantomGap - beforeLayout.size.width,
                        (size.height - beforeLayout.size.height) / 2f,
                    ),
                )
                drawText(
                    afterLayout,
                    topLeft = Offset(
                        wordStart + wordLayout.size.width + phantomGap,
                        (size.height - afterLayout.size.height) / 2f,
                    ),
                )
            }
        }
    }
}

@Composable
private fun LocaleSettings(
    uiState: CompanionUiState,
    onSelectLocale: (String) -> Unit,
    onUploadLocalePack: () -> Unit,
    onRemoveLocalePack: (String) -> Unit,
    onRefreshLocaleCatalog: () -> Unit,
    onInstallOnlineLocale: (String) -> Unit,
) {
    var pendingRemoval by remember { mutableStateOf<Pair<String, String>?>(null) }
    SettingsPage {
        val settings = uiState.settings
        if (settings == null) {
            UnavailableSettings(uiState.isConnected)
            return@SettingsPage
        }
        val install = uiState.catalogInstall
        val installedLoaded = CompanionResource.Locales in uiState.loadedResources
        val controlsEnabled = install == null && installedLoaded
        val installedById = uiState.availableLocales.associateBy { it.id }
        val catalogById = uiState.localeCatalog.associateBy { it.id }
        val localeIds = buildList {
            add(NanoLocales.DEFAULT)
            addAll(uiState.localeCatalog.filterNot { it.locale == NanoLocales.DEFAULT }.map { it.id })
            addAll(uiState.availableLocales.filterNot { it.locale == NanoLocales.DEFAULT }.map { it.id })
        }.distinct()
        SettingsSection(
            title = "Interface languages",
            subtitle = "Choose an installed UI language, or install one from ${catalogSource(settings)}. Reader language support comes from fonts.",
            action = {
                IconButton(onClick = onRefreshLocaleCatalog, enabled = controlsEnabled) {
                    Icon(Icons.Outlined.Sync, contentDescription = "Refresh locale catalog")
                }
            },
        ) {
            if (!installedLoaded) {
                if (CompanionResource.Locales in uiState.loadingResources) {
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                } else {
                    Text("Installed languages could not be loaded.", color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
            if (installedLoaded) localeIds.forEach { id ->
                if (id == NanoLocales.DEFAULT) {
                    CatalogAssetRow(
                        title = "English",
                        subtitle = "Built in${INLINE_DIVIDER}Left-to-right",
                        selected = settings.`interface`.locale == NanoLocales.DEFAULT,
                        enabled = controlsEnabled,
                        onSelect = { onSelectLocale(NanoLocales.DEFAULT) },
                    )
                    return@forEach
                }

                val installedPack = installedById[id]
                val catalogPack = catalogById[id]
                val installed = installedPack != null
                val name = installedPack?.name ?: catalogPack?.name ?: id
                CatalogAssetRow(
                    title = name,
                    subtitle = catalogPack?.let {
                        localeDetails(it.englishName, it.direction, it.translationStatus, it.version)
                    }.orEmpty(),
                    selected = installed && settings.`interface`.locale == installedPack.locale,
                    enabled = controlsEnabled,
                    install = install?.takeIf { it.asset == CatalogAsset.Locale && it.id == id },
                    onSelect = installedPack?.let { pack ->
                        { onSelectLocale(pack.locale) }
                    },
                    onDelete = if (installed) {
                        { pendingRemoval = id to name }
                    } else null,
                    onInstall = if (!installed && catalogPack != null) {
                        { onInstallOnlineLocale(id) }
                    } else null,
                )
            }
            if (uiState.localeCatalog.isEmpty() && uiState.localeCatalogUrl.isNotBlank()) {
                Text(
                    "The online locale catalog is unavailable.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            UploadRow("Install locale pack from ZIP", controlsEnabled, onUploadLocalePack)
        }
    }
    pendingRemoval?.let { (id, name) ->
        ConfirmCatalogRemoval(
            type = "interface language",
            name = name,
            onDismiss = { pendingRemoval = null },
            onConfirm = {
                pendingRemoval = null
                onRemoveLocalePack(id)
            },
        )
    }
}

@Composable
private fun SettingsIndex(
    uiState: CompanionUiState,
    onSelected: (SettingsDestination) -> Unit,
) {
    val settings = uiState.settings
    val summaries = mapOf(
        SettingsDestination.Device to if (uiState.isConnected) {
            buildList {
                add("Connected to ${uiState.currentNano?.ssid ?: "Nano"}")
                if (CompanionResource.Wifi in uiState.loadedResources) {
                    add(uiState.wifiSettings?.ssid?.takeIf(String::isNotBlank) ?: "No internet Wi-Fi")
                }
            }.joinToString(INLINE_DIVIDER)
        } else {
            "Not connected"
        },
        SettingsDestination.Reading to settings?.let {
            listOf("${it.reading.wpm} WPM", "${it.reading.pauseMode.replace('-', ' ')} pause")
                .joinToString(INLINE_DIVIDER)
        }.orEmpty(),
        SettingsDestination.Typography to settings?.let {
            val font = uiState.availableFonts.firstOrNull { font -> font.id == it.reading.typography.fontId }?.name
                ?: it.reading.typography.fontId
            val size = listOf("Large", "Medium", "Small", "Compact")
                .getOrElse(it.reading.typography.fontSizeIndex) { "Default" }
            listOf(font, size, "Tracking ${it.reading.typography.tracking}").joinToString(INLINE_DIVIDER)
        }.orEmpty(),
        SettingsDestination.FocusTimers to if (CompanionResource.FocusTimers in uiState.loadedResources) {
            val count = uiState.focusTimers.timers.size
            "$count ${if (count == 1) "routine" else "routines"}"
        } else if (!uiState.isConnected) {
            "Not connected"
        } else {
            ""
        },
        SettingsDestination.Display to settings?.let {
            listOf("${it.`interface`.brightnessPercent}% brightness", it.`interface`.screensaver)
                .joinToString(INLINE_DIVIDER)
        }.orEmpty(),
        SettingsDestination.Themes to settings?.let {
            val theme = uiState.availableThemes.firstOrNull { installed -> installed.id == it.`interface`.selectedThemeId }?.name
                ?: it.`interface`.selectedThemeId
            buildList {
                add(theme)
                if (CompanionResource.Themes in uiState.loadedResources) {
                    add("${uiState.availableThemes.size} installed")
                }
            }.joinToString(INLINE_DIVIDER)
        }.orEmpty(),
        SettingsDestination.Locales to settings?.let {
            val locale = uiState.availableLocales.firstOrNull { pack -> pack.locale == it.`interface`.locale }?.name
                ?: if (it.`interface`.locale == NanoLocales.DEFAULT) "English" else it.`interface`.locale
            buildList {
                add(locale)
                if (CompanionResource.Locales in uiState.loadedResources) {
                    add("${uiState.availableLocales.size} locale packs installed")
                }
            }.joinToString(INLINE_DIVIDER)
        }.orEmpty(),
        SettingsDestination.Fonts to settings?.let {
            val font = uiState.availableFonts.firstOrNull { installed -> installed.id == it.reading.typography.fontId }?.name
                ?: it.reading.typography.fontId
            buildList {
                add(font)
                if (CompanionResource.Fonts in uiState.loadedResources) {
                    add("${uiState.availableFonts.count { installed -> !installed.builtIn }} reader fonts installed")
                }
            }.joinToString(INLINE_DIVIDER)
        }.orEmpty(),
    )
    Column(
        modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()),
    ) {
        SettingsDestination.entries.filterNot { it == SettingsDestination.About }.forEach { option ->
            ListItem(
                headlineContent = { Text(option.label) },
                supportingContent = summaries[option]?.takeIf(String::isNotBlank)?.let { summary ->
                    { Text(summary, maxLines = 1, overflow = TextOverflow.Ellipsis) }
                },
                leadingContent = { Icon(option.icon, contentDescription = null) },
                trailingContent = { Icon(Icons.AutoMirrored.Outlined.KeyboardArrowRight, contentDescription = null) },
                modifier = Modifier.clickable { onSelected(option) },
            )
            HorizontalDivider(modifier = Modifier.padding(start = 56.dp))
        }
        ListItem(
            headlineContent = { Text("About") },
            supportingContent = { Text("RSVP Nano companion") },
            leadingContent = { Icon(Icons.Outlined.Info, contentDescription = null) },
            trailingContent = { Icon(Icons.AutoMirrored.Outlined.KeyboardArrowRight, contentDescription = null) },
            modifier = Modifier.clickable { onSelected(SettingsDestination.About) },
        )
    }
}

@Composable
private fun FontSettings(
    uiState: CompanionUiState,
    onSelectFont: (String) -> Unit,
    onRefreshFontCatalog: () -> Unit,
    onInstallOnlineFont: (String) -> Unit,
    onUploadFont: () -> Unit,
    onRemoveFont: (String) -> Unit,
) {
    var pendingRemoval by remember { mutableStateOf<Pair<String, String>?>(null) }
    SettingsPage {
        val settings = uiState.settings
        if (settings == null) {
            UnavailableSettings(uiState.isConnected)
            return@SettingsPage
        }
        val install = uiState.catalogInstall
        val installedLoaded = CompanionResource.Fonts in uiState.loadedResources
        val controlsEnabled = install == null && installedLoaded
        val installedById = uiState.availableFonts.associateBy { it.id }
        val catalogById = uiState.fontCatalog.associateBy { it.id }
        val fontIds = buildList {
            addAll(uiState.availableFonts.filter { it.builtIn }.map { it.id })
            addAll(uiState.fontCatalog.map { it.id })
            addAll(uiState.availableFonts.map { it.id })
        }.distinct()
        SettingsSection(
            title = "Reader fonts",
            subtitle = "Choose an installed default, or install one from ${catalogSource(settings)}. Book language choices still show compatible fonts only.",
            action = {
                IconButton(onClick = onRefreshFontCatalog, enabled = controlsEnabled) {
                    Icon(imageVector = Icons.Outlined.Sync, contentDescription = "Refresh font catalog")
                }
            },
        ) {
            if (!installedLoaded) {
                if (CompanionResource.Fonts in uiState.loadingResources) {
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                } else {
                    Text("Installed fonts could not be loaded.", color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            } else if (fontIds.isEmpty()) {
                Text("No fonts reported by the reader.", color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            if (installedLoaded) fontIds.forEach { id ->
                val installedFont = installedById[id]
                val catalogFont = catalogById[id]
                val installed = installedFont != null
                val name = installedFont?.name ?: catalogFont?.name ?: id
                CatalogAssetRow(
                    title = name,
                    subtitle = installedFont?.let {
                        fontDetails(it.scripts, it.builtIn, shaping = false)
                    } ?: catalogFont?.let {
                        fontDetails(it.scripts, builtIn = false, shaping = it.shaping)
                    }.orEmpty(),
                    selected = installed && settings.reading.typography.fontId == id,
                    enabled = controlsEnabled,
                    install = install?.takeIf { it.asset == CatalogAsset.Font && it.id == id },
                    onSelect = if (installed) {
                        { onSelectFont(id) }
                    } else null,
                    onDelete = if (installedFont != null && !installedFont.builtIn) {
                        { pendingRemoval = id to name }
                    } else null,
                    onInstall = if (!installed && catalogFont != null) {
                        { onInstallOnlineFont(id) }
                    } else null,
                )
            }
            if (uiState.fontCatalog.isEmpty() && uiState.fontCatalogUrl.isNotBlank()) {
                Text(
                    "The online font catalog is unavailable.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            UploadRow("Install local .rfont4 file", controlsEnabled, onUploadFont)
        }
    }
    pendingRemoval?.let { (id, name) ->
        ConfirmCatalogRemoval(
            type = "font",
            name = name,
            onDismiss = { pendingRemoval = null },
            onConfirm = {
                pendingRemoval = null
                onRemoveFont(id)
            },
        )
    }
}

@Composable
private fun CatalogAssetRow(
    title: String,
    subtitle: String,
    selected: Boolean = false,
    enabled: Boolean = true,
    install: CatalogInstall? = null,
    onSelect: (() -> Unit)? = null,
    onDelete: (() -> Unit)? = null,
    onInstall: (() -> Unit)? = null,
) {
    Surface(
        color = if (selected) MaterialTheme.colorScheme.secondaryContainer else MaterialTheme.colorScheme.surface,
    ) {
        Column {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .then(
                        if (onSelect == null) Modifier else Modifier.selectable(
                            selected = selected,
                            enabled = enabled,
                            role = Role.RadioButton,
                            onClick = onSelect,
                        ),
                    )
                    .padding(horizontal = 8.dp, vertical = 10.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                if (onSelect != null) {
                    RadioButton(selected = selected, enabled = enabled, onClick = null)
                }
                Column(modifier = Modifier.weight(1f)) {
                    Text(title, style = MaterialTheme.typography.titleSmall)
                    if (selected) {
                        Text(
                            "Default",
                            style = MaterialTheme.typography.labelMedium,
                            color = MaterialTheme.colorScheme.primary,
                        )
                    }
                    Text(
                        subtitle,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                when {
                    onDelete != null -> DestructiveIconButton(
                        contentDescription = "Remove $title",
                        onClick = onDelete,
                        enabled = enabled,
                    )
                    onInstall != null -> TextButton(onClick = onInstall, enabled = enabled) { Text("Install") }
                }
            }
            install?.let { job ->
                val percent = job.progress?.coerceIn(0f, 1f)?.let { (it * 100).toInt() }
                Text(
                    text = job.stage.label + (percent?.let { "$INLINE_DIVIDER$it%" } ?: ""),
                    modifier = Modifier.padding(horizontal = 8.dp),
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.primary,
                )
                if (job.progress == null) {
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                } else {
                    LinearProgressIndicator(
                        progress = { job.progress.coerceIn(0f, 1f) },
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
            }
        }
    }
}

@Composable
private fun ConfirmCatalogRemoval(
    type: String,
    name: String,
    onDismiss: () -> Unit,
    onConfirm: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        icon = {
            Icon(
                imageVector = Icons.Outlined.Delete,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.error,
            )
        },
        title = { Text("Remove $type?") },
        text = { Text("Remove $name from the reader? This cannot be undone.") },
        confirmButton = {
            TextButton(
                onClick = onConfirm,
                colors = ButtonDefaults.textButtonColors(contentColor = MaterialTheme.colorScheme.error),
            ) {
                Text("Remove")
            }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

@Composable
private fun UploadRow(label: String, enabled: Boolean, onClick: () -> Unit) {
    ListItem(
        headlineContent = { Text(label) },
        leadingContent = { Icon(Icons.Outlined.UploadFile, contentDescription = null) },
        trailingContent = { Icon(Icons.AutoMirrored.Outlined.KeyboardArrowRight, contentDescription = null) },
        modifier = Modifier.clickable(enabled = enabled, onClick = onClick),
    )
}

private fun catalogSource(settings: NanoSettings): String {
    val source = releaseSource(settings.updates.repositoryOwner, settings.updates.releaseTag)
        ?: return "configured repository"
    return "${source.owner}/${source.repository}@${source.tag.ifBlank { "main" }}"
}

fun localeDetails(
    englishName: String,
    direction: String,
    status: String,
    version: String = "",
): String = listOf(
    englishName.replace("ChineseSimplified", "Simplified Chinese").replace("ChineseTraditional", "Traditional Chinese"),
    if (direction.equals("rtl", ignoreCase = true)) "Right-to-left" else "Left-to-right",
    status.replaceFirstChar(Char::uppercase),
    version.takeIf(String::isNotBlank)?.let { "v$it" }.orEmpty(),
).filter(String::isNotBlank).joinToString(INLINE_DIVIDER)

fun fontDetails(scripts: List<String>, builtIn: Boolean, shaping: Boolean): String =
    (scripts.map(::scriptName) +
        listOfNotNull("Built in".takeIf { builtIn }, "Shaping".takeIf { shaping }))
        .joinToString(INLINE_DIVIDER)
        .ifBlank { "Reader font" }

private fun scriptName(tag: String): String = when (tag) {
    "Latn" -> "Latin"
    "Cyrl" -> "Cyrillic"
    "Grek" -> "Greek"
    "Hebr" -> "Hebrew"
    "Arab" -> "Arabic"
    "Hani" -> "Han"
    "Hira" -> "Hiragana"
    "Kana" -> "Katakana"
    "Hang" -> "Hangul"
    "Zmth" -> "Math"
    else -> tag
}

@Composable
private fun UnavailableSettings(isConnected: Boolean) {
    Text(
        text = if (isConnected) "Settings are not loaded yet." else "Connect to the Nano to edit reader settings.",
        modifier = Modifier.padding(vertical = 24.dp),
        style = MaterialTheme.typography.bodyLarge,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
}
