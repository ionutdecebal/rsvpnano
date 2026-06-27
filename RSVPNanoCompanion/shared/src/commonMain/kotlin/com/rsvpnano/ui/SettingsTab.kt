package com.rsvpnano.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.clickable
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
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.MenuBook
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material.icons.outlined.CloudUpload
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material.icons.outlined.Edit
import androidx.compose.material.icons.outlined.Newspaper
import androidx.compose.material.icons.outlined.RssFeed
import androidx.compose.material.icons.outlined.Sync
import androidx.compose.material.icons.outlined.UploadFile
import androidx.compose.material.icons.outlined.WarningAmber
import androidx.compose.material.icons.outlined.Wifi
import androidx.compose.material3.AssistChip
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.SnackbarDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.dp
import com.rsvpnano.app.CompanionNotice
import com.rsvpnano.app.CompanionNotice.Attention
import com.rsvpnano.app.CompanionNotice.Error
import com.rsvpnano.app.CompanionNotice.Success
import com.rsvpnano.converters.RsvpSupportedFileTypes
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoSettingsSchema
import com.rsvpnano.models.PendingUpload

@Composable
fun SettingsTab(
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
        Box(modifier = Modifier.fillMaxSize()) {
            LazyColumn(
                modifier = Modifier.fillMaxSize(),
                contentPadding = PaddingValues(bottom = if (uiState.showSettingsSaveStatus) 64.dp else 18.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                val settings = uiState.settings
                item {
                    SectionCard(
                        title = "Reader Connection",
                        subtitle = "Connect to the Nano, manage the saved reader, and set the fallback address.",
                    ) {
                        if (!hasPermissions) {
                            SettingsStatusRow(
                                icon = Icons.Outlined.WarningAmber,
                                title = "Wi-Fi permission needed",
                                body = "Grant access to let the app search for nearby Nano networks.",
                                action = {
                                    TextButton(onClick = onGrantPermissions) {
                                        Text(text = "Grant")
                                    }
                                },
                            )
                        }

                        if (uiState.rememberedNano != null) {
                            val remembered = uiState.rememberedNano
                            SettingsStatusRow(
                                icon = Icons.Outlined.CheckCircle,
                                title = "Remembered Nano",
                                body = remembered.ssid,
                                action = {
                                    TextButton(onClick = onForgetRememberedNano) {
                                        Text(text = "Forget")
                                    }
                                },
                            )
                        } else {
                            SettingsStatusRow(
                                icon = Icons.Outlined.Wifi,
                                title = "No Nano remembered",
                                body = if (uiState.isConnected) {
                                    "Connected, but Android did not expose a network name to save."
                                } else {
                                    "Connect once, then save this Nano for faster reconnects."
                                },
                            )
                        }
                        HorizontalDivider()
                        OutlinedTextField(
                            value = uiState.address,
                            onValueChange = onAddressChange,
                            label = { Text("Reader address") },
                            supportingText = { Text("Used when checking the Nano API.") },
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                        )
                        FilledTonalButton(onClick = onConnectDefault) {
                            Icon(imageVector = Icons.Outlined.Wifi, contentDescription = null)
                            Text(text = "Reset to 192.168.4.1")
                        }
                    }
                }
            if (settings == null) {
                item {
                    EmptyCard(text = if (uiState.isConnected) "Settings are not loaded yet." else "Connect to the Nano to edit reader settings.")
                }
            } else {
                item {
                    SectionCard(
                        title = "Word Pacing",
                        subtitle = "Reading speed, pause timing, and RSVP behavior.",
                    ) {
                        ChoiceRow(
                            label = "Reading mode",
                            selected = settings.reading.readerMode,
                            options = listOf(
                                NanoSettingsSchema.READER_MODE_RSVP to "One word",
                                NanoSettingsSchema.READER_MODE_SCROLL to "Scroll",
                            ),
                            onSelected = { mode -> onUpdateSettings { it.withReaderMode(mode) } },
                        )
                        ChoiceRow(
                            label = "Pause behavior",
                            selected = settings.reading.pauseMode,
                            options = listOf(
                                NanoSettingsSchema.PAUSE_MODE_SENTENCE_END to "Sentence end",
                                NanoSettingsSchema.PAUSE_MODE_INSTANT to "Immediate",
                            ),
                            onSelected = { mode -> onUpdateSettings { it.withPauseMode(mode) } },
                        )
                        SwitchRow(
                            label = "Accurate time estimate",
                            description = "Uses more detailed pacing rules for remaining-time estimates.",
                            checked = settings.reading.accurateTimeEstimate,
                            onCheckedChange = { checked -> onUpdateSettings { it.withAccurateTimeEstimate(checked) } },
                        )
                        SliderRow(
                            label = "Base speed",
                            valueLabel = { value -> "${NanoSettingsSchema.snapWpm(value.toInt())} WPM" },
                            value = settings.reading.wpm.toFloat(),
                            valueRange = NanoSettingsSchema.WPM_MIN.toFloat()..NanoSettingsSchema.WPM_MAX.toFloat(),
                            steps = 0,
                            snapValue = { value -> NanoSettingsSchema.snapWpm(value.toInt()).toFloat() },
                            onValueChangeFinished = { value -> onUpdateSettings { it.withWpm(value.toInt()) } },
                        )
                        SliderRow(
                            label = "Long words",
                            valueLabel = { value -> "${NanoSettingsSchema.snapPacingMs(value.toInt())} ms" },
                            value = settings.reading.pacing.longWordMs.toFloat(),
                            valueRange = NanoSettingsSchema.PACING_MS_MIN.toFloat()..NanoSettingsSchema.PACING_MS_MAX.toFloat(),
                            steps = 11,
                            snapValue = { value -> NanoSettingsSchema.snapPacingMs(value.toInt()).toFloat() },
                            onValueChangeFinished = { value -> onUpdateSettings { it.withPacingLongWordMs(value.toInt()) } },
                        )
                        SliderRow(
                            label = "Complexity",
                            valueLabel = { value -> "${NanoSettingsSchema.snapPacingMs(value.toInt())} ms" },
                            value = settings.reading.pacing.complexWordMs.toFloat(),
                            valueRange = NanoSettingsSchema.PACING_MS_MIN.toFloat()..NanoSettingsSchema.PACING_MS_MAX.toFloat(),
                            steps = 11,
                            snapValue = { value -> NanoSettingsSchema.snapPacingMs(value.toInt()).toFloat() },
                            onValueChangeFinished = { value -> onUpdateSettings { it.withPacingComplexWordMs(value.toInt()) } },
                        )
                        SliderRow(
                            label = "Punctuation",
                            valueLabel = { value -> "${NanoSettingsSchema.snapPacingMs(value.toInt())} ms" },
                            value = settings.reading.pacing.punctuationMs.toFloat(),
                            valueRange = NanoSettingsSchema.PACING_MS_MIN.toFloat()..NanoSettingsSchema.PACING_MS_MAX.toFloat(),
                            steps = 11,
                            snapValue = { value -> NanoSettingsSchema.snapPacingMs(value.toInt()).toFloat() },
                            onValueChangeFinished = { value -> onUpdateSettings { it.withPacingPunctuationMs(value.toInt()) } },
                        )
                        TextButton(onClick = {
                            onUpdateSettings {
                                it.withPacingLongWordMs(0)
                                    .withPacingComplexWordMs(0)
                                    .withPacingPunctuationMs(0)
                            }
                        }) {
                            Icon(imageVector = Icons.Outlined.Sync, contentDescription = null)
                            Text(text = "Reset pacing")
                        }
                    }
                }

                item {
                    SectionCard(
                        title = "Display",
                        subtitle = "Screen mode, standby behavior, and reader status labels.",
                    ) {
                        ChoiceRow(
                            label = "Display mode",
                            selected = settings.appearanceMode,
                            options = listOf(
                                NanoSettingsSchema.APPEARANCE_LIGHT to "Light",
                                NanoSettingsSchema.APPEARANCE_DARK to "Dark",
                                NanoSettingsSchema.APPEARANCE_NIGHT to "Night",
                            ),
                            onSelected = { mode ->
                                onUpdateSettings { it.withAppearanceMode(mode) }
                            },
                        )
                        SliderRow(
                            label = "Brightness",
                            valueLabel = { value -> "${value.toInt() + 1} / 5" },
                            value = settings.display.brightnessIndex.toFloat(),
                            valueRange = NanoSettingsSchema.BRIGHTNESS_MIN.toFloat()..NanoSettingsSchema.BRIGHTNESS_MAX.toFloat(),
                            steps = 3,
                            onValueChangeFinished = { value -> onUpdateSettings { it.withBrightnessIndex(value.toInt()) } },
                        )
                        ChoiceRow(
                            label = "Reader hand",
                            selected = settings.display.handedness,
                            options = listOf(
                                NanoSettingsSchema.HANDEDNESS_LEFT to "Left",
                                NanoSettingsSchema.HANDEDNESS_RIGHT to "Right",
                            ),
                            onSelected = { hand -> onUpdateSettings { it.withHandedness(hand) } },
                        )
                        ChoiceRow(
                            label = "Reader controls",
                            selected = settings.display.readerControls,
                            options = listOf(
                                NanoSettingsSchema.READER_CONTROLS_STANDARD to "Standard",
                                NanoSettingsSchema.READER_CONTROLS_REWIND_TOP_RIGHT to
                                    "Rewind top-right",
                            ),
                            onSelected = { layout -> onUpdateSettings { it.withReaderControls(layout) } },
                        )
                        ChoiceRow(
                            label = "Footer label",
                            selected = settings.display.footerMetric,
                            options = listOf(
                                NanoSettingsSchema.FOOTER_PERCENTAGE to "Percent",
                                NanoSettingsSchema.FOOTER_CHAPTER_TIME to "Chapter time",
                                NanoSettingsSchema.FOOTER_BOOK_TIME to "Book time",
                            ),
                            onSelected = { metric -> onUpdateSettings { it.withFooterMetric(metric) } },
                        )
                        ChoiceRow(
                            label = "Battery label",
                            selected = settings.display.batteryLabel,
                            options = listOf(
                                NanoSettingsSchema.BATTERY_PERCENT to "Percent",
                                NanoSettingsSchema.BATTERY_TIME_REMAINING to "Time left",
                                NanoSettingsSchema.BATTERY_VOLTAGE to "Voltage",
                            ),
                            onSelected = { label -> onUpdateSettings { it.withBatteryLabel(label) } },
                        )
                        ChoiceRow(
                            label = "Screensaver",
                            selected = settings.display.screensaver.toString(),
                            options = listOf(
                                NanoSettingsSchema.SCREENSAVER_LIFE.toString() to "Life",
                                NanoSettingsSchema.SCREENSAVER_MAZE.toString() to "Maze",
                                NanoSettingsSchema.SCREENSAVER_VORONOI.toString() to "Voronoi",
                                NanoSettingsSchema.SCREENSAVER_SCREEN_OFF.toString() to "Screen off",
                            ),
                            onSelected = { mode ->
                                onUpdateSettings { it.withScreensaver(mode.toIntOrNull() ?: NanoSettingsSchema.SCREENSAVER_LIFE) }
                            },
                        )
                        ChoiceRow(
                            label = "Standby timer",
                            selected = settings.display.standbyTimerIndex.toString(),
                            options = listOf(
                                NanoSettingsSchema.STANDBY_TIMER_NEVER.toString() to "Never",
                                NanoSettingsSchema.STANDBY_TIMER_1_MIN.toString() to "1 min",
                                NanoSettingsSchema.STANDBY_TIMER_5_MIN.toString() to "5 min",
                                NanoSettingsSchema.STANDBY_TIMER_10_MIN.toString() to "10 min",
                                NanoSettingsSchema.STANDBY_TIMER_30_MIN.toString() to "30 min",
                            ),
                            onSelected = { index ->
                                onUpdateSettings { it.withStandbyTimerIndex(index.toIntOrNull() ?: NanoSettingsSchema.STANDBY_TIMER_NEVER) }
                            },
                        )
                        ChoiceRow(
                            label = "Language",
                            selected = settings.display.language.toString(),
                            options = listOf(
                                "0" to "English",
                                "1" to "Espanol",
                                "2" to "Francais",
                                "3" to "Deutsch",
                                "4" to "Romana",
                                "5" to "Polski",
                            ),
                            onSelected = { language ->
                                onUpdateSettings { it.withLanguage(language.toIntOrNull() ?: 0) }
                            },
                        )
                        SwitchRow(
                            label = "Battery while reading",
                            checked = settings.display.readingBattery,
                            onCheckedChange = { checked -> onUpdateSettings { it.withReadingBattery(checked) } },
                        )
                        SwitchRow(
                            label = "Chapter while reading",
                            checked = settings.display.readingChapter,
                            onCheckedChange = { checked -> onUpdateSettings { it.withReadingChapter(checked) } },
                        )
                        SwitchRow(
                            label = "Book percent while reading",
                            checked = settings.display.readingProgress,
                            onCheckedChange = { checked -> onUpdateSettings { it.withReadingProgress(checked) } },
                        )
                    }
                }

                item {
                    SectionCard(
                        title = "Typography",
                        subtitle = "Typeface, focus markers, and word placement.",
                    ) {
                        ChoiceRow(
                            label = "Typeface",
                            selected = settings.typography.typeface,
                            options = listOf(
                                NanoSettingsSchema.TYPEFACE_STANDARD to "Standard",
                                NanoSettingsSchema.TYPEFACE_ATKINSON to "Atkinson",
                                NanoSettingsSchema.TYPEFACE_OPEN_DYSLEXIC to "OpenDyslexic",
                            ),
                            onSelected = { typeface -> onUpdateSettings { it.withTypeface(typeface) } },
                        )
                        SwitchRow(
                            label = "Focus highlight",
                            description = "Highlights the current word's focus point.",
                            checked = settings.typography.focusHighlight,
                            onCheckedChange = { checked -> onUpdateSettings { it.withFocusHighlight(checked) } },
                        )
                        SwitchRow(
                            label = "Phantom words",
                            description = "Shows adjacent words as faint context while reading.",
                            checked = settings.display.phantomWords,
                            onCheckedChange = { checked -> onUpdateSettings { it.withPhantomWords(checked) } },
                        )
                        SliderRow(
                            label = "Font size",
                            valueLabel = { value -> "${value.toInt() + 1} / 3" },
                            value = settings.display.fontSizeIndex.toFloat(),
                            valueRange = NanoSettingsSchema.FONT_SIZE_MIN.toFloat()..NanoSettingsSchema.FONT_SIZE_MAX.toFloat(),
                            steps = 1,
                            onValueChangeFinished = { value -> onUpdateSettings { it.withFontSizeIndex(value.toInt()) } },
                        )
                        SliderRow(
                            label = "Tracking",
                            valueLabel = { value -> "${value.toInt()}" },
                            value = settings.typography.tracking.toFloat(),
                            valueRange = NanoSettingsSchema.TRACKING_MIN.toFloat()..NanoSettingsSchema.TRACKING_MAX.toFloat(),
                            steps = 4,
                            onValueChangeFinished = { value -> onUpdateSettings { it.withTracking(value.toInt()) } },
                        )
                        SliderRow(
                            label = "Anchor",
                            valueLabel = { value -> "${value.toInt()}%" },
                            value = settings.typography.anchorPercent.toFloat(),
                            valueRange = NanoSettingsSchema.ANCHOR_PERCENT_MIN.toFloat()..NanoSettingsSchema.ANCHOR_PERCENT_MAX.toFloat(),
                            steps = 9,
                            onValueChangeFinished = { value -> onUpdateSettings { it.withAnchorPercent(value.toInt()) } },
                        )
                        SliderRow(
                            label = "Guide width",
                            valueLabel = { value -> "${NanoSettingsSchema.snapGuideWidth(value.toInt())}" },
                            value = settings.typography.guideWidth.toFloat(),
                            valueRange = NanoSettingsSchema.GUIDE_WIDTH_MIN.toFloat()..NanoSettingsSchema.GUIDE_WIDTH_MAX.toFloat(),
                            steps = 8,
                            snapValue = { value -> NanoSettingsSchema.snapGuideWidth(value.toInt()).toFloat() },
                            onValueChangeFinished = { value -> onUpdateSettings { it.withGuideWidth(value.toInt()) } },
                        )
                        SliderRow(
                            label = "Guide gap",
                            valueLabel = { value -> "${value.toInt()}" },
                            value = settings.typography.guideGap.toFloat(),
                            valueRange = NanoSettingsSchema.GUIDE_GAP_MIN.toFloat()..NanoSettingsSchema.GUIDE_GAP_MAX.toFloat(),
                            steps = 5,
                            onValueChangeFinished = { value -> onUpdateSettings { it.withGuideGap(value.toInt()) } },
                        )
                    }
                }
            }

            if (settings != null && uiState.isConnected) {
                item {
                    SectionCard(
                        title = "Reader Wi-Fi",
                        subtitle = "Saved on the Nano for RSS and OTA updates.",
                    ) {
                        val wifiStatus = uiState.wifiSettings?.let { wifi ->
                            if (wifi.configured) "Saved network: ${wifi.ssid}" else "No saved network"
                        } ?: "Reader Wi-Fi settings are not loaded."
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
                                Text(text = "Forget")
                            }
                        }
                    }
                }
            }
        }

        AnimatedVisibility(
            visible = uiState.showSettingsSaveStatus,
            modifier = Modifier.align(Alignment.BottomCenter),
            enter = fadeIn(animationSpec = tween(durationMillis = 160)) +
                slideInVertically(
                    animationSpec = tween(durationMillis = 180),
                    initialOffsetY = { it / 2 },
                ),
            exit = fadeOut(animationSpec = tween(durationMillis = 140)) +
                slideOutVertically(
                    animationSpec = tween(durationMillis = 160),
                    targetOffsetY = { it / 2 },
                ),
        ) {
            SettingsSaveStatus(
                uiState = uiState,
                modifier = Modifier.padding(bottom = 8.dp),
            )
        }
    }
}
}

@Composable
private fun SettingsSaveStatus(
    uiState: CompanionUiState,
    modifier: Modifier = Modifier,
) {
    Surface(
        modifier = modifier,
        color = MaterialTheme.colorScheme.secondaryContainer,
        contentColor = MaterialTheme.colorScheme.onSecondaryContainer,
        shadowElevation = 6.dp,
        shape = MaterialTheme.shapes.small,
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                imageVector = if (uiState.isSavingSettings) Icons.Outlined.Sync else Icons.Outlined.CheckCircle,
                contentDescription = null,
            )
            Text(
                text = if (uiState.isSavingSettings) {
                    "Saving settings..."
                } else {
                    "Saved on Nano - Exit sync to apply"
                },
                style = MaterialTheme.typography.labelLarge,
                maxLines = 1,
            )
        }
    }
}
