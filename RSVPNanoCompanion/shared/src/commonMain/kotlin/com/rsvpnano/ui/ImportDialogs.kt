package com.rsvpnano.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
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
import androidx.compose.material.icons.outlined.Refresh
import androidx.compose.material.icons.outlined.RssFeed
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
import com.rsvpnano.models.PendingUpload

@Composable
fun AddContentDialog(
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
fun AddArticleDialog(
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
fun RssFeedsDialog(
    uiState: CompanionUiState,
    onDismiss: () -> Unit,
    onFeedChange: (String) -> Unit,
    onAddFeed: () -> Unit,
    onRefreshFeeds: () -> Unit,
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
                    Button(onClick = onAddFeed, enabled = uiState.isConnected) {
                        Icon(imageVector = Icons.Outlined.RssFeed, contentDescription = null)
                        Text("Add")
                    }
                    TextButton(onClick = onRefreshFeeds, enabled = uiState.isConnected) {
                        Icon(imageVector = Icons.Outlined.Refresh, contentDescription = null)
                        Text("Refresh")
                    }
                }
                if (uiState.rssFeeds.isEmpty()) {
                    Text(
                        text = if (uiState.isConnected) "No RSS feeds saved on Nano." else "Connect to your Nano to load RSS feeds.",
                        style = MaterialTheme.typography.bodyMedium,
                    )
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
