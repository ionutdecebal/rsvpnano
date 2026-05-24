package com.rsvpnano.android.ui

import android.app.Activity
import android.content.Context
import android.content.ContextWrapper
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.OpenableColumns
import android.provider.Settings
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
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.pullrefresh.PullRefreshIndicator
import androidx.compose.material.pullrefresh.pullRefresh
import androidx.compose.material.pullrefresh.rememberPullRefreshState
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
import com.rsvpnano.models.PendingUpload

@Composable
fun ArticlesTab(
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
fun RssTab(
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
