import SwiftUI
import shared

@MainActor
final class NanoConnectionManager: ObservableObject {
    static let shared = NanoConnectionManager()
    
    let companionController = IosSharedWiringKt.createIosCompanionController(appGroupIdentifier: SharedInbox.appGroupIdentifier)
    
    @Published var address = shared.SharedAppUtils.shared.DEFAULT_DEVICE_ADDRESS
    @Published var info: NanoInfo?
    @Published var status = "Waiting for RSVP Nano Wi-Fi."
    @Published var isBusy = false
    @Published var hasAttemptedConnection = false
    @Published var lastConnectionError: String?
    @Published var showAddressEntry = false
    
    var isConnected: Bool {
        info != nil
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
    
    @discardableResult
    func connectOnce(showBusy: Bool = true) async -> Bool {
        await run("Looking for RSVP Nano", showBusy: showBusy) { [self] in
            self.hasAttemptedConnection = true
            let address = self.normalizedAddress(self.address)
            // Note: RSS feeds will be updated by specialized view models after connection
            let snapshot = try await companionController.connect(baseUrl: address, localRssFeeds: [])
            self.applyConnectionSnapshot(snapshot, address: address)
        }
        return isConnected
    }
    
    func run(
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
    
    func applyConnectionSnapshot(_ snapshot: shared.CompanionConnectSnapshot, address: String) {
        let device = snapshot.device
        self.address = address
        self.info = device.info
        self.lastConnectionError = nil
        self.showAddressEntry = false
        self.status = "Connected to \(self.info?.name ?? "RSVP Nano"). \(device.summaryText)"
        
        // Post notifications or update other view models as needed
        NotificationCenter.default.post(name: .nanoDeviceConnected, object: snapshot)
    }
    
    func markDisconnected(_ message: String) {
        info = nil
        lastConnectionError = message
        showAddressEntry = normalizedAddress(address) == shared.SharedAppUtils.shared.DEFAULT_DEVICE_ADDRESS
        status = message
        NotificationCenter.default.post(name: .nanoDeviceDisconnected, object: message)
    }
    
    func normalizedAddress(_ value: String) -> String {
        return shared.SharedAppUtils.shared.normalizedAddress(value: value)
    }
}

extension NSNotification.Name {
    static let nanoDeviceConnected = NSNotification.Name("nanoDeviceConnected")
    static let nanoDeviceDisconnected = NSNotification.Name("nanoDeviceDisconnected")
}
