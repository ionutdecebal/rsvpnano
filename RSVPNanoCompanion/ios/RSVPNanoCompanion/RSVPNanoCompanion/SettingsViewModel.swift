import SwiftUI
import shared

@MainActor
final class SettingsViewModel: ObservableObject {
    @Published var deviceSettings: NanoSettings?
    @Published var wifiSettings: NanoWifiSettings?
    @Published var wifiSsidDraft = ""
    @Published var wifiPasswordDraft = ""
    @Published var rssFeeds: [String] = []
    @Published var syncedRssFeeds: [String] = []
    @Published var rssFeedDraft = ""
    
    private let connection: NanoConnectionManager
    private var connectionObserver: Any?
    private var disconnectionObserver: Any?
    
    init(connection: NanoConnectionManager = .shared) {
        self.connection = connection
        
        connectionObserver = NotificationCenter.default.addObserver(
            forName: .nanoDeviceConnected,
            object: nil,
            queue: .main
        ) { [weak self] notification in
            if let snapshot = notification.object as? shared.CompanionConnectSnapshot {
                self?.applySnapshot(snapshot)
            }
        }
        
        disconnectionObserver = NotificationCenter.default.addObserver(
            forName: .nanoDeviceDisconnected,
            object: nil,
            queue: .main
        ) { [weak self] _ in
            self?.deviceSettings = nil
            self?.wifiSettings = nil
            self?.syncedRssFeeds = []
        }
    }
    
    func refreshSettings() {
        Task {
            await connection.run("Reading settings", requiresConnection: true) { [self] in
                let snapshot = try await connection.companionController.refreshSettings(baseUrl: connection.address)
                self.deviceSettings = snapshot.settings
                if let wifi = snapshot.wifiSettings {
                    self.applyWifiSettings(wifi)
                }
                connection.status = "Device settings refreshed."
            }
        }
    }

    func saveSettings(_ settings: NanoSettings) {
        Task {
            await connection.run("Saving settings", requiresConnection: true) { [self] in
                let next = settings.withAccurateTimeEstimate(value: true)
                self.deviceSettings = try await connection.companionController.saveSettings(
                    baseUrl: connection.address, 
                    settings: next
                ).settings
                connection.status = "Device settings saved. Exit sync on the reader to apply all changes."
            }
        }
    }

    func refreshWifiSettings() {
        Task {
            await connection.run("Reading Wi-Fi settings", requiresConnection: true) { [self] in
                self.applyWifiSettings(
                    try await connection.companionController.refreshWifiSettings(baseUrl: connection.address).wifiSettings
                )
                connection.status = "Wi-Fi settings refreshed."
            }
        }
    }

    func saveWifiSettings() {
        let ssid = wifiSsidDraft.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !ssid.isEmpty else {
            connection.lastConnectionError = "Enter a Wi-Fi network name first."
            connection.status = "Wi-Fi was not saved."
            return
        }
        Task {
            await connection.run("Saving Wi-Fi", requiresConnection: true) { [self] in
                let wifi = try await connection.companionController.saveWifiSettings(
                    baseUrl: connection.address,
                    ssid: ssid,
                    password: self.wifiPasswordDraft
                ).wifiSettings
                self.applyWifiSettings(wifi)
                connection.status = "Wi-Fi saved for RSS and OTA."
            }
        }
    }

    func forgetWifiSettings() {
        Task {
            await connection.run("Clearing Wi-Fi", requiresConnection: true) { [self] in
                self.applyWifiSettings(
                    try await connection.companionController.clearWifiSettings(baseUrl: connection.address).wifiSettings
                )
                connection.status = "Wi-Fi credentials cleared."
            }
        }
    }

    func refreshRssFeeds() {
        Task {
            await connection.run("Reading RSS feeds", requiresConnection: true) { [self] in
                let snapshot = try await connection.companionController.refreshRssFeeds(
                    baseUrl: connection.address,
                    localRssFeeds: self.rssFeeds
                )
                self.rssFeeds = snapshot.rssFeeds
                self.syncedRssFeeds = snapshot.syncedRssFeeds
                connection.status = "RSS feeds loaded from the SD card."
            }
        }
    }

    func addRssFeed() {
        let feed = rssFeedDraft.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !feed.isEmpty else { return }
        let scheme = URL(string: feed)?.scheme?.lowercased()
        guard scheme == "http" || scheme == "https" else {
            connection.lastConnectionError = "RSS feed URLs must start with http:// or https://."
            connection.status = "Could not add RSS feed."
            return
        }
        var next = rssFeeds
        if !next.contains(feed) {
            next.append(feed)
        }
        Task {
            await saveRssFeeds(next, status: connection.isConnected ? "RSS feed synced." : "RSS feed saved locally.")
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
        await connection.run("Saving RSS feeds", showBusy: connection.isConnected, requiresConnection: connection.isConnected) { [self] in
            let snapshot = try await connection.companionController.saveRssFeeds(
                baseUrl: connection.address,
                feeds: feeds,
                syncToDevice: connection.isConnected
            )
            self.rssFeeds = snapshot.rssFeeds
            if snapshot.didSyncDevice {
                self.syncedRssFeeds = snapshot.syncedRssFeeds
            }
            connection.status = successStatus
        }
    }

    func syncRssFeeds() {
        Task {
            await saveRssFeeds(rssFeeds, status: "RSS feeds synced to the reader.")
        }
    }

    private func applySnapshot(_ snapshot: shared.CompanionConnectSnapshot) {
        let device = snapshot.device
        self.deviceSettings = device.settings
        if let wifi = device.wifiSettings {
            self.applyWifiSettings(wifi)
        }
        self.rssFeeds = snapshot.rssFeeds
        self.syncedRssFeeds = snapshot.syncedRssFeeds
    }

    private func applyWifiSettings(_ wifi: NanoWifiSettings) {
        wifiSettings = wifi
        wifiSsidDraft = wifi.ssid
        wifiPasswordDraft = ""
    }
}
