package com.rsvpnano.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.MenuBook
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material.icons.outlined.Language
import androidx.compose.material.icons.outlined.MyLocation
import androidx.compose.material.icons.outlined.Newspaper
import androidx.compose.material.icons.outlined.Sync
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoBookLanguage
import com.rsvpnano.models.NanoFontSummary
import com.rsvpnano.models.NanoLanguageFont
import com.rsvpnano.models.PendingUpload
import kotlin.math.roundToInt

private enum class LibraryFilter(val label: String) {
    All("All"),
    Books("Books"),
    Articles("Articles"),
}


@Composable
internal fun LibraryScreen(
    uiState: CompanionUiState,
    onRefresh: () -> Unit,
    needsArticleFetch: (PendingUpload) -> Boolean,
    onEditDraft: (PendingUpload) -> Unit,
    onDeleteDraft: (PendingUpload) -> Unit,
    onSyncArticles: () -> Unit,
    onOpenBook: (NanoBook) -> Unit,
    onDeleteBook: (NanoBook) -> Unit,
    onAddContent: () -> Unit,
) {
    var searchQuery by rememberSaveable { mutableStateOf("") }
    var filterName by rememberSaveable { mutableStateOf(LibraryFilter.All.name) }
    val filter = LibraryFilter.valueOf(filterName)
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
            book.metadata.author.contains(query, ignoreCase = true) ||
            book.name.contains(query, ignoreCase = true)
        matchesFilter && matchesQuery
    }
    PullRefreshBox(
        isRefreshing = CompanionResource.Library in uiState.loadingResources,
        onRefresh = onRefresh,
    ) {
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            contentPadding = PaddingValues(bottom = 18.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            item {
                Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
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
                                onClick = { filterName = option.name },
                                label = { Text(option.label) },
                            )
                        }
                    }
                }
            }

            if (visibleDrafts.isNotEmpty()) {
                item {
                    Text(
                        text = "Queued articles",
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
                    LibraryEmptyState(
                        text = when {
                            !uiState.isConnected -> "Connect to view the reader library."
                            searchQuery.isNotBlank() || filter != LibraryFilter.All -> "No items match this search."
                            CompanionResource.Library in uiState.loadingResources -> "Loading reader library..."
                            CompanionResource.Library !in uiState.loadedResources -> "Reader library could not be loaded."
                            visibleDrafts.isNotEmpty() -> "No other items are on the reader yet."
                            else -> "Your reader library is empty."
                        },
                        onAddContent = onAddContent.takeIf { searchQuery.isBlank() && filter == LibraryFilter.All },
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
                        onOpenBook = { onOpenBook(book) },
                        onDeleteBook = { bookToDelete = book },
                    )
                }
            }
        }
    }

    bookToDelete?.let { book ->
        AlertDialog(
            onDismissRequest = { bookToDelete = null },
            icon = {
                Icon(
                    imageVector = Icons.Outlined.Delete,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.error,
                )
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
                Icon(
                    imageVector = Icons.Outlined.Delete,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.error,
                )
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
private fun PendingArticleRow(
    draft: PendingUpload,
    needsFetch: Boolean,
    onEdit: () -> Unit,
    onDelete: () -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onEdit)
            .padding(vertical = 10.dp),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(
            imageVector = Icons.Outlined.Newspaper,
            contentDescription = null,
            tint = if (needsFetch) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.primary,
        )
        Column(modifier = Modifier.weight(1f)) {
            Text(text = draft.title, style = MaterialTheme.typography.titleSmall)
            Text(
                text = listOfNotNull(
                    if (needsFetch) "Queued for download" else "Ready to sync",
                    draft.sourceUrl?.takeIf(String::isNotBlank)?.substringAfter("://")?.substringBefore('/'),
                ).joinToString(INLINE_DIVIDER),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        DestructiveIconButton(contentDescription = "Delete saved article", onClick = onDelete)
    }
}

private const val FONT_TARGET_MATH = "math"
private const val MATH_SCRIPT = "Zmth"

@Composable
private fun LibraryEmptyState(text: String, onAddContent: (() -> Unit)?) {
    Column(
        modifier = Modifier.fillMaxWidth().padding(vertical = 48.dp, horizontal = 24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Icon(
            Icons.AutoMirrored.Outlined.MenuBook,
            contentDescription = null,
            tint = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Text(text, color = MaterialTheme.colorScheme.onSurfaceVariant)
        if (onAddContent != null) {
            TextButton(onClick = onAddContent) { Text("Add content") }
        }
    }
}

@Composable
private fun LibraryBookRow(
    book: NanoBook,
    onOpenBook: () -> Unit,
    onDeleteBook: (NanoBook) -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onOpenBook)
            .padding(vertical = 12.dp),
        horizontalArrangement = Arrangement.spacedBy(12.dp),
        verticalAlignment = Alignment.Top,
    ) {
        Icon(
            imageVector = if (book.isArticle) Icons.Outlined.Newspaper else Icons.AutoMirrored.Outlined.MenuBook,
            contentDescription = null,
            tint = if (book.isArticle) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.secondary,
            modifier = Modifier.padding(top = 2.dp),
        )
        Column(
            modifier = Modifier.weight(1f),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Text(
                text = book.displayTitle,
                style = MaterialTheme.typography.titleSmall,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                text = book.librarySubtitle,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
            )
            book.readPercent?.let { progress ->
                LinearProgressIndicator(
                    progress = { progress.coerceIn(0, 100) / 100f },
                    modifier = Modifier.fillMaxWidth().height(2.dp),
                )
            }
        }
        DestructiveIconButton(
            contentDescription = "Delete ${book.displayTitle} from reader",
            onClick = { onDeleteBook(book) },
        )
    }
}

val NanoBook.librarySubtitle: String
    get() = buildList {
        metadata.author.takeIf(String::isNotBlank)?.let(::add)
        metadata.wordCount.takeIf { it > 0 }?.let { add("$it words") }
        if (!isArticle) metadata.chapters.size.takeIf { it > 0 }?.let { add("$it chapters") }
        readPercent?.let { add("$it% read") }
    }.joinToString(INLINE_DIVIDER)

@Composable
internal fun BookDetailScreen(
    book: NanoBook,
    availableFonts: List<NanoFontSummary>,
    globalFontId: String,
    wpm: Int,
    onSetPosition: (Int) -> Unit,
    onSetLanguageFonts: (List<NanoLanguageFont>) -> Unit,
) {
    val metadata = book.metadata
    val reading = book.reading
    val wordCount = metadata.wordCount
    val canSetProgress = wordCount > 0
    val currentIndex = if (canSetProgress) {
        (reading?.wordIndex ?: 0).coerceIn(0, wordCount - 1)
    } else {
        0
    }
    val chapters = metadata.chapters
        .filter { it.wordIndex in 0 until wordCount }
        .sortedBy { it.wordIndex }
    var targetIndex by remember(book.id, currentIndex) { mutableStateOf(currentIndex) }
    var showLanguageFonts by remember(book.id) { mutableStateOf(false) }

    fun percentForIndex(index: Int): Int = if (wordCount <= 1) {
        0
    } else {
        ((index.toFloat() / (wordCount - 1)) * 100).roundToInt().coerceIn(0, 100)
    }

    fun indexForPercent(percent: Int): Int = if (wordCount <= 1) {
        0
    } else {
        ((percent.coerceIn(0, 100) / 100f) * (wordCount - 1)).roundToInt().coerceIn(0, wordCount - 1)
    }

    val targetPercent = percentForIndex(targetIndex)
    val targetChapterIndex = chapters.indexOfLast { it.wordIndex <= targetIndex }

    Column(modifier = Modifier.fillMaxSize()) {
        LazyColumn(
            modifier = Modifier.weight(1f),
            contentPadding = PaddingValues(vertical = 12.dp),
            verticalArrangement = Arrangement.spacedBy(24.dp),
        ) {
            item {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                    verticalAlignment = Alignment.Top,
                ) {
                    Icon(
                        imageVector = if (book.isArticle) Icons.Outlined.Newspaper else Icons.AutoMirrored.Outlined.MenuBook,
                        contentDescription = null,
                        tint = if (book.isArticle) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.secondary,
                    )
                    Column(
                        modifier = Modifier.weight(1f),
                        verticalArrangement = Arrangement.spacedBy(3.dp),
                    ) {
                        Text(
                            text = metadata.author.ifBlank { if (book.isArticle) "Saved article" else "Unknown author" },
                            style = MaterialTheme.typography.titleMedium,
                        )
                        Text(
                            text = book.name,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 2,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                    if (canSetProgress) {
                        OutlinedButton(onClick = { showLanguageFonts = true }) {
                            Icon(imageVector = Icons.Outlined.Language, contentDescription = null)
                            Text("Language fonts")
                        }
                    }
                }
            }

            item {
                Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text("Book information", style = MaterialTheme.typography.titleMedium)
                    FlowRow(
                        horizontalArrangement = Arrangement.spacedBy(28.dp),
                        verticalArrangement = Arrangement.spacedBy(12.dp),
                    ) {
                        MetadataField("Words", metadata.wordCount.toString())
                        MetadataField("Chapters", metadata.chapters.size.toString())
                        MetadataField("File size", book.byteLabel)
                        metadata.locale.takeIf(String::isNotBlank)?.let { MetadataField("Language", it) }
                        metadata.scripts.takeIf { it.isNotEmpty() }
                            ?.let { MetadataField("Scripts", it.joinToString()) }
                    }
                }
            }

            item {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Reading progress", style = MaterialTheme.typography.titleMedium)
                    if (canSetProgress) {
                        Text(
                            text = "${percentForIndex(currentIndex)}% read",
                            style = MaterialTheme.typography.headlineSmall,
                        )
                        FlowRow(
                            horizontalArrangement = Arrangement.spacedBy(28.dp),
                            verticalArrangement = Arrangement.spacedBy(12.dp),
                        ) {
                            reading?.let {
                                val chapterIndex = chapters.indexOfLast { chapter -> chapter.wordIndex <= currentIndex }
                                if (chapterIndex >= 0) {
                                    MetadataField(
                                        "Current chapter",
                                        "${chapterIndex + 1}. ${chapters[chapterIndex].title}",
                                    )
                                }
                                val remainingWords = (wordCount - currentIndex - 1).coerceAtLeast(0)
                                MetadataField("Remaining", "$remainingWords words")
                                val minutes = if (wpm <= 0) 0 else (remainingWords + wpm - 1) / wpm
                                MetadataField("Reading time", "About $minutes min")
                            }
                        }
                    } else {
                        Text(
                            text = "Reading progress becomes available after the reader indexes this book.",
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            }

            if (chapters.isNotEmpty()) {
                item {
                    Text("Chapters", style = MaterialTheme.typography.titleMedium)
                }
                itemsIndexed(chapters, key = { index, chapter -> "${chapter.wordIndex}:$index" }) { index, chapter ->
                    val selected = index == targetChapterIndex
                    val atChapterStart = targetIndex == chapter.wordIndex
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(12.dp))
                            .background(
                                if (selected) MaterialTheme.colorScheme.secondaryContainer
                                else MaterialTheme.colorScheme.surface,
                            )
                            .padding(start = 12.dp, top = 8.dp, end = 4.dp, bottom = 8.dp),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                            Text("${index + 1}. ${chapter.title}", style = MaterialTheme.typography.titleSmall)
                            Text(
                                listOf(
                                    "${percentForIndex(chapter.wordIndex)}%",
                                    "Word ${chapter.wordIndex + 1}",
                                ).joinToString(INLINE_DIVIDER),
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        TextButton(onClick = { targetIndex = chapter.wordIndex }) {
                            Icon(Icons.Outlined.MyLocation, contentDescription = null)
                            Text(if (atChapterStart) "Target" else "Set here")
                        }
                    }
                }
            }

        }

        if (canSetProgress) {
            Surface(
                color = MaterialTheme.colorScheme.surfaceContainer,
                tonalElevation = 3.dp,
            ) {
                Column(
                    modifier = Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 10.dp),
                    verticalArrangement = Arrangement.spacedBy(2.dp),
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(12.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                if (targetIndex == currentIndex) "Current position" else "New position: $targetPercent%",
                                style = MaterialTheme.typography.labelLarge,
                            )
                            Text(
                                "Word ${targetIndex + 1} of $wordCount",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        Button(
                            onClick = { onSetPosition(targetIndex) },
                            enabled = targetIndex != currentIndex,
                        ) {
                            Icon(Icons.Outlined.MyLocation, contentDescription = null)
                            Text(if (targetIndex == currentIndex) "Unchanged" else "Update")
                        }
                    }
                    Slider(
                        value = targetPercent.toFloat(),
                        onValueChange = { targetIndex = indexForPercent(it.roundToInt()) },
                        valueRange = 0f..100f,
                        steps = 99,
                    )
                }
            }
        }
    }

    if (showLanguageFonts) {
        BookLanguageFontsDialog(
            book = book,
            availableFonts = availableFonts,
            globalFontId = globalFontId,
            onDismiss = { showLanguageFonts = false },
            onSave = { selections ->
                showLanguageFonts = false
                onSetLanguageFonts(selections)
            },
        )
    }
}

@Composable
private fun BookLanguageFontsDialog(
    book: NanoBook,
    availableFonts: List<NanoFontSummary>,
    globalFontId: String,
    onDismiss: () -> Unit,
    onSave: (List<NanoLanguageFont>) -> Unit,
) {
    var selections by remember(book.id) { mutableStateOf(book.reading?.languageFonts.orEmpty()) }
    val languages = book.metadata.languages.ifEmpty {
        listOfNotNull(
            book.metadata.locale.takeIf(String::isNotBlank)?.let {
                NanoBookLanguage(it, book.metadata.scripts)
            },
        )
    }.map { it.copy(scripts = it.scripts.filterNot(MATH_SCRIPT::equals)) }
        .filter { it.scripts.isNotEmpty() }
    val hasMath = MATH_SCRIPT in book.metadata.scripts
    AlertDialog(
        onDismissRequest = onDismiss,
        icon = { Icon(imageVector = Icons.Outlined.Language, contentDescription = null) },
        title = { Text("Language fonts") },
        text = {
            LazyColumn(verticalArrangement = Arrangement.spacedBy(14.dp)) {
                items(languages, key = { it.locale }) { bookLanguage ->
                    val locale = bookLanguage.locale
                    val requiredScripts = bookLanguage.scripts
                    val compatible = availableFonts.filter { it.usableFor(locale, requiredScripts) }
                    val selectedId = selections.firstOrNull { it.locale == locale }?.fontId
                    FontTargetOptions(locale, selectedId, compatible, availableFonts, globalFontId) { fontId ->
                        selections = selections.filterNot { it.locale == locale } + listOfNotNull(
                            fontId?.let { NanoLanguageFont(locale = locale, fontId = it) },
                        )
                    }
                }
                if (hasMath) {
                    item(key = "math") {
                        val compatible = availableFonts.filter {
                            MATH_SCRIPT in it.scripts
                        }
                        val selectedId = selections.firstOrNull { it.locale == FONT_TARGET_MATH }?.fontId
                        FontTargetOptions("Math", selectedId, compatible, availableFonts, globalFontId) { fontId ->
                            selections = selections.filterNot { it.locale == FONT_TARGET_MATH } +
                                listOfNotNull(
                                    fontId?.let { NanoLanguageFont(locale = FONT_TARGET_MATH, fontId = it) },
                                )
                        }
                    }
                }
            }
        },
        confirmButton = {
            Button(onClick = { onSave(selections) }) { Text("Save") }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

@Composable
private fun FontTargetOptions(
    label: String,
    selectedId: String?,
    compatible: List<NanoFontSummary>,
    availableFonts: List<NanoFontSummary>,
    globalFontId: String,
    onSelect: (String?) -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Text(label, style = MaterialTheme.typography.titleSmall)
        FlowRow(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            FilterChip(
                selected = selectedId == null,
                onClick = { onSelect(null) },
                label = {
                    val globalName = availableFonts.firstOrNull { it.id == globalFontId }?.name
                        ?: globalFontId.ifBlank { "default" }
                    Text("Global ($globalName)")
                },
            )
            compatible.forEach { font ->
                FilterChip(
                    selected = selectedId == font.id,
                    onClick = { onSelect(font.id) },
                    label = { Text(font.name) },
                )
            }
        }
    }
}

@Composable
private fun MetadataField(label: String, value: String) {
    Column(verticalArrangement = Arrangement.spacedBy(1.dp)) {
        Text(text = label, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(text = value, style = MaterialTheme.typography.bodySmall)
    }
}

val NanoBook.isArticle: Boolean
    get() = name.startsWith("articles/")

val NanoBook.readPercent: Int?
    get() = reading?.let {
        val lastIndex = (metadata.wordCount - 1).coerceAtLeast(1)
        (it.wordIndex.coerceIn(0, lastIndex) * 100) / lastIndex
    }

val NanoBook.byteLabel: String
    get() = bytes.toByteLabel()

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

