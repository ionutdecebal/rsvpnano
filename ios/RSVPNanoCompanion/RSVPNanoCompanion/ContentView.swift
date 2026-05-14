import SwiftUI
import UIKit

@MainActor
final class NanoViewModel: ObservableObject {
    private let rssFeedStore = LocalRssFeedStore()

    @Published var address = "http://192.168.4.1"
    @Published var info: NanoInfo?
    @Published var books: [NanoBook] = []
    @Published var deviceSettings: NanoSettings?
    @Published var wifiSettings: NanoWifiSettings?
    @Published var wifiSsidDraft = ""
    @Published var wifiPasswordDraft = ""
    @Published var rssFeeds: [String] = []
    @Published var syncedRssFeeds: [String] = []
    @Published var rssFeedDraft = ""
    @Published var pendingUploads: [PendingUpload] = []
    @Published var status = "Waiting for RSVP Nano Wi-Fi."
    @Published var isBusy = false
    @Published var showingPicker = false
    @Published var showingTextImport = false
    @Published var editingArticle: PendingUpload?
    @Published var hasAttemptedConnection = false
    @Published var lastConnectionError: String?

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
        let articleCount = books.filter(\.isArticle).count
        let bookCount = books.count - articleCount
        let bookLabel = bookCount == 1 ? "book" : "books"
        let articleLabel = articleCount == 1 ? "article" : "articles"
        let knownProgressCount = books.filter { $0.progressPercent != nil }.count
        let base = "\(bookCount) \(bookLabel) · \(articleCount) \(articleLabel)"
        if knownProgressCount > 0 {
            return "\(base) · \(knownProgressCount) with saved progress"
        }
        return base
    }

    func startAutoConnect() {
        refreshPendingUploads()
        rssFeeds = rssFeedStore.load()
    }

    func stopAutoConnect() {
    }

    func connect(showBusy: Bool = true) {
        Task {
            _ = await connectOnce(showBusy: showBusy)
        }
    }

    func refreshBooks() {
        Task {
            await run("Refreshing") { [self] in
                let client = NanoClient(baseURLString: self.address)
                do {
                    self.books = try await client.fetchBooks()
                } catch {
                    self.lastConnectionError = "Library refresh failed: \(error.localizedDescription)"
                    self.status = "Connected, but the library could not be read."
                    return
                }
                self.deviceSettings = try? await client.fetchSettings()
                if let wifi = try? await client.fetchWifiSettings() {
                    self.applyWifiSettings(wifi)
                }
                if let deviceFeeds = try? await client.fetchRssFeeds().feeds {
                    self.mergeRssFeedsFromDevice(deviceFeeds)
                }
                self.refreshPendingUploads()
                self.status = "Library refreshed from the SD card."
            }
        }
    }

    func refreshSettings() {
        Task {
            await run("Reading settings") { [self] in
                let client = NanoClient(baseURLString: self.address)
                self.deviceSettings = try await client.fetchSettings()
                if let wifi = try? await client.fetchWifiSettings() {
                    self.applyWifiSettings(wifi)
                }
                self.status = "Device settings refreshed."
            }
        }
    }

    func saveSettings(_ settings: NanoSettings) {
        Task {
            await run("Saving settings") { [self] in
                var next = settings
                next.reading.accurateTimeEstimate = true
                self.deviceSettings = try await NanoClient(baseURLString: self.address).updateSettings(next)
                self.status = "Device settings saved. Exit sync on the reader to apply all changes."
            }
        }
    }

    func refreshWifiSettings() {
        Task {
            await run("Reading Wi-Fi settings") { [self] in
                self.applyWifiSettings(try await NanoClient(baseURLString: self.address).fetchWifiSettings())
                self.status = "Wi-Fi settings refreshed."
            }
        }
    }

    func saveWifiSettings() {
        let ssid = wifiSsidDraft.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !ssid.isEmpty else {
            lastConnectionError = "Enter a Wi-Fi network name first."
            status = "Wi-Fi was not saved."
            return
        }
        Task {
            await run("Saving Wi-Fi") { [self] in
                let wifi = try await NanoClient(baseURLString: self.address)
                    .updateWifi(ssid: ssid, password: self.wifiPasswordDraft)
                self.applyWifiSettings(wifi)
                self.status = "Wi-Fi saved for RSS and OTA."
            }
        }
    }

    func forgetWifiSettings() {
        Task {
            await run("Clearing Wi-Fi") { [self] in
                self.applyWifiSettings(try await NanoClient(baseURLString: self.address).forgetWifi())
                self.status = "Wi-Fi credentials cleared."
            }
        }
    }

    func refreshRssFeeds() {
        Task {
            await run("Reading RSS feeds") { [self] in
                self.mergeRssFeedsFromDevice(try await NanoClient(baseURLString: self.address).fetchRssFeeds().feeds)
                self.status = "RSS feeds loaded from the SD card."
            }
        }
    }

    func addRssFeed() {
        let feed = rssFeedDraft.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !feed.isEmpty else { return }
        let scheme = URL(string: feed)?.scheme?.lowercased()
        guard scheme == "http" || scheme == "https" else {
            lastConnectionError = "RSS feed URLs must start with http:// or https://."
            status = "Could not add RSS feed."
            return
        }
        var next = rssFeeds
        if !next.contains(feed) {
            next.append(feed)
        }
        saveRssFeeds(next, status: isConnected ? "RSS feed synced." : "RSS feed saved locally.")
        rssFeedDraft = ""
    }

    func deleteRssFeeds(at offsets: IndexSet) {
        var next = rssFeeds
        next.remove(atOffsets: offsets)
        saveRssFeeds(next, status: "RSS feed removed.")
    }

    private func saveRssFeeds(_ feeds: [String], status successStatus: String) {
        let cleanedFeeds = normalizedRssFeeds(feeds)
        rssFeeds = cleanedFeeds
        rssFeedStore.save(cleanedFeeds)
        if !isConnected {
            status = successStatus
            return
        }

        Task {
            await run("Saving RSS feeds") { [self] in
                let deviceFeeds = try await NanoClient(baseURLString: self.address).updateRssFeeds(cleanedFeeds).feeds
                self.syncedRssFeeds = normalizedRssFeeds(deviceFeeds)
                self.rssFeeds = normalizedRssFeeds(cleanedFeeds + deviceFeeds)
                self.rssFeedStore.save(self.rssFeeds)
                self.status = successStatus
            }
        }
    }

    func syncRssFeeds() {
        saveRssFeeds(rssFeeds, status: "RSS feeds synced to the reader.")
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
        deleteBooks(booksToDelete)
    }

    func deleteBooks(_ booksToDelete: [NanoBook]) {
        guard !booksToDelete.isEmpty else { return }
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

    func handleSharedInboxOpen() {
        refreshPendingUploads()
        guard let item = pendingUploads.first(where: { $0.needsArticleFetch }) else {
            status = "Saved article ready to edit or sync."
            return
        }
        fetchArticleText(for: item)
    }

    func fetchArticleText(for item: PendingUpload) {
        Task {
            isBusy = true
            status = "Fetching article text"
            do {
                let article = try await ArticleFetchService.fetch(title: item.title, source: item.source)
                try PendingUploadStore().update(item, title: article.title, body: article.text)
                pendingUploads = try PendingUploadStore().all()
                lastConnectionError = nil
                status = "Fetched article text for \(article.title)."
            } catch {
                lastConnectionError = error.localizedDescription
                status = "Could not fetch article text."
            }
            isBusy = false
        }
    }

    func savePendingUpload(_ item: PendingUpload, title: String, body: String) {
        do {
            try PendingUploadStore().update(item, title: title, body: body)
            pendingUploads = try PendingUploadStore().all()
            editingArticle = nil
            lastConnectionError = nil
            status = "Saved \(title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty ? item.title : title)."
        } catch {
            lastConnectionError = error.localizedDescription
            status = "Could not save article."
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
            do {
                self.books = try await client.fetchBooks()
            } catch {
                self.books = []
                self.lastConnectionError = "Library read failed: \(error.localizedDescription)"
                self.deviceSettings = try? await client.fetchSettings()
                if let wifi = try? await client.fetchWifiSettings() {
                    self.applyWifiSettings(wifi)
                }
                self.mergeRssFeedsFromDevice((try? await client.fetchRssFeeds().feeds) ?? [])
                self.refreshPendingUploads()
                self.status = "Connected to \(self.info?.name ?? "RSVP Nano"), but the library could not be read."
                return
            }
            self.deviceSettings = try? await client.fetchSettings()
            if let wifi = try? await client.fetchWifiSettings() {
                self.applyWifiSettings(wifi)
            }
            self.mergeRssFeedsFromDevice((try? await client.fetchRssFeeds().feeds) ?? [])
            self.refreshPendingUploads()
            self.lastConnectionError = nil
            self.status = "Connected to \(self.info?.name ?? "RSVP Nano"). Reading /books."
        }
        return isConnected
    }

    private func uploadConverted(_ file: RsvpBookFile) async throws {
        let client = NanoClient(baseURLString: self.address)
        _ = try await client.uploadBook(data: file.data, filename: file.filename, category: "book")
        self.books = try await client.fetchBooks()
        self.status = uploadStatus(for: file)
    }

    private func uploadPendingItem(_ item: PendingUpload) async throws {
        let store = PendingUploadStore()
        let file = try store.bookFile(for: item)
        _ = try await NanoClient(baseURLString: self.address).uploadBook(data: file.data, filename: file.filename, category: "article")
        try store.delete(item)
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
            status = isConnected ? "Connected, but the last request failed." : "Still waiting for RSVP Nano Wi-Fi."
        }
        if showBusy {
            isBusy = false
        }
    }

    private func uploadStatus(for file: RsvpBookFile) -> String {
        guard file.wordCount > 0 else {
            return "Uploaded \(file.title) into /books/books."
        }
        let wordLabel = file.wordCount == 1 ? "word" : "words"
        let chapterLabel = file.chapterCount == 1 ? "chapter" : "chapters"
        return "Uploaded \(file.title) into /books/books: \(file.wordCount) \(wordLabel), \(file.chapterCount) \(chapterLabel)."
    }

    private func mergeRssFeedsFromDevice(_ deviceFeeds: [String]) {
        syncedRssFeeds = normalizedRssFeeds(deviceFeeds)
        rssFeeds = normalizedRssFeeds(rssFeedStore.load() + syncedRssFeeds)
        rssFeedStore.save(rssFeeds)
    }

    private func applyWifiSettings(_ wifi: NanoWifiSettings) {
        wifiSettings = wifi
        wifiSsidDraft = wifi.ssid
        wifiPasswordDraft = ""
    }

    private func normalizedRssFeeds(_ feeds: [String]) -> [String] {
        var seen = Set<String>()
        var result: [String] = []
        for feed in feeds {
            let cleaned = feed.trimmingCharacters(in: .whitespacesAndNewlines)
            guard !cleaned.isEmpty, !seen.contains(cleaned) else { continue }
            seen.insert(cleaned)
            result.append(cleaned)
        }
        return result
    }
}

private struct LocalRssFeedStore {
    private let key = "RSVPNanoLocalRssFeeds"

    func load() -> [String] {
        UserDefaults.standard.stringArray(forKey: key) ?? []
    }

    func save(_ feeds: [String]) {
        UserDefaults.standard.set(feeds, forKey: key)
    }
}

private enum CompanionPage: String, CaseIterable, Identifiable {
    case library = "Library"
    case articles = "Articles"
    case settings = "Settings"
    case help = "Help"

    var id: String { rawValue }

    var systemImage: String {
        switch self {
        case .library:
            return "books.vertical"
        case .articles:
            return "doc.text"
        case .settings:
            return "slider.horizontal.3"
        case .help:
            return "questionmark.circle"
        }
    }
}

struct ContentView: View {
    @StateObject private var viewModel = NanoViewModel()
    @State private var selectedPage: CompanionPage = .library

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                pageSelector
                Divider()
                selectedPageContent
            }
            .navigationTitle(selectedPage.rawValue)
            .toolbar {
                if selectedPage == .articles || (selectedPage == .library && viewModel.isConnected) {
                    EditButton()
                }
            }
        }
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
        .sheet(item: $viewModel.editingArticle) { item in
            ArticleEditorView(item: item) { title, body in
                viewModel.savePendingUpload(item, title: title, body: body)
            } onCancel: {
                viewModel.editingArticle = nil
            }
        }
        .task {
            viewModel.startAutoConnect()
        }
        .onReceive(NotificationCenter.default.publisher(for: UIApplication.willEnterForegroundNotification)) { _ in
            viewModel.refreshPendingUploads()
        }
        .onReceive(NotificationCenter.default.publisher(for: UIApplication.didBecomeActiveNotification)) { _ in
            viewModel.refreshPendingUploads()
        }
        .onOpenURL { url in
            if url.scheme == "rsvpnano", url.host == "inbox" {
                viewModel.handleSharedInboxOpen()
            }
        }
        .onDisappear {
            viewModel.stopAutoConnect()
        }
    }

    private var pageSelector: some View {
        Picker("Page", selection: $selectedPage) {
            ForEach(CompanionPage.allCases) { page in
                Label(page.rawValue, systemImage: page.systemImage)
                    .tag(page)
            }
        }
        .pickerStyle(.segmented)
        .padding(.horizontal)
        .padding(.vertical, 8)
        .background(.bar)
    }

    @ViewBuilder
    private var selectedPageContent: some View {
        switch selectedPage {
        case .library:
            libraryPage
        case .articles:
            articlesPage
        case .settings:
            settingsPage
        case .help:
            helpPage
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
                    Text("Tap Check Again to connect and show the SD card library from /books.")
                        .foregroundStyle(.secondary)
                }
                .padding(.vertical, 8)
            }

            Section {
                HStack {
                    if viewModel.isBusy {
                        ProgressView()
                    }
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

    @ViewBuilder
    private var savedArticlesSection: some View {
        Section("Saved Articles") {
            if viewModel.pendingUploads.isEmpty {
                VStack(alignment: .leading, spacing: 6) {
                    Text("No saved articles yet.")
                    Text("Use Share -> RSVP Nano from Safari, Chrome, or another app, then tap Save in the share sheet.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            } else {
                ForEach(viewModel.pendingUploads) { item in
                    VStack(alignment: .leading, spacing: 10) {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(item.title)
                                .foregroundStyle(.primary)
                            Text(pendingDetailLabel(for: item))
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }

                        HStack(spacing: 10) {
                            Button {
                                viewModel.editingArticle = item
                            } label: {
                                Label("Preview/Edit", systemImage: "pencil.and.list.clipboard")
                                    .frame(maxWidth: .infinity)
                            }
                            .buttonStyle(.bordered)

                            if item.needsArticleFetch {
                                Button {
                                    viewModel.fetchArticleText(for: item)
                                } label: {
                                    Label("Fetch", systemImage: "doc.text.magnifyingglass")
                                        .frame(maxWidth: .infinity)
                                }
                                .buttonStyle(.borderedProminent)
                                .disabled(viewModel.isBusy)
                            } else {
                                Button {
                                    viewModel.syncPendingUpload(item)
                                } label: {
                                    Label("Sync", systemImage: "arrow.up.doc")
                                        .frame(maxWidth: .infinity)
                                }
                                .buttonStyle(.borderedProminent)
                                .disabled(!viewModel.canUpload)
                            }
                        }
                    }
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
    }

    private var settingsSummarySection: some View {
        Section("Device Settings") {
            Button {
                viewModel.refreshSettings()
            } label: {
                Label("Load Settings", systemImage: "slider.horizontal.3")
            }
            .disabled(viewModel.info == nil || viewModel.isBusy)
        }
    }

    private var wordPacingSettingsSection: some View {
        Section("Word Pacing") {
            if let settings = viewModel.deviceSettings {
                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Reading Mode")
                    Picker("Reading Mode", selection: readerModeBinding(for: settings)) {
                        Text("One Word").tag("rsvp")
                        Text("Scroll Text").tag("scroll")
                    }
                    .pickerStyle(.segmented)
                }
                .disabled(viewModel.isBusy)

                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Pause Behaviour")
                    Picker("Pause Behaviour", selection: pauseModeBinding(for: settings)) {
                        Text("At Sentence End").tag("sentence_end")
                        Text("Immediately").tag("instant")
                    }
                    .pickerStyle(.segmented)
                }
                .disabled(viewModel.isBusy)

                Stepper(value: wpmBinding(for: settings), in: 100...1000, step: 25) {
                    LabeledContent("Base Speed", value: "\(settings.reading.wpm) WPM")
                }
                .disabled(viewModel.isBusy)

                Stepper(value: pacingLongBinding(for: settings), in: 0...600, step: 50) {
                    LabeledContent("Long Words", value: "\(settings.reading.pacing.longWordMs) ms")
                }
                .disabled(viewModel.isBusy)

                Stepper(value: pacingComplexBinding(for: settings), in: 0...600, step: 50) {
                    LabeledContent("Complexity", value: "\(settings.reading.pacing.complexWordMs) ms")
                }
                .disabled(viewModel.isBusy)

                Stepper(value: pacingPunctuationBinding(for: settings), in: 0...600, step: 50) {
                    LabeledContent("Punctuation", value: "\(settings.reading.pacing.punctuationMs) ms")
                }
                .disabled(viewModel.isBusy)
            }
        }
    }

    private var displaySettingsSection: some View {
        Section("Display") {
            if let settings = viewModel.deviceSettings {
                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Display Mode")
                    Picker("Display Mode", selection: appearanceModeBinding(for: settings)) {
                        Text("Light").tag("light")
                        Text("Dark").tag("dark")
                        Text("Night").tag("night")
                    }
                    .pickerStyle(.segmented)
                }
                .disabled(viewModel.isBusy)

                Stepper(value: brightnessBinding(for: settings), in: 0...4) {
                    LabeledContent("Brightness", value: "\(settings.display.brightnessIndex + 1) / 5")
                }
                .disabled(viewModel.isBusy)

                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Reader Hand")
                    Picker("Reader Hand", selection: handednessBinding(for: settings)) {
                        Text("Left").tag("left")
                        Text("Right").tag("right")
                    }
                    .pickerStyle(.segmented)
                }
                .disabled(viewModel.isBusy)

                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Footer Label")
                    Picker("Footer Label", selection: footerMetricBinding(for: settings)) {
                        Text("Percent Read").tag("percentage")
                        Text("Chapter Time").tag("chapter_time")
                        Text("Book Time").tag("book_time")
                    }
                }
                .disabled(viewModel.isBusy)

                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Battery Label")
                    Picker("Battery Label", selection: batteryLabelBinding(for: settings)) {
                        Text("Percentage").tag("percent")
                        Text("Time Remaining").tag("time_remaining")
                    }
                    .pickerStyle(.segmented)
                }
                .disabled(viewModel.isBusy)
            }
        }
    }

    private var typographySettingsSection: some View {
        Section("Typography") {
            if let settings = viewModel.deviceSettings {
                Picker("Typeface", selection: typefaceBinding(for: settings)) {
                    Text("Standard").tag("standard")
                    Text("Atkinson").tag("atkinson")
                    Text("OpenDyslexic").tag("open_dyslexic")
                }
                .disabled(viewModel.isBusy)

                Toggle("Focus Highlight", isOn: focusHighlightBinding(for: settings))
                    .disabled(viewModel.isBusy)

                Toggle("Phantom Words", isOn: phantomWordsBinding(for: settings))
                    .disabled(viewModel.isBusy)

                Stepper(value: fontSizeBinding(for: settings), in: 0...2) {
                    LabeledContent("Font Size", value: "\(settings.display.fontSizeIndex + 1) / 3")
                }
                .disabled(viewModel.isBusy)

                Stepper(value: trackingBinding(for: settings), in: -2...3) {
                    LabeledContent("Tracking", value: "\(settings.typography.tracking)")
                }
                .disabled(viewModel.isBusy)

                Stepper(value: anchorBinding(for: settings), in: 30...40) {
                    LabeledContent("Anchor", value: "\(settings.typography.anchorPercent)%")
                }
                .disabled(viewModel.isBusy)

                Stepper(value: guideWidthBinding(for: settings), in: 12...30, step: 2) {
                    LabeledContent("Guide Width", value: "\(settings.typography.guideWidth)")
                }
                .disabled(viewModel.isBusy)

                Stepper(value: guideGapBinding(for: settings), in: 2...8) {
                    LabeledContent("Guide Gap", value: "\(settings.typography.guideGap)")
                }
                .disabled(viewModel.isBusy)
            }
        }
    }

    private func settingsControlLabel(_ text: String) -> some View {
        Text(text)
            .font(.caption.weight(.semibold))
            .foregroundStyle(.secondary)
    }

    @ViewBuilder
    private var libraryPage: some View {
        if viewModel.isConnected {
            libraryList
        } else {
            connectInstructions
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

            let bookItems = viewModel.books.filter { !$0.isArticle }
            let articleItems = viewModel.books.filter(\.isArticle)

            Section("Books") {
                if viewModel.books.isEmpty {
                    VStack(alignment: .leading, spacing: 6) {
                        Text("No books reported yet.")
                        Text("Upload a book here after the SD card has a /books folder. New books are saved in /books/books.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                } else if bookItems.isEmpty {
                    Text("No books yet.")
                        .foregroundStyle(.secondary)
                } else {
                    ForEach(bookItems) { book in
                        libraryBookRow(book)
                    }
                    .onDelete { offsets in
                        viewModel.deleteBooks(offsets.map { bookItems[$0] })
                    }
                }
            }

            Section("Articles") {
                if articleItems.isEmpty {
                    VStack(alignment: .leading, spacing: 6) {
                        Text("No articles synced yet.")
                        Text("Shared articles and RSS downloads are saved in /books/articles.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                } else {
                    ForEach(articleItems) { book in
                        libraryBookRow(book)
                    }
                    .onDelete { offsets in
                        viewModel.deleteBooks(offsets.map { articleItems[$0] })
                    }
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
        }
    }

    private func libraryBookRow(_ book: NanoBook) -> some View {
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

    private var articlesPage: some View {
        List {
            savedArticlesSection
            syncedArticlesSection
            rssFeedsSection

            Section("Article Workflow") {
                VStack(alignment: .leading, spacing: 6) {
                    Label("Share from the browser", systemImage: "square.and.arrow.up")
                    Text("Use Share -> RSVP Nano from Safari, Chrome, or another app. URL-only articles can be fetched and renamed in the app before syncing.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("Edit before sync", systemImage: "pencil")
                    Text("Saved article drafts keep their title, source URL, and body text locally until you sync or delete them.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }

    private var syncedArticlesSection: some View {
        let articleItems = viewModel.books.filter(\.isArticle)

        return Section {
            if !viewModel.isConnected {
                Text("Connect to Companion sync to see articles already on the SD card.")
                    .foregroundStyle(.secondary)
            } else if articleItems.isEmpty {
                Text("No synced articles on the SD card yet.")
                    .foregroundStyle(.secondary)
            } else {
                ForEach(articleItems) { article in
                    VStack(alignment: .leading, spacing: 8) {
                        HStack(alignment: .firstTextBaseline, spacing: 10) {
                            libraryBookRow(article)
                            Text("Synced")
                                .font(.caption.weight(.semibold))
                                .foregroundStyle(.green)
                        }
                    }
                }
                .onDelete { offsets in
                    viewModel.deleteBooks(offsets.map { articleItems[$0] })
                }
            }

            if viewModel.isConnected {
                Button {
                    viewModel.refreshBooks()
                } label: {
                    Label("Refresh Synced Articles", systemImage: "arrow.clockwise")
                }
                .disabled(viewModel.isBusy)
            }
        } header: {
            Text("Synced Articles")
        } footer: {
            Text("These are articles already saved on the reader in /books/articles.")
        }
    }

    private var rssFeedsSection: some View {
        Section {
            if viewModel.rssFeeds.isEmpty {
                VStack(alignment: .leading, spacing: 6) {
                    Text("No RSS feeds saved.")
                    Text("Add feed URLs now, then sync them to the reader when Companion sync is connected.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            } else {
                ForEach(viewModel.rssFeeds, id: \.self) { feed in
                    HStack(alignment: .firstTextBaseline, spacing: 10) {
                        Text(feed)
                            .font(.caption)
                            .textSelection(.enabled)
                            .frame(maxWidth: .infinity, alignment: .leading)
                        Text(viewModel.syncedRssFeeds.contains(feed) ? "Synced" : "Pending")
                            .font(.caption.weight(.semibold))
                            .foregroundStyle(viewModel.syncedRssFeeds.contains(feed) ? .green : .orange)
                    }
                }
                .onDelete(perform: viewModel.deleteRssFeeds)
            }

            TextField("https://example.com/feed.xml", text: $viewModel.rssFeedDraft)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .keyboardType(.URL)

            HStack(spacing: 10) {
                Button {
                    viewModel.addRssFeed()
                } label: {
                    Label(viewModel.isConnected ? "Add & Sync" : "Add Feed", systemImage: "plus")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .disabled(viewModel.isBusy || viewModel.rssFeedDraft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)

                Button {
                    viewModel.isConnected ? viewModel.syncRssFeeds() : viewModel.connect()
                } label: {
                    Label(viewModel.isConnected ? "Sync Feeds" : "Connect", systemImage: "arrow.triangle.2.circlepath")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .disabled(viewModel.isBusy || (viewModel.isConnected && viewModel.rssFeeds.isEmpty))
            }

            if viewModel.isConnected {
                Button {
                    viewModel.refreshRssFeeds()
                } label: {
                    Label("Reload From Reader", systemImage: "arrow.down.circle")
                }
                .disabled(viewModel.isBusy)
            }
        } header: {
            Text("RSS Feeds")
        } footer: {
            Text("Feeds marked Pending are saved on this iPhone. Sync writes the full list to /config/rss.conf on the reader.")
        }
    }

    private var settingsPage: some View {
        List {
            if viewModel.deviceSettings == nil {
                settingsSummarySection
            } else {
                wordPacingSettingsSection
                displaySettingsSection
                wifiSettingsSection
                typographySettingsSection

                Section {
                    Button {
                        viewModel.refreshSettings()
                    } label: {
                        Label("Refresh Settings", systemImage: "arrow.clockwise")
                    }
                    .disabled(viewModel.isBusy)

                    Text("Changes are saved to the reader. Exit sync on the device to apply every setting on-screen.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }

    private var wifiSettingsSection: some View {
        Section {
            if let wifi = viewModel.wifiSettings {
                LabeledContent("Saved Network", value: wifi.configured ? wifi.ssid : "Not set")
                if wifi.passwordSet {
                    Text("A password is saved on the reader. The app cannot read it back.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }

            TextField("Network name", text: $viewModel.wifiSsidDraft)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()

            SecureField("Password", text: $viewModel.wifiPasswordDraft)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()

            HStack(spacing: 10) {
                Button {
                    viewModel.saveWifiSettings()
                } label: {
                    Label("Save Wi-Fi", systemImage: "wifi")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .disabled(viewModel.isBusy || viewModel.wifiSsidDraft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)

                Button(role: .destructive) {
                    viewModel.forgetWifiSettings()
                } label: {
                    Label("Forget", systemImage: "trash")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .disabled(viewModel.isBusy || viewModel.wifiSettings?.configured != true)
            }

            Button {
                viewModel.refreshWifiSettings()
            } label: {
                Label("Refresh Wi-Fi", systemImage: "arrow.clockwise")
            }
            .disabled(viewModel.isBusy)
        } header: {
            Text("Wi-Fi")
        } footer: {
            Text("Used for RSS and OTA. You can still configure Wi-Fi directly on the reader if you want the standalone path.")
        }
    }

    private var helpPage: some View {
        List {
            Section("Connection") {
                VStack(alignment: .leading, spacing: 6) {
                    Label("Open Companion sync", systemImage: "wifi")
                    Text("On RSVP Nano, open the main menu and choose Companion sync. Join the RSVP-Nano Wi-Fi network on your iPhone, then return to the app.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("Refresh after uploading", systemImage: "arrow.triangle.2.circlepath")
                    Text("After uploading, hold BOOT on the reader to exit Companion sync and refresh the on-device library.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }

            Section("SD Card") {
                VStack(alignment: .leading, spacing: 6) {
                    Label("Recommended card", systemImage: "sdcard")
                    Text("Use a known-good microSD card. 8-32 GB is the most conservative range, and 64 GB cards can work well when formatted as FAT32 with a single partition.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("Best file format", systemImage: "doc.text")
                    Text("The app uploads .rsvp files when it can. New books go in /books/books, shared and RSS articles go in /books/articles, and older files directly in /books still work.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("If the card fails", systemImage: "exclamationmark.triangle")
                    Text("The usual causes are exFAT formatting, a missing /books folder, the card not being seated fully, a tired or counterfeit card, or files with unsupported extensions.")
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

            Section("Articles") {
                VStack(alignment: .leading, spacing: 6) {
                    Label("Safari and Chrome behave differently", systemImage: "safari")
                    Text("Chrome often shares a title immediately. Safari is handled as URL-first for stability, then the app fetches the article text and title after saving.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("Large pages", systemImage: "doc.text.magnifyingglass")
                    Text("Very large pages may be rejected during article fetch so the app stays responsive.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("RSS feeds", systemImage: "dot.radiowaves.left.and.right")
                    Text("Add feed URLs from the Articles page while connected to Companion sync. The reader saves them to /config/rss.conf and can check them from its main menu when Wi-Fi is configured.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }

    private var statusBar: some View {
        HStack(spacing: 8) {
            if viewModel.isBusy {
                ProgressView()
                    .scaleEffect(0.75)
            }
            Text(viewModel.status)
                .font(.caption2)
                .foregroundStyle(.secondary)
                .lineLimit(1)
                .truncationMode(.tail)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 5)
        .background(.bar)
    }

    private func pendingDetailLabel(for item: PendingUpload) -> String {
        let size = ByteCountFormatter.string(fromByteCount: Int64(item.bytes), countStyle: .file)
        let words = item.body.split { $0.isWhitespace }.count
        let detail = item.needsArticleFetch ? "link saved" : (words == 1 ? "\(words) word" : "\(words) words")
        if item.source.isEmpty {
            return "\(detail) · \(size)"
        }
        return "\(detail) · \(size) · \(item.source)"
    }

    private func wpmBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.reading.wpm ?? settings.reading.wpm },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.reading.wpm = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func pauseModeBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.reading.pauseMode ?? settings.reading.pauseMode },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.reading.pauseMode = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func readerModeBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.reading.readerMode ?? settings.reading.readerMode },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.reading.readerMode = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func pacingLongBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.reading.pacing.longWordMs ?? settings.reading.pacing.longWordMs },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.reading.pacing.longWordMs = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func pacingComplexBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.reading.pacing.complexWordMs ?? settings.reading.pacing.complexWordMs },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.reading.pacing.complexWordMs = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func pacingPunctuationBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.reading.pacing.punctuationMs ?? settings.reading.pacing.punctuationMs },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.reading.pacing.punctuationMs = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func brightnessBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.display.brightnessIndex ?? settings.display.brightnessIndex },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.display.brightnessIndex = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func handednessBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.display.handedness ?? settings.display.handedness },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.display.handedness = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func footerMetricBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.display.footerMetric ?? settings.display.footerMetric },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.display.footerMetric = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func batteryLabelBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.display.batteryLabel ?? settings.display.batteryLabel },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.display.batteryLabel = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func appearanceModeBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: {
                let display = viewModel.deviceSettings?.display ?? settings.display
                if display.nightMode {
                    return "night"
                }
                return display.darkMode ? "dark" : "light"
            },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.display.darkMode = value == "dark" || value == "night"
                next.display.nightMode = value == "night"
                viewModel.saveSettings(next)
            }
        )
    }

    private func phantomWordsBinding(for settings: NanoSettings) -> Binding<Bool> {
        Binding(
            get: { viewModel.deviceSettings?.display.phantomWords ?? settings.display.phantomWords },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.display.phantomWords = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func fontSizeBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.display.fontSizeIndex ?? settings.display.fontSizeIndex },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.display.fontSizeIndex = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func typefaceBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.typography.typeface ?? settings.typography.typeface },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.typography.typeface = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func focusHighlightBinding(for settings: NanoSettings) -> Binding<Bool> {
        Binding(
            get: { viewModel.deviceSettings?.typography.focusHighlight ?? settings.typography.focusHighlight },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.typography.focusHighlight = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func trackingBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.typography.tracking ?? settings.typography.tracking },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.typography.tracking = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func anchorBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.typography.anchorPercent ?? settings.typography.anchorPercent },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.typography.anchorPercent = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func guideWidthBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.typography.guideWidth ?? settings.typography.guideWidth },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.typography.guideWidth = value
                viewModel.saveSettings(next)
            }
        )
    }

    private func guideGapBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.typography.guideGap ?? settings.typography.guideGap },
            set: { value in
                guard var next = viewModel.deviceSettings else { return }
                next.typography.guideGap = value
                viewModel.saveSettings(next)
            }
        )
    }

}

struct ArticleEditorView: View {
    let item: PendingUpload
    var onSave: (String, String) -> Void
    var onCancel: () -> Void

    @State private var title: String
    @State private var articleBody: String

    init(item: PendingUpload, onSave: @escaping (String, String) -> Void, onCancel: @escaping () -> Void) {
        self.item = item
        self.onSave = onSave
        self.onCancel = onCancel
        _title = State(initialValue: item.title)
        _articleBody = State(initialValue: item.needsArticleFetch ? "" : item.body)
    }

    private var wordCount: Int {
        articleBody.split { $0.isWhitespace }.count
    }

    private var canSave: Bool {
        !articleBody.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }

    var body: some View {
        NavigationStack {
            List {
                Section("Title") {
                    TextField("Article title", text: $title)
                }

                if !item.source.isEmpty {
                    Section("Source") {
                        Text(item.source)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .textSelection(.enabled)
                    }
                }

                Section {
                    TextEditor(text: $articleBody)
                        .frame(minHeight: 320)
                        .font(.body)
                        .autocorrectionDisabled()
                } header: {
                    Text("Article")
                } footer: {
                    Text(item.needsArticleFetch ? "Fetch article text first, or paste text here manually." : "\(wordCount) words")
                }
            }
            .navigationTitle("Article")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel", action: onCancel)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        onSave(title, articleBody)
                    }
                    .disabled(!canSave)
                }
            }
        }
    }
}
