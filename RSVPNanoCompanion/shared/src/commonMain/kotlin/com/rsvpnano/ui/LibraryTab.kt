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

private enum class LibraryFilter(val label: String) {
    All("All"),
    Books("Books"),
    Articles("Articles"),
}


@Composable
fun LibraryTab(
    uiState: CompanionUiState,
    onRefresh: () -> Unit,
    needsArticleFetch: (PendingUpload) -> Boolean,
    onEditDraft: (PendingUpload) -> Unit,
    onDeleteDraft: (PendingUpload) -> Unit,
    onSyncArticles: () -> Unit,
    onDeleteBook: (NanoBook) -> Unit,
    onSetBookPosition: (NanoBook, Int) -> Unit,
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
            book.displayName.contains(query, ignoreCase = true)
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
            onSetPosition = { wordIndex ->
                selectedBook = null
                onSetBookPosition(book, wordIndex)
            },
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
    onSetPosition: (Int) -> Unit,
) {
    val wordCount = book.wordCount?.takeIf { it > 0 }
    val initialWordIndex = if (wordCount == null) {
        0
    } else {
        (book.wordIndex ?: 0).coerceIn(0, wordCount - 1)
    }
    var targetWordIndex by remember(book.id) { mutableStateOf(initialWordIndex) }
    var wordText by remember(book.id) { mutableStateOf((initialWordIndex + 1).toString()) }
    val chapters = if (wordCount == null) {
        emptyList()
    } else {
        book.chapters.filter { it.wordIndex in 0 until wordCount }.sortedBy { it.wordIndex }
    }
    fun updateTarget(index: Int) {
        if (wordCount == null) {
            return
        }
        targetWordIndex = index.coerceIn(0, wordCount - 1)
        wordText = (targetWordIndex + 1).toString()
    }
    fun currentChapterIndex(): Int {
        if (chapters.isEmpty()) {
            return -1
        }
        var selected = 0
        chapters.forEachIndexed { index, chapter ->
            if (chapter.wordIndex <= targetWordIndex) {
                selected = index
            }
        }
        return selected
    }

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
                Text(text = "Path: ${book.displayName}")
                Text(text = "Type: ${if (book.isArticle) "Article" else "Book"}")
                book.progressPercent?.let { progress ->
                    Text(text = "Progress: ${progress.coerceIn(0, 100)}%")
                }
                if (wordCount == null || book.sourceSize == null || book.sourceFingerprint == null) {
                    Text(
                        text = "Resume location unavailable until the book has been indexed on the reader.",
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                } else {
                    val chapterIndex = currentChapterIndex()
                    val chapterStart = chapters.getOrNull(chapterIndex)?.wordIndex ?: 0
                    val chapterEnd = chapters.getOrNull(chapterIndex + 1)?.wordIndex ?: wordCount
                    Text(text = "Resume location: word ${targetWordIndex + 1} of $wordCount")
                    if (chapters.isNotEmpty()) {
                        Row(
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            TextButton(
                                onClick = { updateTarget(chapters[(chapterIndex - 1).coerceAtLeast(0)].wordIndex) },
                                enabled = chapterIndex > 0,
                            ) {
                                Text("Previous")
                            }
                            Text(
                                text = "${chapterIndex + 1}/${chapters.size} ${chapters[chapterIndex].title}",
                                modifier = Modifier.weight(1f),
                            )
                            TextButton(
                                onClick = { updateTarget(chapters[(chapterIndex + 1).coerceAtMost(chapters.lastIndex)].wordIndex) },
                                enabled = chapterIndex in 0 until chapters.lastIndex,
                            ) {
                                Text("Next")
                            }
                        }
                    }
                    if (chapterEnd - chapterStart > 1) {
                        Slider(
                            value = targetWordIndex.toFloat(),
                            onValueChange = { updateTarget(it.toInt()) },
                            valueRange = chapterStart.toFloat()..(chapterEnd - 1).toFloat(),
                        )
                    }
                    OutlinedTextField(
                        value = wordText,
                        onValueChange = { value ->
                            wordText = value.filter { it.isDigit() }.take(9)
                            wordText.toIntOrNull()?.let { updateTarget(it - 1) }
                        },
                        label = { Text("Word number") },
                        singleLine = true,
                    )
                }
            }
        },
        confirmButton = {
            if (wordCount != null && book.sourceSize != null && book.sourceFingerprint != null) {
                Button(onClick = { onSetPosition(targetWordIndex) }) {
                    Text(text = "Save location")
                }
            } else {
                TextButton(onClick = onDismiss) {
                    Text(text = "Close")
                }
            }
        },
        dismissButton = if (wordCount != null && book.sourceSize != null && book.sourceFingerprint != null) {
            {
                TextButton(onClick = onDismiss) {
                    Text(text = "Cancel")
                }
            }
        } else {
            null
        },
    )
}

val NanoBook.isArticle: Boolean
    get() = category == "article" || displayName.lowercase().startsWith("articles/")

val NanoBook.libraryMetaLabel: String
    get() = listOfNotNull(
        author?.takeIf { it.isNotBlank() },
        byteLabel,
        displayName.takeIf { displayTitle != displayName.substringAfterLast('/') },
    ).joinToString(" · ").ifBlank { id }

val NanoBook.byteLabel: String
    get() = bytes.toByteLabel()

fun pendingArticleMeta(draft: PendingUpload, needsFetch: Boolean): String {
    val state = if (needsFetch) "Needs article text" else "Ready to sync"
    val source = draft.sourceUrl?.takeIf { it.isNotBlank() }?.substringAfter("://")?.substringBefore("/")
    return listOfNotNull(state, draft.body.encodeToByteArray().size.toByteLabel(), source).joinToString(" · ")
}

fun Int.toByteLabel(): String {
    return when {
        this < 1024 -> "$this B"
        this < 1024 * 1024 -> "${oneDecimal(this / 1024.0)} KB"
        else -> "${oneDecimal(this / (1024.0 * 1024.0))} MB"
    }
}

private fun oneDecimal(value: Double): String {
    val scaled = (value * 10.0).toInt()
    return "${scaled / 10}.${scaled % 10}"
}

