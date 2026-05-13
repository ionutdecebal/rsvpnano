import SwiftUI
import UIKit

@MainActor
final class NanoViewModel: ObservableObject {
    @Published var address = "http://192.168.4.1"
    @Published var info: NanoInfo?
    @Published var books: [NanoBook] = []
    @Published var pendingUploads: [PendingUpload] = []
    @Published var status = "Waiting for RSVP Nano Wi-Fi."
    @Published var isBusy = false
    @Published var showingPicker = false
    @Published var showingTextImport = false
    @Published var hasAttemptedConnection = false
    @Published var lastConnectionError: String?

    private var autoConnectTask: Task<Void, Never>?

    var canUpload: Bool {
        info != nil && !isBusy
    }

    var isConnected: Bool {
        info != nil
    }

    var canSyncPending: Bool {
        isConnected && !isBusy && !pendingUploads.isEmpty
    }

    var librarySummary: String {
        let bookCount = books.count
        let bookLabel = bookCount == 1 ? "book" : "books"
        let knownProgressCount = books.filter { $0.progressPercent != nil }.count
        if knownProgressCount > 0 {
            return "\(bookCount) \(bookLabel) in /books · \(knownProgressCount) with saved progress"
        }
        return "\(bookCount) \(bookLabel) in /books"
    }

    func startAutoConnect() {
        refreshPendingUploads()
        guard autoConnectTask == nil else {
            return
        }

        autoConnectTask = Task { [weak self] in
            while !Task.isCancelled {
                guard let self else {
                    return
                }
                if await self.connectOnce(showBusy: false) {
                    self.autoConnectTask = nil
                    return
                }
                try? await Task.sleep(for: .seconds(2))
            }
        }
    }

    func stopAutoConnect() {
        autoConnectTask?.cancel()
        autoConnectTask = nil
    }

    func connect(showBusy: Bool = true) {
        Task {
            _ = await connectOnce(showBusy: showBusy)
        }
    }

    func refreshBooks() {
        Task {
            await run("Refreshing") { [self] in
                self.books = try await NanoClient(baseURLString: self.address).fetchBooks()
                self.refreshPendingUploads()
            self.status = "Library refreshed from the SD card."
            }
        }
    }

    func upload(_ file: PickedBookFile) {
        Task {
            await run("Preparing \(file.filename)") { [self] in
                do {
                    let converted = try RsvpConverter.bookFile(data: file.data, filename: file.filename)
                    try await self.uploadConverted(converted)
                } catch RsvpConversionError.unsupportedEpub where file.filename.lowercased().hasSuffix(".epub") {
                    let raw = RsvpBookFile(
                        filename: file.filename,
                        data: file.data,
                        title: RsvpConverter.filenameWithoutExtension(file.filename)
                    )
                    try await self.uploadConverted(raw)
                }
            }
        }
    }

    func upload(_ file: RsvpBookFile) {
        Task {
            await run("Uploading \(file.title)") { [self] in
                try await self.uploadConverted(file)
            }
        }
    }

    func deleteBooks(at offsets: IndexSet) {
        let booksToDelete = offsets.map { books[$0] }
        Task {
            await run(booksToDelete.count == 1 ? "Deleting \(booksToDelete[0].displayTitle)" : "Deleting books") { [self] in
                let client = NanoClient(baseURLString: self.address)
                for book in booksToDelete {
                    _ = try await client.deleteBook(named: book.name)
                }
                self.books = try await client.fetchBooks()
                self.status = booksToDelete.count == 1 ? "Deleted \(booksToDelete[0].displayTitle)." : "Deleted books."
            }
        }
    }

    func refreshPendingUploads() {
        do {
            pendingUploads = try PendingUploadStore().all()
        } catch {
            lastConnectionError = error.localizedDescription
        }
    }

    func syncPendingUploads() {
        let items = pendingUploads
        Task {
            await run(items.count == 1 ? "Syncing \(items[0].title)" : "Syncing saved articles") { [self] in
                for item in items {
                    try await self.uploadPendingItem(item)
                }
                self.pendingUploads = try PendingUploadStore().all()
                self.books = try await NanoClient(baseURLString: self.address).fetchBooks()
                self.status = items.count == 1 ? "Synced \(items[0].title)." : "Synced saved articles."
            }
        }
    }

    func syncPendingUpload(_ item: PendingUpload) {
        Task {
            await run("Syncing \(item.title)") { [self] in
                try await self.uploadPendingItem(item)
                self.pendingUploads = try PendingUploadStore().all()
                self.books = try await NanoClient(baseURLString: self.address).fetchBooks()
                self.status = "Synced \(item.title)."
            }
        }
    }

    func deletePendingUploads(at offsets: IndexSet) {
        do {
            try PendingUploadStore().delete(at: offsets)
            refreshPendingUploads()
        } catch {
            lastConnectionError = error.localizedDescription
        }
    }

    @discardableResult
    private func connectOnce(showBusy: Bool = true) async -> Bool {
        await run("Looking for RSVP Nano", showBusy: showBusy) { [self] in
            self.hasAttemptedConnection = true
            let client = NanoClient(baseURLString: self.address)
            self.info = try await client.fetchInfo()
            self.books = try await client.fetchBooks()
            self.refreshPendingUploads()
            self.lastConnectionError = nil
            self.status = "Connected to \(self.info?.name ?? "RSVP Nano"). Reading /books."
        }
        return isConnected
    }

    private func uploadConverted(_ file: RsvpBookFile) async throws {
        let client = NanoClient(baseURLString: self.address)
        _ = try await client.uploadBook(data: file.data, filename: file.filename)
        self.books = try await client.fetchBooks()
        self.status = uploadStatus(for: file)
    }

    private func uploadPendingItem(_ item: PendingUpload) async throws {
        let data = try PendingUploadStore().data(for: item)
        _ = try await NanoClient(baseURLString: self.address).uploadBook(data: data, filename: item.filename)
        try PendingUploadStore().delete(item)
    }

    private func run(_ busyStatus: String, showBusy: Bool = true, operation: @escaping () async throws -> Void) async {
        if showBusy {
            isBusy = true
        }
        status = busyStatus
        do {
            try await operation()
        } catch {
            lastConnectionError = error.localizedDescription
            status = "Still waiting for RSVP Nano Wi-Fi."
        }
        if showBusy {
            isBusy = false
        }
    }

    private func uploadStatus(for file: RsvpBookFile) -> String {
        guard file.wordCount > 0 else {
            return "Uploaded \(file.title) into /books."
        }
        let wordLabel = file.wordCount == 1 ? "word" : "words"
        let chapterLabel = file.chapterCount == 1 ? "chapter" : "chapters"
        return "Uploaded \(file.title) into /books: \(file.wordCount) \(wordLabel), \(file.chapterCount) \(chapterLabel)."
    }
}

struct ContentView: View {
    @StateObject private var viewModel = NanoViewModel()

    var body: some View {
        NavigationStack {
            content
            .navigationTitle("RSVP Nano")
            .safeAreaInset(edge: .bottom) {
                statusBar
            }
            .sheet(isPresented: $viewModel.showingPicker) {
                BookDocumentPicker { file in
                    viewModel.showingPicker = false
                    viewModel.upload(file)
                } onCancel: {
                    viewModel.showingPicker = false
                }
            }
            .sheet(isPresented: $viewModel.showingTextImport) {
                TextImportView { file in
                    viewModel.showingTextImport = false
                    viewModel.upload(file)
                } onCancel: {
                    viewModel.showingTextImport = false
                }
            }
            .task {
                viewModel.startAutoConnect()
            }
            .onReceive(NotificationCenter.default.publisher(for: UIApplication.willEnterForegroundNotification)) { _ in
                viewModel.refreshPendingUploads()
            }
            .onDisappear {
                viewModel.stopAutoConnect()
            }
            .toolbar {
                if viewModel.isConnected {
                    EditButton()
                }
            }
        }
    }

    @ViewBuilder
    private var content: some View {
        if viewModel.isConnected {
            libraryList
        } else {
            connectInstructions
        }
    }

    private var connectInstructions: some View {
        List {
            Section {
                VStack(alignment: .leading, spacing: 16) {
                    Label("Open Companion sync on the reader", systemImage: "1.circle")
                        .font(.headline)
                    Text("On RSVP Nano, insert the SD card, open the main menu, and choose Companion sync.")
                        .foregroundStyle(.secondary)

                    Label("Join the reader Wi-Fi", systemImage: "2.circle")
                        .font(.headline)
                    Text("On this iPhone, open Settings -> Wi-Fi and join the network shown on the reader. It starts with RSVP-Nano.")
                        .foregroundStyle(.secondary)

                    Label("Return here", systemImage: "3.circle")
                        .font(.headline)
                    Text("The app will detect the reader automatically and show the SD card library from /books.")
                        .foregroundStyle(.secondary)
                }
                .padding(.vertical, 8)
            }

            Section("SD Card") {
                VStack(alignment: .leading, spacing: 6) {
                    Label("Use a simple FAT32 card", systemImage: "sdcard")
                    Text("A reliable 8-32 GB microSD card is the safest boring choice. 64 GB cards can work well too when formatted as FAT32; avoid exFAT for this reader.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("Create /books at the root", systemImage: "folder")
                    Text("The reader scans /books for .rsvp, .txt, and .epub files. This app uploads converted books there automatically.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("If it does not show up", systemImage: "exclamationmark.triangle")
                    Text("The usual causes are exFAT formatting, a missing /books folder, the card not being seated fully, a tired or counterfeit card, or files with unsupported extensions.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }

            Section {
                HStack {
                    ProgressView()
                    Text(viewModel.status)
                        .foregroundStyle(.secondary)
                }

                Button {
                    viewModel.connect()
                } label: {
                    Label("Check Again", systemImage: "arrow.clockwise")
                }
                .disabled(viewModel.isBusy)

                if viewModel.hasAttemptedConnection, let error = viewModel.lastConnectionError {
                    Text(error)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }

    private var libraryList: some View {
        List {
            if let info = viewModel.info {
                Section("Reader") {
                    LabeledContent("Name", value: info.name)
                    LabeledContent("Wi-Fi", value: info.networkSsid ?? "Connected")
                    LabeledContent("Library", value: viewModel.librarySummary)
                }
            }

            if !viewModel.pendingUploads.isEmpty {
                Section("Saved Articles") {
                    ForEach(viewModel.pendingUploads) { item in
                        Button {
                            viewModel.syncPendingUpload(item)
                        } label: {
                            VStack(alignment: .leading, spacing: 4) {
                                Text(item.title)
                                    .foregroundStyle(.primary)
                                Text(pendingDetailLabel(for: item))
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        }
                        .disabled(!viewModel.canUpload)
                    }
                    .onDelete(perform: viewModel.deletePendingUploads)

                    Button {
                        viewModel.syncPendingUploads()
                    } label: {
                        Label("Sync Saved Articles", systemImage: "arrow.up.doc")
                    }
                    .disabled(!viewModel.canSyncPending)
                }
            }

            Section("Library") {
                if viewModel.books.isEmpty {
                    VStack(alignment: .leading, spacing: 6) {
                        Text("No books reported yet.")
                        Text("Upload a book here after the SD card has a /books folder. If the list stays empty, check the card format and seating.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                } else {
                    ForEach(viewModel.books) { book in
                        VStack(alignment: .leading, spacing: 8) {
                            HStack(alignment: .firstTextBaseline, spacing: 10) {
                                Text(book.displayTitle)
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
                            if !book.detailLabel.isEmpty {
                                Text(book.detailLabel)
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                    .onDelete(perform: viewModel.deleteBooks)
                }
            }

            Section {
                Button {
                    viewModel.showingPicker = true
                } label: {
                    Label("Upload File", systemImage: "doc.badge.plus")
                }
                .disabled(!viewModel.canUpload)

                Button {
                    viewModel.showingTextImport = true
                } label: {
                    Label("New Text", systemImage: "text.badge.plus")
                }
                .disabled(!viewModel.canUpload)

                Button {
                    viewModel.refreshBooks()
                } label: {
                    Label("Refresh Library", systemImage: "arrow.clockwise")
                }
                .disabled(viewModel.info == nil || viewModel.isBusy)
            }

            Section("Prepare SD Card") {
                VStack(alignment: .leading, spacing: 6) {
                    Label("Recommended card", systemImage: "sdcard")
                    Text("Use a known-good microSD card. 8-32 GB is the most conservative range, and 64 GB cards can work well when formatted as FAT32 with a single partition.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("Best file format", systemImage: "doc.text")
                    Text("The app uploads .rsvp files when it can. The reader can also see .txt and .epub files in /books, but .rsvp is the most predictable format.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("Uploads are atomic", systemImage: "checkmark.seal")
                    Text("A book is written as a temporary file first, then renamed into place when the upload completes.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("Leave sync to refresh", systemImage: "arrow.triangle.2.circlepath")
                    Text("After uploading, hold BOOT on the reader to exit Companion sync and refresh the on-device library.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }

            Section("If The Card Fails") {
                VStack(alignment: .leading, spacing: 6) {
                    Label("Check formatting", systemImage: "exclamationmark.triangle")
                    Text("If the reader cannot mount the card, exFAT or a multi-partition layout is the most likely cause. Reformat as FAT32 and try again.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("Check the basics", systemImage: "magnifyingglass")
                    Text("Make sure the card is fully seated, /books exists, filenames are not hidden, and at least one supported book file is present.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("Try another card", systemImage: "arrow.uturn.forward")
                    Text("Intermittent mounts or failed writes usually point to a worn, counterfeit, or marginal card. A smaller brand-name card is often more reliable.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }

    private var statusBar: some View {
        HStack(spacing: 10) {
            if viewModel.isBusy {
                ProgressView()
            }
            Text(viewModel.status)
                .font(.footnote)
                .foregroundStyle(.secondary)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding(.horizontal)
        .padding(.vertical, 10)
        .background(.bar)
    }

    private func pendingDetailLabel(for item: PendingUpload) -> String {
        let size = ByteCountFormatter.string(fromByteCount: Int64(item.bytes), countStyle: .file)
        if item.source.isEmpty {
            return size
        }
        return "\(item.source) · \(size)"
    }
}
