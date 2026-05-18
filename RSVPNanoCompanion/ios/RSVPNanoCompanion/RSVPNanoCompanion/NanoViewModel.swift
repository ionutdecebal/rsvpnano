import SwiftUI
import UIKit
import shared

@MainActor
final class NanoViewModel: ObservableObject {
    private let companionController = IosSharedWiringKt.createIosCompanionController(appGroupIdentifier: SharedInbox.appGroupIdentifier)

    @Published var address = shared.SharedAppUtils.shared.DEFAULT_DEVICE_ADDRESS
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
    @Published var showAddressEntry = false

    var canUpload: Bool {
        info != nil && !isBusy
    }

    var isConnected: Bool {
        info != nil
    }

    var canSyncPending: Bool {
        isConnected && !isBusy && !pendingUploads.isEmpty
    }

    func startAutoConnect() async {
        do {
            let local = try await companionController.refreshLocal()
            pendingUploads = local.drafts
            rssFeeds = local.rssFeeds
            _ = await connectOnce(showBusy: false)
        } catch {
            lastConnectionError = error.localizedDescription
            pendingUploads = []
            rssFeeds = []
        }
    }

    func stopAutoConnect() {
    }

    func connect(showBusy: Bool = true) {
        Task {
            _ = await connectOnce(showBusy: showBusy)
        }
    }

    func connectDefault(showBusy: Bool = true) {
        address = shared.SharedAppUtils.shared.DEFAULT_DEVICE_ADDRESS
        connect(showBusy: showBusy)
    }

    func showManualAddressEntry() {
        showAddressEntry = true
        status = "If the default address is not working, enter the address shown by the reader."
    }

    func refreshBooks() {
        Task {
            await run("Refreshing", requiresConnection: true) { [self] in
                let snapshot = try await companionController.refreshDevice(baseUrl: self.address, localRssFeeds: self.rssFeeds)
                self.books = snapshot.books
                self.deviceSettings = snapshot.settings
                if let wifi = snapshot.wifiSettings {
                    self.applyWifiSettings(wifi)
                }
                self.syncedRssFeeds = snapshot.syncedRssFeeds
                self.rssFeeds = snapshot.rssFeeds
                self.pendingUploads = snapshot.drafts
                self.status = "Library refreshed from the SD card."
            }
        }
    }

    func refreshSettings() {
        Task {
            await run("Reading settings", requiresConnection: true) { [self] in
                let snapshot = try await companionController.refreshSettings(baseUrl: self.address)
                self.deviceSettings = snapshot.settings
                if let wifi = snapshot.wifiSettings {
                    self.applyWifiSettings(wifi)
                }
                self.status = "Device settings refreshed."
            }
        }
    }

    func saveSettings(_ settings: NanoSettings) {
        Task {
            await run("Saving settings", requiresConnection: true) { [self] in
                let next = settings.withAccurateTimeEstimate(value: true)
                self.deviceSettings = try await companionController.saveSettings(baseUrl: self.address, settings: next).settings
                self.status = "Device settings saved. Exit sync on the reader to apply all changes."
            }
        }
    }

    func refreshWifiSettings() {
        Task {
            await run("Reading Wi-Fi settings", requiresConnection: true) { [self] in
                self.applyWifiSettings(try await companionController.refreshWifiSettings(baseUrl: self.address).wifiSettings)
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
            await run("Saving Wi-Fi", requiresConnection: true) { [self] in
                let wifi = try await companionController.saveWifiSettings(
                    baseUrl: self.address,
                    ssid: ssid,
                    password: self.wifiPasswordDraft
                ).wifiSettings
                self.applyWifiSettings(wifi)
                self.status = "Wi-Fi saved for RSS and OTA."
            }
        }
    }

    func forgetWifiSettings() {
        Task {
            await run("Clearing Wi-Fi", requiresConnection: true) { [self] in
                self.applyWifiSettings(try await companionController.clearWifiSettings(baseUrl: self.address).wifiSettings)
                self.status = "Wi-Fi credentials cleared."
            }
        }
    }

    func refreshRssFeeds() {
        Task {
            await run("Reading RSS feeds", requiresConnection: true) { [self] in
                let snapshot = try await companionController.refreshRssFeeds(
                    baseUrl: self.address,
                    localRssFeeds: self.rssFeeds
                )
                self.rssFeeds = snapshot.rssFeeds
                self.syncedRssFeeds = snapshot.syncedRssFeeds
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
        Task {
            await saveRssFeeds(next, status: isConnected ? "RSS feed synced." : "RSS feed saved locally.")
        }
        rssFeedDraft = ""
    }

    func deleteRssFeeds(at offsets: IndexSet) {
        var next = rssFeeds
        next.remove(atOffsets: offsets)
        Task {
            await saveRssFeeds(next, status: "RSS feed removed.")
        }
    }

    private func saveRssFeeds(_ feeds: [String], status successStatus: String) async {
        await run("Saving RSS feeds", showBusy: isConnected, requiresConnection: isConnected) { [self] in
            let snapshot = try await companionController.saveRssFeeds(
                baseUrl: self.address,
                feeds: feeds,
                syncToDevice: self.isConnected
            )
            self.rssFeeds = snapshot.rssFeeds
            if snapshot.didSyncDevice {
                self.syncedRssFeeds = snapshot.syncedRssFeeds
            }
            self.status = successStatus
        }
    }

    func syncRssFeeds() {
        Task {
            await saveRssFeeds(rssFeeds, status: "RSS feeds synced to the reader.")
        }
    }

    func upload(_ file: PickedBookFile) {
        Task {
            await run("Preparing \(file.filename)", requiresConnection: true) { [self] in
                do {
                    let converted = try shared.RsvpConverter.shared.bookFile(
                        data: kotlinByteArray(from: file.data),
                        filename: file.filename
                    )
                    try await self.uploadConverted(converted)
                } catch {
                    if file.filename.lowercased().hasSuffix(".epub"),
                       error.localizedDescription == "This EPUB could not be converted locally." {
                        let raw = shared.RsvpBookFile(
                            filename: file.filename,
                            data: kotlinByteArray(from: file.data),
                            title: shared.RsvpConverter.shared.filenameWithoutExtension(filename: file.filename),
                            wordCount: 0,
                            chapterCount: 0
                        )
                        try await self.uploadConverted(raw)
                    } else {
                        throw error
                    }
                }
            }
        }
    }

    func upload(_ file: shared.RsvpBookFile) {
        Task {
            await run("Uploading \(file.title)", requiresConnection: true) { [self] in
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
            await run(
                booksToDelete.count == 1 ? "Deleting \(booksToDelete[0].displayTitle)" : "Deleting books",
                requiresConnection: true
            ) { [self] in
                let snapshot = try await companionController.deleteBooks(
                    baseUrl: self.address,
                    bookIds: booksToDelete.map(\.id)
                )
                self.books = snapshot.books
                self.status = booksToDelete.count == 1 ? "Deleted \(booksToDelete[0].displayTitle)." : "Deleted books."
            }
        }
    }

    func refreshPendingUploads() async {
        do {
            let snapshot = try await companionController.refreshDrafts()
            pendingUploads = snapshot.drafts
        } catch {
            lastConnectionError = error.localizedDescription
            pendingUploads = []
        }
    }

    func handleSharedInboxOpen() {
        Task {
            await refreshPendingUploads()
            guard let item = pendingUploads.first(where: { companionController.needsArticleFetch(item: $0) }) else {
                status = "Saved article ready to edit or sync."
                return
            }
            fetchArticleText(for: item)
        }
    }

    func fetchArticleText(for item: PendingUpload) {
        Task {
            isBusy = true
            status = "Fetching article text"
            do {
                let snapshot = try await companionController.fetchArticle(item: item)
                pendingUploads = snapshot.drafts
                lastConnectionError = nil
                status = "Fetched article text for \(snapshot.article.title)."
            } catch {
                lastConnectionError = error.localizedDescription
                status = "Could not fetch article text."
            }
            isBusy = false
        }
    }

    func savePendingUpload(_ item: PendingUpload, title: String, body: String) {
        Task {
            do {
                let snapshot = try await companionController.updateDraft(
                    item: item,
                    title: title,
                    body: body
                )
                pendingUploads = snapshot.drafts
                editingArticle = nil
                lastConnectionError = nil
                status = "Saved \(title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty ? item.title : title)."
            } catch {
                lastConnectionError = error.localizedDescription
                status = "Could not save article."
            }
        }
    }

    func syncPendingUploads() {
        let items = pendingUploads
        Task {
            await run(
                items.count == 1 ? "Syncing \(items[0].title)" : "Syncing saved articles",
                requiresConnection: true
            ) { [self] in
                let snapshot = try await companionController.syncPendingUploads(
                    baseUrl: self.address,
                    items: items
                )
                self.pendingUploads = snapshot.drafts
                self.books = snapshot.books
                self.status = items.count == 1 ? "Synced \(items[0].title)." : "Synced saved articles."
            }
        }
    }

    func syncPendingUpload(_ item: PendingUpload) {
        Task {
            await run("Syncing \(item.title)", requiresConnection: true) { [self] in
                let snapshot = try await companionController.syncPendingUploads(
                    baseUrl: self.address,
                    items: [item]
                )
                self.pendingUploads = snapshot.drafts
                self.books = snapshot.books
                self.status = "Synced \(item.title)."
            }
        }
    }

    func deletePendingUploads(at offsets: IndexSet) {
        Task {
            do {
                let ids = offsets.compactMap { index in
                    index < pendingUploads.count ? pendingUploads[index].id : nil
                }
                let snapshot = try await companionController.deleteDrafts(ids: ids)
                pendingUploads = snapshot.drafts
            } catch {
                lastConnectionError = error.localizedDescription
            }
        }
    }

    @discardableResult
    private func connectOnce(showBusy: Bool = true) async -> Bool {
        await run("Looking for RSVP Nano", showBusy: showBusy) { [self] in
            self.hasAttemptedConnection = true
            let address = self.normalizedAddress(self.address)
            let snapshot = try await companionController.connect(baseUrl: address, localRssFeeds: self.rssFeeds)
            self.applyConnectionSnapshot(snapshot, address: address)
        }
        return isConnected
    }

    private func uploadConverted(_ file: shared.RsvpBookFile) async throws {
        let snapshot = try await companionController.uploadBook(baseUrl: self.address, file: file, category: "book")
        self.books = snapshot.books
        self.status = uploadStatus(for: file)
    }

    private func run(
        _ busyStatus: String,
        showBusy: Bool = true,
        requiresConnection: Bool = false,
        operation: @escaping () async throws -> Void
    ) async {
        if showBusy {
            isBusy = true
        }
        status = busyStatus
        do {
            if requiresConnection {
                try await companionController.verifyReachable(baseUrl: normalizedAddress(address))
            }
            try await operation()
        } catch {
            lastConnectionError = error.localizedDescription
            if requiresConnection {
                markDisconnected("Reader disconnected. Reconnect to RSVP Nano before continuing.")
            } else if !isConnected && normalizedAddress(address) == shared.SharedAppUtils.shared.DEFAULT_DEVICE_ADDRESS {
                showAddressEntry = true
                status = "Could not find RSVP Nano at \(shared.SharedAppUtils.shared.DEFAULT_DEVICE_ADDRESS). Check the Nano Wi-Fi, or enter the address shown on the reader."
            } else {
                status = isConnected ? "Connected, but the last request failed." : "Still waiting for RSVP Nano Wi-Fi."
            }
        }
        if showBusy {
            isBusy = false
        }
    }

    private func applyConnectionSnapshot(_ snapshot: shared.CompanionConnectSnapshot, address: String) {
        let device = snapshot.device
        self.address = address
        self.info = device.info
        self.books = device.books
        self.deviceSettings = device.settings
        if let wifi = device.wifiSettings {
            self.applyWifiSettings(wifi)
        }
        self.rssFeeds = snapshot.rssFeeds
        self.syncedRssFeeds = snapshot.syncedRssFeeds
        self.pendingUploads = snapshot.drafts
        self.lastConnectionError = nil
        self.showAddressEntry = false
        self.status = "Connected to \(self.info?.name ?? "RSVP Nano"). \(device.summaryText)"
    }

    private func markDisconnected(_ message: String) {
        info = nil
        books = []
        deviceSettings = nil
        wifiSettings = nil
        lastConnectionError = message
        showAddressEntry = normalizedAddress(address) == shared.SharedAppUtils.shared.DEFAULT_DEVICE_ADDRESS
        status = message
    }

    private func uploadStatus(for file: shared.RsvpBookFile) -> String {
        guard file.wordCount > 0 else {
            return "Uploaded \(file.title) into /books/books."
        }
        let wordLabel = file.wordCount == 1 ? "word" : "words"
        let chapterLabel = file.chapterCount == 1 ? "chapter" : "chapters"
        return "Uploaded \(file.title) into /books/books: \(file.wordCount) \(wordLabel), \(file.chapterCount) \(chapterLabel)."
    }

    private func applyWifiSettings(_ wifi: NanoWifiSettings) {
        wifiSettings = wifi
        wifiSsidDraft = wifi.ssid
        wifiPasswordDraft = ""
    }

    private func normalizedAddress(_ value: String) -> String {
        return shared.SharedAppUtils.shared.normalizedAddress(value: value)
    }

    private func kotlinByteArray(from data: Data) -> KotlinByteArray {
        let array = KotlinByteArray(size: Int32(data.count))
        data.withUnsafeBytes { buffer in
            guard let baseAddress = buffer.baseAddress else { return }
            for index in 0..<data.count {
                let value = baseAddress.advanced(by: index).assumingMemoryBound(to: UInt8.self).pointee
                array.set(index: Int32(index), value: Int8(bitPattern: value))
            }
        }
        return array
    }
}
