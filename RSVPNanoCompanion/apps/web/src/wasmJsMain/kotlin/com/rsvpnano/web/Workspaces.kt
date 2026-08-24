package com.rsvpnano.web

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.background
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.MenuBook
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material.icons.outlined.Newspaper
import androidx.compose.material.icons.outlined.Refresh
import androidx.compose.material.icons.outlined.Search
import androidx.compose.material.icons.outlined.Sync
import androidx.compose.material.icons.outlined.UploadFile
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.draw.clip
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoFocusTimer
import com.rsvpnano.models.NanoFocusTimerRules
import com.rsvpnano.models.NanoFocusTimers
import com.rsvpnano.models.NanoLocales
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.NanoSettingsSchema
import com.rsvpnano.models.PendingUpload
import com.rsvpnano.models.needsArticleFetch
import com.rsvpnano.ui.CompanionPresenter
import com.rsvpnano.ui.CatalogInstall
import com.rsvpnano.ui.CatalogAsset
import com.rsvpnano.ui.CompanionResource
import com.rsvpnano.ui.CompanionUiState
import com.rsvpnano.ui.TypographyPreview
import com.rsvpnano.ui.fontDetails
import com.rsvpnano.ui.localeDetails
import com.rsvpnano.ui.byteLabel
import com.rsvpnano.ui.isArticle
import com.rsvpnano.ui.librarySubtitle
import com.rsvpnano.ui.readPercent
import io.github.vinceglb.filekit.dialogs.FileKitType
import io.github.vinceglb.filekit.dialogs.compose.rememberFilePickerLauncher
import io.github.vinceglb.filekit.name
import io.github.vinceglb.filekit.readBytes
import kotlinx.coroutines.launch
import kotlinx.browser.window

@Composable
internal fun ColumnScope.LibraryWorkspace(presenter: CompanionPresenter, state: CompanionUiState) {
    val scope = rememberCoroutineScope()
    var filter by remember { mutableStateOf(WebLibraryFilter.Books) }
    var search by remember { mutableStateOf("") }
    var selectedBookId by remember { mutableStateOf<String?>(null) }
    var editingArticle by remember { mutableStateOf(false) }
    var uploadCategory by remember { mutableStateOf("book") }
    val picker = rememberFilePickerLauncher(
        type = FileKitType.File(extensions = listOf("epub", "txt", "html", "htm", "rsvp")),
    ) { file ->
        if (file != null) scope.launch {
            presenter.uploadSelectedFile(file.name, file.readBytes(), uploadCategory)
        }
    }

    LaunchedEffect(state.isConnected) {
        if (state.isConnected && CompanionResource.Library !in state.loadedResources) presenter.refreshLibrary()
    }

    val query = search.trim()
    val visibleBooks = state.books.filter { book ->
        book.isArticle == (filter == WebLibraryFilter.Articles) && (
            query.isEmpty() || book.displayTitle.contains(query, ignoreCase = true) ||
                book.metadata.author.contains(query, ignoreCase = true) || book.name.contains(query, ignoreCase = true)
            )
    }
    val visibleDrafts = if (filter == WebLibraryFilter.Articles) {
        state.drafts.filter { draft ->
            query.isEmpty() || draft.title.contains(query, ignoreCase = true) ||
                draft.sourceUrl.orEmpty().contains(query, ignoreCase = true)
        }
    } else {
        emptyList()
    }
    val selectedBook = visibleBooks.firstOrNull { it.id == selectedBookId } ?: visibleBooks.firstOrNull()

    LaunchedEffect(filter, search, state.books) {
        if (!editingArticle) selectedBookId = selectedBook?.id
    }

    LibraryToolbar(
        filter = filter,
        search = search,
        connected = state.isConnected,
        onFilter = {
            filter = it
            editingArticle = false
            selectedBookId = null
        },
        onSearch = { search = it },
        onRefresh = presenter::refreshLibrary,
        onUpload = { category ->
            uploadCategory = category
            picker.launch()
        },
        onNewArticle = {
            filter = WebLibraryFilter.Articles
            selectedBookId = null
            editingArticle = true
            presenter.cancelDraftEdit()
        },
    )

    state.bookJob?.let { job ->
        Surface(
            color = MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.48f),
            shape = RoundedCornerShape(8.dp),
        ) {
            Column(Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 11.dp), verticalArrangement = Arrangement.spacedBy(7.dp)) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    Text("${job.active.activeLabel} ${job.name}", fontWeight = FontWeight.Bold)
                    job.percent?.let { Text("$it%", style = MaterialTheme.typography.labelMedium) }
                }
                val progress = job.progress
                if (progress != null) LinearProgressIndicator(progress = { progress }, Modifier.fillMaxWidth())
                else LinearProgressIndicator(Modifier.fillMaxWidth())
            }
        }
    }

    BoxWithConstraints(Modifier.fillMaxWidth().weight(1f)) {
        if (maxWidth >= 840.dp) {
            Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                LibraryListPane(
                    books = visibleBooks,
                    drafts = visibleDrafts,
                    selectedBookId = selectedBook?.id.takeUnless { editingArticle },
                    connected = state.isConnected,
                    loading = CompanionResource.Library in state.loadingResources,
                    needsArticleFetch = { it.needsArticleFetch() },
                    onSelectBook = {
                        selectedBookId = it.id
                        editingArticle = false
                    },
                    onEditDraft = {
                        presenter.editDraft(it)
                        editingArticle = true
                        selectedBookId = null
                    },
                    onDeleteDraft = presenter::deleteDraft,
                    onSyncArticles = presenter::syncSavedArticles,
                    modifier = Modifier.weight(3f).fillMaxHeight(),
                )
                Column(Modifier.weight(2f).fillMaxHeight().verticalScroll(rememberScrollState())) {
                    if (editingArticle) {
                        DraftEditor(presenter, state, onClose = { editingArticle = false })
                    } else {
                        LibraryDetailPane(selectedBook, presenter)
                    }
                }
            }
        } else {
            Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                LibraryListPane(
                    books = visibleBooks,
                    drafts = visibleDrafts,
                    selectedBookId = selectedBook?.id.takeUnless { editingArticle },
                    connected = state.isConnected,
                    loading = CompanionResource.Library in state.loadingResources,
                    needsArticleFetch = { it.needsArticleFetch() },
                    onSelectBook = {
                        selectedBookId = it.id
                        editingArticle = false
                    },
                    onEditDraft = {
                        presenter.editDraft(it)
                        editingArticle = true
                        selectedBookId = null
                    },
                    onDeleteDraft = presenter::deleteDraft,
                    onSyncArticles = presenter::syncSavedArticles,
                )
                if (editingArticle) DraftEditor(presenter, state, onClose = { editingArticle = false })
                else LibraryDetailPane(selectedBook, presenter)
            }
        }
    }
}

private enum class WebLibraryFilter(val label: String) {
    Books("Books"),
    Articles("Articles"),
}

@Composable
private fun LibraryToolbar(
    filter: WebLibraryFilter,
    search: String,
    connected: Boolean,
    onFilter: (WebLibraryFilter) -> Unit,
    onSearch: (String) -> Unit,
    onRefresh: () -> Unit,
    onUpload: (String) -> Unit,
    onNewArticle: () -> Unit,
) {
    BoxWithConstraints(Modifier.fillMaxWidth()) {
        val narrow = maxWidth < 880.dp
        val filters: @Composable () -> Unit = {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                WebLibraryFilter.entries.forEach { option ->
                    FilterChip(selected = filter == option, onClick = { onFilter(option) }, label = { Text(option.label) })
                }
            }
        }
        val actions: @Composable () -> Unit = {
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("ADD TO READER", Modifier.align(Alignment.CenterVertically), style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.secondary)
                Button(onClick = { onUpload("book") }, enabled = connected) {
                    Icon(Icons.Outlined.UploadFile, null, Modifier.size(18.dp))
                    Spacer(Modifier.width(7.dp))
                    Text("Book file")
                }
                OutlinedButton(onClick = { onUpload("article") }, enabled = connected) {
                    Icon(Icons.Outlined.UploadFile, null, Modifier.size(18.dp))
                    Spacer(Modifier.width(7.dp))
                    Text("Article file")
                }
                OutlinedButton(onClick = onNewArticle) { Text("Write article") }
            }
        }
        Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            if (narrow) {
                filters()
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    LibrarySearchField(search, onSearch, Modifier.weight(1f))
                    IconButton(onClick = onRefresh, enabled = connected) {
                        Icon(Icons.Outlined.Refresh, "Refresh library")
                    }
                }
            } else {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp), verticalAlignment = Alignment.CenterVertically) {
                    filters()
                    LibrarySearchField(search, onSearch, Modifier.weight(1f))
                    IconButton(onClick = onRefresh, enabled = connected) {
                        Icon(Icons.Outlined.Refresh, "Refresh library")
                    }
                }
            }
            actions()
        }
    }
}

@Composable
private fun LibrarySearchField(value: String, onValueChange: (String) -> Unit, modifier: Modifier = Modifier) {
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        placeholder = { Text("Search title or author") },
        leadingIcon = { Icon(Icons.Outlined.Search, null) },
        singleLine = true,
        modifier = modifier,
    )
}

@Composable
private fun LibraryListPane(
    books: List<NanoBook>,
    drafts: List<PendingUpload>,
    selectedBookId: String?,
    connected: Boolean,
    loading: Boolean,
    needsArticleFetch: (PendingUpload) -> Boolean,
    onSelectBook: (NanoBook) -> Unit,
    onEditDraft: (PendingUpload) -> Unit,
    onDeleteDraft: (PendingUpload) -> Unit,
    onSyncArticles: () -> Unit,
    modifier: Modifier = Modifier,
) {
    SectionCard(modifier) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("ON THE READER", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.secondary)
            Text("${books.size} ${if (books.size == 1) "item" else "items"}", style = MaterialTheme.typography.labelSmall)
        }
        Column(Modifier.fillMaxWidth().weight(1f, fill = false).verticalScroll(rememberScrollState())) {
            drafts.forEach { draft ->
                LibraryDraftRow(draft, needsArticleFetch(draft), onEditDraft, onDeleteDraft)
            }
            books.forEach { book ->
                LibraryBookRow(book, selectedBookId == book.id) { onSelectBook(book) }
            }
            if (books.isEmpty() && drafts.isEmpty()) {
                Text(
                    when {
                        !connected -> "Connect a Nano to load its library."
                        loading -> "Loading library..."
                        else -> "Nothing matches this view."
                    },
                    Modifier.padding(vertical = 28.dp),
                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.65f),
                )
            }
        }
        if (drafts.isNotEmpty()) {
            OutlinedButton(onClick = onSyncArticles, enabled = connected && drafts.any { !needsArticleFetch(it) }) {
                Icon(Icons.Outlined.Sync, null, Modifier.size(18.dp))
                Spacer(Modifier.width(7.dp))
                Text("Sync ready articles")
            }
        }
    }
}

@Composable
private fun LibraryBookRow(book: NanoBook, selected: Boolean, onClick: () -> Unit) {
    val shape = RoundedCornerShape(8.dp)
    Row(
        Modifier.fillMaxWidth().padding(vertical = 2.dp).clip(shape)
            .background(if (selected) MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.62f) else MaterialTheme.colorScheme.surface)
            .clickable(onClick = onClick).padding(horizontal = 12.dp, vertical = 11.dp),
        horizontalArrangement = Arrangement.spacedBy(11.dp),
        verticalAlignment = Alignment.Top,
    ) {
        Icon(
            if (book.isArticle) Icons.Outlined.Newspaper else Icons.AutoMirrored.Outlined.MenuBook,
            null,
            Modifier.size(20.dp),
            tint = if (book.isArticle) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.secondary,
        )
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(book.displayTitle, fontWeight = FontWeight.Bold)
            book.librarySubtitle.takeIf(String::isNotBlank)?.let {
                Text(it, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f))
            }
            book.readPercent?.let { progress ->
                LinearProgressIndicator(progress = { progress / 100f }, Modifier.fillMaxWidth().height(2.dp))
            }
        }
    }
}

@Composable
private fun LibraryDraftRow(
    draft: PendingUpload,
    needsFetch: Boolean,
    onEdit: (PendingUpload) -> Unit,
    onDelete: (PendingUpload) -> Unit,
) {
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(8.dp)).clickable { onEdit(draft) }
            .padding(horizontal = 12.dp, vertical = 11.dp),
        horizontalArrangement = Arrangement.spacedBy(11.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(Icons.Outlined.Newspaper, null, Modifier.size(20.dp), tint = MaterialTheme.colorScheme.tertiary)
        Column(Modifier.weight(1f)) {
            Text(draft.title, fontWeight = FontWeight.Bold)
            Text(
                if (needsFetch) "Needs article text" else "Saved locally, ready to sync",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f),
            )
        }
        IconButton(onClick = { onDelete(draft) }) { Icon(Icons.Outlined.Delete, "Delete saved article") }
    }
}

@Composable
private fun LibraryDetailPane(book: NanoBook?, presenter: CompanionPresenter) {
    SectionCard {
        Text("DETAILS", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.secondary)
        if (book == null) {
            Text("Select an item to see its information.", color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.65f))
            return@SectionCard
        }
        Icon(
            if (book.isArticle) Icons.Outlined.Newspaper else Icons.AutoMirrored.Outlined.MenuBook,
            null,
            Modifier.size(28.dp),
            tint = if (book.isArticle) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.secondary,
        )
        Text(book.displayTitle, style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
        book.metadata.author.takeIf(String::isNotBlank)?.let { Text(it, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.72f)) }
        DetailRow("Words", book.metadata.wordCount.toString())
        DetailRow("Chapters", book.metadata.chapters.size.toString())
        DetailRow("File size", book.byteLabel)
        book.metadata.locale.takeIf(String::isNotBlank)?.let { DetailRow("Language", it) }
        book.readPercent?.let { progress ->
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                DetailRow("Reading progress", "$progress%")
                LinearProgressIndicator(progress = { progress / 100f }, Modifier.fillMaxWidth())
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { presenter.setBookPosition(book, 0) }) { Text("Reset position") }
            OutlinedButton(onClick = { presenter.deleteDeviceBook(book) }) {
                Icon(Icons.Outlined.Delete, null, Modifier.size(18.dp))
                Spacer(Modifier.width(7.dp))
                Text("Remove")
            }
        }
    }
}

@Composable
private fun DraftEditor(
    presenter: CompanionPresenter,
    state: CompanionUiState,
    onClose: () -> Unit,
    modifier: Modifier = Modifier,
) {
    SectionCard(modifier) {
        Text(if (state.editingDraftId == null) "NEW ARTICLE" else "EDIT ARTICLE", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.secondary)
        OutlinedTextField(
            value = state.draftTitle,
            onValueChange = presenter::setDraftTitle,
            label = { Text("Title") },
            modifier = Modifier.fillMaxWidth(),
            singleLine = true,
        )
        OutlinedTextField(
            value = state.draftSourceUrl,
            onValueChange = presenter::setDraftSourceUrl,
            label = { Text("Article URL (optional)") },
            modifier = Modifier.fillMaxWidth(),
            singleLine = true,
        )
        OutlinedTextField(
            value = state.draftBody,
            onValueChange = presenter::setDraftBody,
            label = { Text("Paste article text or HTML") },
            modifier = Modifier.fillMaxWidth().height(150.dp),
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = {
                presenter.saveTextDraft()
                onClose()
            }) { Text("Save text") }
            OutlinedButton(onClick = {
                presenter.saveLinkDraft()
                onClose()
            }, enabled = state.draftSourceUrl.isNotBlank()) {
                Text("Fetch URL")
            }
            TextButton(onClick = {
                presenter.cancelDraftEdit()
                onClose()
            }) { Text("Cancel") }
        }
    }
}

@Composable
internal fun ColumnScope.AppearanceWorkspace(presenter: CompanionPresenter, state: CompanionUiState) {
    val scope = rememberCoroutineScope()
    val themePicker = rememberFilePickerLauncher(FileKitType.File(listOf("toml"))) { file ->
        if (file != null) scope.launch { presenter.uploadThemeFile(file.name, file.readBytes()) }
    }
    val fontPicker = rememberFilePickerLauncher(FileKitType.File(listOf("rfont4"))) { file ->
        if (file != null) scope.launch { presenter.uploadFontFile(file.name, file.readBytes()) }
    }
    val localePicker = rememberFilePickerLauncher(FileKitType.File(listOf("zip"))) { file ->
        if (file != null) scope.launch { presenter.installLocalePackFile(file.name, file.readBytes()) }
    }

    LaunchedEffect(state.isConnected) {
        if (!state.isConnected) return@LaunchedEffect
        presenter.refreshSettings()
        presenter.refreshThemes()
        presenter.refreshFonts()
        presenter.refreshLocales()
        presenter.refreshThemeCatalog()
        presenter.refreshFontCatalog()
        presenter.refreshLocaleCatalog()
    }

    val themeEntries = remember(state.availableThemes, state.themeCatalog, state.settings, state.catalogInstall) {
        val installed = state.availableThemes.associateBy { it.id }
        val catalog = state.themeCatalog.associateBy { it.id }
        buildList {
            (listOf(NanoSettingsSchema.THEME_DEFAULT) + catalog.keys + installed.keys).distinct().forEach { id ->
                val local = installed[id]
                val online = catalog[id]
                val isInstalled = id == NanoSettingsSchema.THEME_DEFAULT || local != null
                add(CatalogEntry(
                    id = id,
                    title = local?.name ?: online?.name ?: "Default",
                    subtitle = if (id == NanoSettingsSchema.THEME_DEFAULT) "Built in" else if (isInstalled) "Installed" else "Available to install",
                    selected = isInstalled && state.settings?.`interface`?.selectedThemeId == id,
                    install = state.catalogInstall?.takeIf { it.asset == CatalogAsset.Theme && it.id == id },
                    onSelect = if (isInstalled) ({ presenter.selectTheme(id) }) else null,
                    onDelete = if (local != null && id != NanoSettingsSchema.THEME_DEFAULT) ({ presenter.removeTheme(id) }) else null,
                    onInstall = if (!isInstalled && online != null) ({ presenter.installOnlineTheme(id) }) else null,
                ))
            }
        }
    }
    val fontEntries = remember(state.availableFonts, state.fontCatalog, state.settings, state.catalogInstall) {
        val installed = state.availableFonts.associateBy { it.id }
        val catalog = state.fontCatalog.associateBy { it.id }
        (catalog.keys + installed.keys).distinct().map { id ->
            val local = installed[id]
            val online = catalog[id]
            CatalogEntry(
                id = id,
                title = local?.name ?: online?.name ?: id,
                subtitle = local?.let { fontDetails(it.scripts, it.builtIn, false) }
                    ?: online?.let { fontDetails(it.scripts, false, it.shaping) }.orEmpty(),
                selected = local != null && state.settings?.reading?.typography?.fontId == id,
                install = state.catalogInstall?.takeIf { it.asset == CatalogAsset.Font && it.id == id },
                onSelect = local?.let { { presenter.selectFont(id) } },
                onDelete = local?.takeIf { !it.builtIn }?.let { { presenter.removeFont(id) } },
                onInstall = online?.takeIf { local == null }?.let { { presenter.installOnlineFont(id) } },
            )
        }
    }
    val localeEntries = remember(state.availableLocales, state.localeCatalog, state.settings, state.catalogInstall) {
        val installed = state.availableLocales.associateBy { it.id }
        val catalog = state.localeCatalog.associateBy { it.id }
        buildList {
            add(CatalogEntry(
                id = NanoLocales.DEFAULT,
                title = "English",
                subtitle = "Built in, left-to-right",
                selected = state.settings?.`interface`?.locale == NanoLocales.DEFAULT,
                onSelect = { presenter.selectLocale(NanoLocales.DEFAULT) },
            ))
            (catalog.keys + installed.keys).distinct().filterNot { it == NanoLocales.DEFAULT }.forEach { id ->
                val local = installed[id]
                val online = catalog[id]
                add(CatalogEntry(
                    id = id,
                    title = online?.englishName
                        ?.replace("ChineseSimplified", "Simplified Chinese")
                        ?.replace("ChineseTraditional", "Traditional Chinese")
                        ?: local?.locale
                        ?: id,
                    subtitle = online?.let { localeDetails(it.englishName, it.direction, it.translationStatus, it.version) }
                        ?: local?.locale.orEmpty(),
                    selected = local != null && state.settings?.`interface`?.locale == local.locale,
                    install = state.catalogInstall?.takeIf { it.asset == CatalogAsset.Locale && it.id == id },
                    onSelect = local?.let { { presenter.selectLocale(it.locale) } },
                    onDelete = local?.let { { presenter.removeLocalePack(id) } },
                    onInstall = online?.takeIf { local == null }?.let { { presenter.installOnlineLocalePack(id) } },
                ))
            }
        }
    }

    var selectedCatalog by remember { mutableStateOf(0) }
    BoxWithConstraints(Modifier.fillMaxWidth().weight(1f)) {
        val catalogs = listOf<@Composable () -> Unit>(
            { CatalogColumn("Themes", state.themeCatalogUrl, themeEntries, presenter::refreshThemeCatalog) { themePicker.launch() } },
            { CatalogColumn("Reader fonts", state.fontCatalogUrl, fontEntries, presenter::refreshFontCatalog) { fontPicker.launch() } },
            { CatalogColumn("Interface languages", state.localeCatalogUrl, localeEntries, presenter::refreshLocaleCatalog) { localePicker.launch() } },
        )
        if (maxWidth >= 960.dp) {
            Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                catalogs.forEach { catalog ->
                    Column(Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState())) { catalog() }
                }
            }
        } else {
            Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf("Themes", "Fonts", "Languages").forEachIndexed { index, label ->
                        val active = selectedCatalog == index
                        val shape = RoundedCornerShape(14.dp)
                        Surface(
                            modifier = Modifier.weight(1f).clip(shape).clickable { selectedCatalog = index },
                            shape = shape,
                            color = if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surface,
                            contentColor = if (active) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurface,
                        ) {
                            Text(label, Modifier.padding(vertical = 10.dp), textAlign = androidx.compose.ui.text.style.TextAlign.Center)
                        }
                    }
                }
                Box(Modifier.fillMaxWidth().weight(1f).verticalScroll(rememberScrollState())) {
                    catalogs[selectedCatalog]()
                }
            }
        }
    }
}

private data class CatalogEntry(
    val id: String,
    val title: String,
    val subtitle: String,
    val selected: Boolean = false,
    val install: CatalogInstall? = null,
    val onSelect: (() -> Unit)? = null,
    val onDelete: (() -> Unit)? = null,
    val onInstall: (() -> Unit)? = null,
)

@Composable
private fun CatalogColumn(
    title: String,
    source: String,
    entries: List<CatalogEntry>,
    onRefresh: () -> Unit,
    onUpload: () -> Unit,
) {
    SectionCard {
        Text(title, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text(catalogSourceLabel(source), Modifier.weight(1f), style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.62f))
            IconButton(onClick = onRefresh) { Icon(Icons.Outlined.Sync, "Refresh $title") }
            OutlinedButton(onClick = onUpload) {
                Icon(Icons.Outlined.UploadFile, null, Modifier.size(18.dp))
                Spacer(Modifier.width(5.dp))
                Text("Upload")
            }
        }
        if (entries.isEmpty()) Text("Connect a Nano to load this catalog.")
        entries.forEach { entry -> CatalogRow(entry) }
    }
}

@Composable
private fun CatalogRow(entry: CatalogEntry) {
    val shape = RoundedCornerShape(12.dp)
    Surface(
        modifier = Modifier.fillMaxWidth().clip(shape)
            .border(1.dp, if (entry.selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.22f), shape)
            .then(if (entry.onSelect == null) Modifier else Modifier.clickable(onClick = entry.onSelect)),
        shape = shape,
        color = if (entry.selected) MaterialTheme.colorScheme.primary.copy(alpha = 0.1f) else MaterialTheme.colorScheme.surface,
    ) {
        Column {
            Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 10.dp), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(entry.title, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                    if (entry.selected) Text("Default", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
                    if (entry.subtitle.isNotBlank()) Text(entry.subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.65f))
                }
                when {
                    entry.onDelete != null -> IconButton(onClick = entry.onDelete) {
                        Icon(Icons.Outlined.Delete, "Remove ${entry.title}", tint = MaterialTheme.colorScheme.error)
                    }
                    entry.onInstall != null -> TextButton(onClick = entry.onInstall) { Text("Install") }
                }
            }
            entry.install?.let { job ->
                Text(job.stage.label, Modifier.padding(horizontal = 12.dp), style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.primary)
                val progress = job.progress
                if (progress == null) LinearProgressIndicator(Modifier.fillMaxWidth())
                else LinearProgressIndicator(progress = { progress.coerceIn(0f, 1f) }, modifier = Modifier.fillMaxWidth())
            }
        }
    }
}

private fun catalogSourceLabel(url: String): String {
    if (url.isBlank()) return "Catalog"
    val githubPath = url.substringAfter("githubusercontent.com/", "")
    return if (githubPath.isNotBlank()) githubPath.split('/').take(3).joinToString("/")
    else url.substringAfter("://").substringBefore('/')
}

@Composable
internal fun ColumnScope.SettingsWorkspace(presenter: CompanionPresenter, state: CompanionUiState, routeHash: String) {
    LaunchedEffect(state.isConnected) {
        if (!state.isConnected) return@LaunchedEffect
        presenter.refreshSettings()
        presenter.refreshWifiSettings()
    }

    val settings = state.settings
    if (settings == null) {
        Text(if (state.isConnected) "Loading settings…" else "Connect a Nano to edit settings.")
        return
    }

    val sections = listOf(
        "reading" to "Reading",
        "display" to "Display",
        "updates" to "Updates",
        "wifi" to "Wi-Fi",
    )
    val selected = routeHash.substringAfterLast('/').takeIf { id -> sections.any { it.first == id } } ?: "reading"
    BoxWithConstraints(Modifier.fillMaxWidth().weight(1f)) {
        if (maxWidth >= 900.dp) {
            Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(22.dp)) {
                Column(Modifier.width(170.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    sections.forEach { (id, label) ->
                        SettingsNavigationItem(label, selected == id) { window.location.hash = "#/settings/$id" }
                    }
                }
                Column(Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState())) {
                    SettingsContent(selected, presenter, state, settings)
                }
            }
        } else {
            Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Row(
                    Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    sections.forEach { (id, label) ->
                        FilterChip(
                            selected = selected == id,
                            onClick = { window.location.hash = "#/settings/$id" },
                            label = { Text(label) },
                        )
                    }
                }
                Column(Modifier.fillMaxWidth().weight(1f).verticalScroll(rememberScrollState())) {
                    SettingsContent(selected, presenter, state, settings)
                }
            }
        }
    }
}

@Composable
private fun SettingsNavigationItem(label: String, selected: Boolean, onClick: () -> Unit) {
    val shape = RoundedCornerShape(14.dp)
    Surface(
        modifier = Modifier.fillMaxWidth().clip(shape).clickable(onClick = onClick),
        shape = shape,
        color = if (selected) MaterialTheme.colorScheme.primary.copy(alpha = 0.16f) else MaterialTheme.colorScheme.surface,
        contentColor = if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
    ) {
        Text(label, Modifier.padding(horizontal = 16.dp, vertical = 13.dp), fontWeight = FontWeight.Bold)
    }
}

@Composable
private fun SettingsContent(
    selected: String,
    presenter: CompanionPresenter,
    state: CompanionUiState,
    settings: NanoSettings,
) {
    when (selected) {
        "display" -> DisplaySettings(presenter, settings)
        "updates" -> UpdateSettings(presenter, state, settings)
        "wifi" -> NetworkSettings(presenter, state)
        else -> ReadingSettings(presenter, settings)
    }
}

@Composable
private fun SettingsPageHeader(title: String, description: String) {
    Column(Modifier.fillMaxWidth().padding(bottom = 16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text(title, style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Bold)
        Text(description, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.68f))
    }
}

@Composable
private fun ReadingSettings(presenter: CompanionPresenter, settings: NanoSettings) {
    val reading = settings.reading
    SettingsPageHeader("Reading", "Tune pacing, typography, and what stays visible while you read.")
    BoxWithConstraints(Modifier.fillMaxWidth()) {
        if (maxWidth >= 760.dp) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(16.dp), verticalAlignment = Alignment.Top) {
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(16.dp)) {
                    SettingsPanel("Preview") {
                        TypographyPreview(
                            typography = reading.typography,
                            phantomWords = reading.phantomWords,
                            modifier = Modifier.fillMaxWidth().height(210.dp),
                        )
                    }
                    ReadingRhythmSettings(presenter, reading)
                    PacingSettings(presenter, reading)
                }
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(16.dp)) {
                    TypographySettings(presenter, reading)
                    ReadingVisibilitySettings(presenter, reading)
                }
            }
        } else {
            Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                SettingsPanel("Preview") {
                    TypographyPreview(
                        typography = reading.typography,
                        phantomWords = reading.phantomWords,
                        modifier = Modifier.fillMaxWidth().height(180.dp),
                    )
                }
                ReadingRhythmSettings(presenter, reading)
                TypographySettings(presenter, reading)
                PacingSettings(presenter, reading)
                ReadingVisibilitySettings(presenter, reading)
            }
        }
    }
}

@Composable
private fun ReadingRhythmSettings(presenter: CompanionPresenter, reading: NanoSettings.Reading) {
    SettingsPanel("Reading rhythm") {
        IntSlider("Words per minute", reading.wpm, NanoSettingsSchema.WPM_MIN..NanoSettingsSchema.WPM_MAX) {
            presenter.updateSettings { current -> current.withWpm(it) }
        }
        ChoiceRow("Mode", reading.mode, listOf("rsvp" to "RSVP", "page" to "Page")) { value ->
            presenter.updateSettings { it.copy(reading = it.reading.copy(mode = value)) }
        }
        ChoiceRow("Pause timing", reading.pauseMode, listOf("sentenceEnd" to "Sentence end", "instant" to "Instant")) { value ->
            presenter.updateSettings { it.withPauseMode(value) }
        }
        ToggleRow("Phantom words", reading.phantomWords) { value -> presenter.updateSettings { it.withPhantomWords(value) } }
        ToggleRow("Reverse chapter scrolling", reading.chapterScrollReversed) { value ->
            presenter.updateSettings { it.copy(reading = it.reading.copy(chapterScrollReversed = value)) }
        }
        ToggleRow("Left-handed controls", reading.leftHanded) { value ->
            presenter.updateSettings { it.withHandedness(if (value) "left" else "right") }
        }
    }
}

@Composable
private fun TypographySettings(presenter: CompanionPresenter, reading: NanoSettings.Reading) {
    SettingsPanel("Typography") {
        ToggleRow("Focus highlight", reading.typography.focusHighlight) { value ->
            presenter.updateSettings { it.withFocusHighlight(value) }
        }
        IntSlider("Font size", reading.typography.fontSizeIndex, NanoSettingsSchema.FONT_SIZE_MIN..NanoSettingsSchema.FONT_SIZE_MAX) {
            presenter.updateSettings { current -> current.withFontSizeIndex(it) }
        }
        IntSlider("Tracking", reading.typography.tracking, NanoSettingsSchema.TRACKING_MIN..NanoSettingsSchema.TRACKING_MAX) {
            presenter.updateSettings { current -> current.withTracking(it) }
        }
        IntSlider("Focus anchor", reading.typography.anchor, NanoSettingsSchema.ANCHOR_PERCENT_MIN..NanoSettingsSchema.ANCHOR_PERCENT_MAX, "%") {
            presenter.updateSettings { current -> current.withAnchorPercent(it) }
        }
        IntSlider("Guide width", reading.typography.guideWidth, NanoSettingsSchema.GUIDE_WIDTH_MIN..NanoSettingsSchema.GUIDE_WIDTH_MAX) {
            presenter.updateSettings { current -> current.withGuideWidth(it) }
        }
        IntSlider("Guide gap", reading.typography.guideGap, NanoSettingsSchema.GUIDE_GAP_MIN..NanoSettingsSchema.GUIDE_GAP_MAX) {
            presenter.updateSettings { current -> current.withGuideGap(it) }
        }
    }
}

@Composable
private fun PacingSettings(presenter: CompanionPresenter, reading: NanoSettings.Reading) {
    SettingsPanel("Word timing") {
        IntSlider("Long word delay", reading.pacing.longWordDelayMs, NanoSettingsSchema.PACING_MS_MIN..NanoSettingsSchema.PACING_MS_MAX, " ms") {
            presenter.updateSettings { current -> current.withPacingLongWordMs(it) }
        }
        IntSlider("Complex word delay", reading.pacing.complexWordDelayMs, NanoSettingsSchema.PACING_MS_MIN..NanoSettingsSchema.PACING_MS_MAX, " ms") {
            presenter.updateSettings { current -> current.withPacingComplexWordMs(it) }
        }
        IntSlider("Punctuation delay", reading.pacing.punctuationDelayMs, NanoSettingsSchema.PACING_MS_MIN..NanoSettingsSchema.PACING_MS_MAX, " ms") {
            presenter.updateSettings { current -> current.withPacingPunctuationMs(it) }
        }
    }
}

@Composable
private fun ReadingVisibilitySettings(presenter: CompanionPresenter, reading: NanoSettings.Reading) {
    SettingsPanel("Reading screen") {
        ChoiceRow("Footer", reading.footerMetric, listOf("percentage" to "Percentage", "chapterTime" to "Chapter time", "bookTime" to "Book time")) { value ->
            presenter.updateSettings { it.withFooterMetric(value) }
        }
        ChoiceRow("Battery label", reading.batteryLabel, listOf("percentage" to "Percentage", "timeRemaining" to "Time left", "voltage" to "Voltage")) { value ->
            presenter.updateSettings { it.withBatteryLabel(value) }
        }
        ToggleRow("Battery icon", reading.batteryIconVisible) { value -> presenter.updateSettings { it.withBatteryIconVisible(value) } }
        ToggleRow("Battery while reading", reading.batteryVisibleWhileReading) { value -> presenter.updateSettings { it.withReadingBattery(value) } }
        ToggleRow("Chapter while reading", reading.chapterVisibleWhileReading) { value -> presenter.updateSettings { it.withReadingChapter(value) } }
        ToggleRow("Progress while reading", reading.progressVisibleWhileReading) { value -> presenter.updateSettings { it.withReadingProgress(value) } }
    }
}

@Composable
private fun DisplaySettings(presenter: CompanionPresenter, settings: NanoSettings) {
    val display = settings.`interface`
    SettingsPageHeader("Display", "Choose how the Nano looks when it is active or waiting.")
    BoxWithConstraints(Modifier.fillMaxWidth()) {
        val content: @Composable () -> Unit = {
            SettingsPanel("Screen") {
                IntSlider("Brightness", display.brightnessPercent, NanoSettingsSchema.BRIGHTNESS_MIN..NanoSettingsSchema.BRIGHTNESS_MAX, "%") {
                    presenter.updateSettings { current -> current.withBrightnessPercent(it) }
                }
                ChoiceRow("Standby", display.standbyTimerIndex.toString(), listOf("0" to "Never", "1" to "1 minute", "2" to "5 minutes", "3" to "15 minutes", "4" to "30 minutes")) { value ->
                    presenter.updateSettings { it.withStandbyTimerIndex(value.toInt()) }
                }
            }
        }
        val screensaver: @Composable () -> Unit = {
            SettingsPanel("Screensaver") {
                ChoiceRow("Animation", display.screensaver, listOf("life" to "Life", "maze" to "Maze", "voronoi" to "Voronoi", "reaction" to "Reaction", "screenOff" to "Screen off")) { value ->
                    presenter.updateSettings { it.withScreensaver(value) }
                }
            }
        }
        if (maxWidth >= 680.dp) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(16.dp), verticalAlignment = Alignment.Top) {
                Column(Modifier.weight(1f)) { content() }
                Column(Modifier.weight(1f)) { screensaver() }
            }
        } else {
            Column(verticalArrangement = Arrangement.spacedBy(14.dp)) { content(); screensaver() }
        }
    }
}

@Composable
private fun UpdateSettings(presenter: CompanionPresenter, state: CompanionUiState, settings: NanoSettings) {
    var ownerDraft by remember(settings.updates.repositoryOwner) { mutableStateOf(settings.updates.repositoryOwner) }
    var tagDraft by remember(settings.updates.releaseTag) { mutableStateOf(settings.updates.releaseTag) }
    SettingsPageHeader("Updates", "Control automatic checks and the release source used by this Nano.")
    ResponsiveSettingsColumns(
        first = {
            SettingsPanel("Checks") {
                ToggleRow("Check on reader startup", settings.updates.checkOnStartup) { value ->
                    presenter.updateSettings { it.withUpdateChecksOnStartup(value) }
                }
                ToggleRow("Browser update notifications", state.firmwareNotificationsEnabled, presenter::setFirmwareNotificationsEnabled)
            }
        },
        second = {
            SettingsPanel("Release source") {
                OutlinedTextField(
                    value = ownerDraft,
                    onValueChange = { ownerDraft = it },
                    label = { Text("Repository owner or owner/repository") },
                    modifier = Modifier.fillMaxWidth(),
                    singleLine = true,
                )
                OutlinedTextField(
                    value = tagDraft,
                    onValueChange = { tagDraft = it },
                    label = { Text("Release tag") },
                    modifier = Modifier.fillMaxWidth(),
                    singleLine = true,
                )
                Text(
                    "Leave both blank to use the official RSVP Nano releases.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.68f),
                )
                Button(
                    onClick = {
                        presenter.updateSettings {
                            it.withUpdateOwner(ownerDraft.trim()).withUpdateTag(tagDraft.trim())
                        }
                    },
                    enabled = ownerDraft.trim() != settings.updates.repositoryOwner ||
                        tagDraft.trim() != settings.updates.releaseTag,
                ) { Text("Save source") }
            }
        },
    )
}

@Composable
private fun NetworkSettings(presenter: CompanionPresenter, state: CompanionUiState) {
    SettingsPageHeader("Wi-Fi", "Save the network the Nano should use away from USB.")
    ResponsiveSettingsColumns(
        first = {
            SettingsPanel("Current network") {
                Text(state.wifiSettings?.ssid?.ifBlank { "No network saved" } ?: "Loading network…", style = MaterialTheme.typography.titleMedium)
                Text("The password stays on the Nano and is never shown here.", color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.68f))
                OutlinedButton(onClick = presenter::clearWifiSettings) { Text("Forget network") }
            }
        },
        second = {
            SettingsPanel("Connect the Nano") {
                OutlinedTextField(state.wifiSsidDraft, presenter::setWifiSsidDraft, Modifier.fillMaxWidth(), label = { Text("Network name") }, singleLine = true)
                OutlinedTextField(state.wifiPasswordDraft, presenter::setWifiPasswordDraft, Modifier.fillMaxWidth(), label = { Text("Password") }, singleLine = true)
                Button(onClick = presenter::saveWifiSettings) { Text("Save Wi-Fi") }
            }
        },
    )
}

@Composable
private fun ResponsiveSettingsColumns(first: @Composable () -> Unit, second: @Composable () -> Unit) {
    BoxWithConstraints(Modifier.fillMaxWidth()) {
        if (maxWidth >= 620.dp) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(16.dp), verticalAlignment = Alignment.Top) {
                Column(Modifier.weight(1f)) { first() }
                Column(Modifier.weight(1f)) { second() }
            }
        } else {
            Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                first()
                second()
            }
        }
    }
}

@Composable
private fun SettingsPanel(
    title: String,
    modifier: Modifier = Modifier,
    content: @Composable ColumnScope.() -> Unit,
) {
    val shape = RoundedCornerShape(18.dp)
    Surface(
        modifier = modifier.fillMaxWidth().border(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.22f), shape),
        shape = shape,
        color = MaterialTheme.colorScheme.surface,
    ) {
        Column(Modifier.fillMaxWidth().padding(18.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text(title, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
            content()
        }
    }
}

@Composable
internal fun ColumnScope.FeedsWorkspace(presenter: CompanionPresenter, state: CompanionUiState) {
    LaunchedEffect(state.isConnected) {
        if (!state.isConnected) return@LaunchedEffect
        presenter.refreshRssFeeds()
    }
    Column(Modifier.fillMaxWidth().weight(1f).verticalScroll(rememberScrollState())) {
        SectionCard {
            Text("RSS feeds", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(state.rssFeedDraft, presenter::setRssFeedDraft, Modifier.weight(1f), label = { Text("Feed URL") }, singleLine = true)
                Button(onClick = presenter::addRssFeed) { Text("Add") }
            }
            state.rssFeeds.forEach { feed ->
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Text(feed, Modifier.weight(1f))
                    OutlinedButton(onClick = { presenter.deleteRssFeed(feed) }) { Text("Remove") }
                }
            }
            OutlinedButton(onClick = presenter::refreshRssFeeds) { Text("Fetch latest articles") }
        }
    }
}

@Composable
internal fun ColumnScope.TimersWorkspace(presenter: CompanionPresenter, state: CompanionUiState) {
    LaunchedEffect(state.isConnected) {
        if (state.isConnected) presenter.refreshFocusTimers()
    }
    Column(Modifier.fillMaxWidth().weight(1f).verticalScroll(rememberScrollState())) {
        FocusEditor(presenter, state)
    }
}

@Composable
private fun FocusEditor(presenter: CompanionPresenter, state: CompanionUiState) {
    var timers by remember(state.focusTimers) { mutableStateOf(state.focusTimers.timers) }
    SectionCard {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Text("Focus routines", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
            OutlinedButton(
                onClick = { timers = timers + NanoFocusTimer(name = "Routine ${timers.size + 1}") },
                enabled = timers.size < NanoFocusTimerRules.MAX_TIMERS,
            ) { Text("Add routine") }
        }
        timers.forEachIndexed { index, timer ->
            Column(Modifier.fillMaxWidth().padding(vertical = 8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    timer.name,
                    { value -> timers = timers.toMutableList().also { it[index] = timer.copy(name = value) } },
                    Modifier.fillMaxWidth(),
                    label = { Text("Routine name") },
                    singleLine = true,
                )
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    NumberField("Focus min", timer.focusMinutes, Modifier.weight(1f)) { value ->
                        timers = timers.toMutableList().also { it[index] = timer.copy(focusMinutes = value) }
                    }
                    NumberField("Break min", timer.breakMinutes, Modifier.weight(1f)) { value ->
                        timers = timers.toMutableList().also { it[index] = timer.copy(breakMinutes = value) }
                    }
                    NumberField("Rounds", timer.rounds, Modifier.weight(1f)) { value ->
                        timers = timers.toMutableList().also { it[index] = timer.copy(rounds = value) }
                    }
                }
                OutlinedButton(onClick = { timers = timers.filterIndexed { itemIndex, _ -> itemIndex != index } }) { Text("Remove routine") }
            }
        }
        Button(
            onClick = { presenter.saveFocusTimers(NanoFocusTimers(timers)) },
            enabled = timers.all(NanoFocusTimerRules::valid),
        ) { Text("Save routines") }
    }
}

@Composable
private fun IntSlider(label: String, value: Int, range: IntRange, suffix: String = "", onChange: (Int) -> Unit) {
    var sliderValue by remember(value) { mutableStateOf(value) }
    Column {
        DetailRow(label, "$sliderValue$suffix")
        Slider(
            value = sliderValue.toFloat(),
            onValueChange = { sliderValue = it.toInt() },
            onValueChangeFinished = { onChange(sliderValue) },
            valueRange = range.first.toFloat()..range.last.toFloat(),
        )
    }
}

@Composable
private fun ChoiceRow(label: String, selected: String, choices: List<Pair<String, String>>, onSelect: (String) -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Text(label, fontWeight = FontWeight.Bold)
        Row(
            Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            choices.forEach { (value, text) -> FilterChip(selected == value, { onSelect(value) }, { Text(text) }) }
        }
    }
}

@Composable
private fun ToggleRow(label: String, checked: Boolean, onCheckedChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(label, Modifier.weight(1f))
        Switch(checked, onCheckedChange)
    }
}

@Composable
private fun NumberField(label: String, value: Int, modifier: Modifier = Modifier, onChange: (Int) -> Unit) {
    OutlinedTextField(
        value = value.toString(),
        onValueChange = { text -> text.toIntOrNull()?.let(onChange) },
        modifier = modifier,
        label = { Text(label) },
        singleLine = true,
    )
}

@Composable
private fun SectionCard(modifier: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    Card(modifier, colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)) {
        Column(Modifier.fillMaxWidth().padding(18.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            content()
        }
    }
}
