import SwiftUI
import shared

struct LibraryPage: View {
    @ObservedObject var viewModel: LibraryViewModel
    @ObservedObject var inboxViewModel: InboxViewModel
    var onShowUpload: () -> Void
    @ObservedObject var connection: NanoConnectionManager = .shared
    @State private var searchText = ""
    @State private var filter = LibraryFilter.all
    @State private var selectedBook: NanoBook?

    var body: some View {
        libraryList
            .sheet(item: $selectedBook) { book in
                LibraryBookDetail(book: book) {
                    selectedBook = nil
                    viewModel.deleteBooks([book])
                }
            }
    }

    private var libraryList: some View {
        List {
            if let info = connection.info {
                Section {
                    VStack(alignment: .leading, spacing: 8) {
                        Text(info.name)
                            .font(.title2.weight(.semibold))
                        Text(viewModel.librarySummary)
                            .foregroundStyle(.secondary)
                        Text(info.networkSsid ?? "Connected to reader")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                    .padding(.vertical, 4)
                }
            }

            let visibleItems = filteredBooks
            let visibleDrafts = filteredDrafts

            Section {
                Button {
                    onShowUpload()
                } label: {
                    HStack(spacing: 10) {
                        Image(systemName: "doc.badge.plus")
                            .foregroundStyle(.green)
                        Text("Upload")
                            .font(.body.weight(.semibold))
                            .foregroundStyle(.primary)
                        Spacer(minLength: 0)
                    }
                    .padding(.vertical, 2)
                }
                .listRowBackground(Color.green.opacity(0.14))
            }

            Section {
                TextField("Search library", text: $searchText)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled()

                Picker("Filter", selection: $filter) {
                    ForEach(LibraryFilter.allCases) { item in
                        Text(item.title).tag(item)
                    }
                }
                .pickerStyle(.segmented)
            }

            pendingArticlesSection(visibleDrafts)
            libraryItemsSection(visibleItems)
        }
        .listStyle(.insetGrouped)
        .refreshable {
            await inboxViewModel.refreshPendingUploads()
            if connection.isConnected {
                await viewModel.refreshBooks()
            }
        }
    }

    @ViewBuilder
    private func pendingArticlesSection(_ visibleDrafts: [PendingUpload]) -> some View {
        if !visibleDrafts.isEmpty {
            Section("Pending Articles") {
                ForEach(visibleDrafts) { item in
                    PendingArticleLibraryRow(
                        item: item,
                        canSync: connection.isConnected && !connection.isBusy && !item.needsArticleFetch
                    ) {
                        inboxViewModel.editingArticle = item
                    } onDelete: {
                        inboxViewModel.deletePendingUpload(item)
                    } onSync: {
                        inboxViewModel.syncPendingUpload(item)
                    }
                }

                Button {
                    inboxViewModel.syncPendingUploads()
                } label: {
                    Label("Sync Ready Articles", systemImage: "arrow.up.doc")
                }
                .buttonStyle(.borderedProminent)
                .disabled(!connection.isConnected || connection.isBusy || !inboxViewModel.pendingUploads.contains { !$0.needsArticleFetch })
            }
        }
    }

    @ViewBuilder
    private func libraryItemsSection(_ visibleItems: [NanoBook]) -> some View {
        Section("Library") {
            if !connection.isConnected {
                Text("Connect to the reader to load its library.")
                    .foregroundStyle(.secondary)
            } else if viewModel.books.isEmpty {
                VStack(alignment: .leading, spacing: 6) {
                    Text("No library items reported yet.")
                    Text("Upload books here. Synced articles and RSS downloads also appear in this library.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            } else if visibleItems.isEmpty {
                Text("No matching items.")
                    .foregroundStyle(.secondary)
            } else {
                ForEach(visibleItems) { book in
                    Button {
                        selectedBook = book
                    } label: {
                        LibraryBookRow(book: book)
                    }
                    .buttonStyle(.plain)
                }
                .onDelete { offsets in
                    viewModel.deleteBooks(offsets.map { visibleItems[$0] })
                }
            }
        }
    }

    private var filteredBooks: [NanoBook] {
        viewModel.books.filter { book in
            let matchesFilter: Bool
            switch filter {
            case .all:
                matchesFilter = true
            case .books:
                matchesFilter = !book.isArticle
            case .articles:
                matchesFilter = book.isArticle
            }

            let query = searchText.trimmingCharacters(in: .whitespacesAndNewlines)
            let matchesSearch = query.isEmpty ||
                book.displayTitle.localizedCaseInsensitiveContains(query) ||
                (book.author ?? "").localizedCaseInsensitiveContains(query) ||
                book.id.localizedCaseInsensitiveContains(query)

            return matchesFilter && matchesSearch
        }
    }

    private var filteredDrafts: [PendingUpload] {
        guard filter != .books else { return [] }
        return inboxViewModel.pendingUploads.filter { item in
            let query = searchText.trimmingCharacters(in: .whitespacesAndNewlines)
            return query.isEmpty ||
                item.title.localizedCaseInsensitiveContains(query) ||
                (item.sourceUrl ?? "").localizedCaseInsensitiveContains(query)
        }
    }
}

private enum LibraryFilter: String, CaseIterable, Identifiable {
    case all
    case books
    case articles

    var id: String { rawValue }

    var title: String {
        switch self {
        case .all: return "All"
        case .books: return "Books"
        case .articles: return "Articles"
        }
    }
}

struct LibraryBookRow: View {
    let book: NanoBook

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(alignment: .firstTextBaseline, spacing: 10) {
                Image(systemName: book.isArticle ? "newspaper" : "book.closed")
                    .foregroundStyle(book.isArticle ? .orange : .secondary)
                VStack(alignment: .leading, spacing: 3) {
                    Text(book.displayTitle)
                        .foregroundStyle(.primary)
                    if !book.detailLabel.isEmpty {
                        Text(book.detailLabel)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                if let progressPercent = book.progressPercent {
                    Text("\(max(0, min(100, progressPercent)))%")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.secondary)
                }
            }
            if let progressPercent = book.progressPercent {
                ProgressView(value: Double(max(0, min(100, progressPercent))), total: 100)
            }
        }
    }
}

private struct PendingArticleLibraryRow: View {
    let item: PendingUpload
    let canSync: Bool
    var onEdit: () -> Void
    var onDelete: () -> Void
    var onSync: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(alignment: .firstTextBaseline, spacing: 10) {
                Image(systemName: "newspaper")
                    .foregroundStyle(item.needsArticleFetch ? .orange : .accentColor)
                VStack(alignment: .leading, spacing: 3) {
                    Text(item.title)
                        .foregroundStyle(.primary)
                    Text(detailLabel)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            }

            HStack(spacing: 10) {
                Button {
                    onEdit()
                } label: {
                    Label("Edit", systemImage: "pencil")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)

                Button {
                    onSync()
                } label: {
                    Label("Sync", systemImage: "arrow.up.doc")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .disabled(!canSync)

                Button(role: .destructive) {
                    onDelete()
                } label: {
                    Image(systemName: "trash")
                }
                .buttonStyle(.bordered)
            }
        }
        .padding(.vertical, 4)
    }

    private var detailLabel: String {
        let size = ByteCountFormatter.string(fromByteCount: Int64(item.bytes), countStyle: .file)
        var parts = [item.needsArticleFetch ? "Needs article text" : "Ready to sync", size]
        if let sourceUrl = item.sourceUrl, let host = URL(string: sourceUrl)?.host, !host.isEmpty {
            parts.append(host)
        }
        return parts.joined(separator: " · ")
    }
}

private struct LibraryBookDetail: View {
    let book: NanoBook
    var onDelete: () -> Void
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                Section {
                    LabeledContent("Title", value: book.displayTitle)
                    if let author = book.author, !author.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                        LabeledContent("Author", value: author)
                    }
                    LabeledContent("Size", value: book.byteLabel)
                    LabeledContent("Path", value: book.id)
                    LabeledContent("Type", value: book.isArticle ? "Article" : "Book")
                    if let progress = book.progressPercent {
                        LabeledContent("Progress", value: "\(max(0, min(100, progress)))%")
                    }
                }

                Section {
                    Button(role: .destructive) {
                        onDelete()
                    } label: {
                        Label("Delete", systemImage: "trash")
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(.red)
                }
            }
            .navigationTitle(book.displayTitle)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") {
                        dismiss()
                    }
                }
            }
        }
    }
}
