import SwiftUI
import shared

@MainActor
final class InboxViewModel: ObservableObject {
    @Published var pendingUploads: [PendingUpload] = []
    @Published var editingArticle: PendingUpload?
    
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
                self?.pendingUploads = snapshot.drafts
            }
        }
        
        disconnectionObserver = NotificationCenter.default.addObserver(
            forName: .nanoDeviceDisconnected,
            object: nil,
            queue: .main
        ) { [weak self] _ in
            // Drafts are local, so we don't necessarily clear them on disconnect,
            // but we might want to refresh them.
            Task {
                await self?.refreshPendingUploads()
            }
        }
    }
    
    func refreshPendingUploads() async {
        do {
            let snapshot = try await connection.companionController.refreshDrafts()
            pendingUploads = snapshot.drafts
        } catch {
            connection.lastConnectionError = error.localizedDescription
            pendingUploads = []
        }
    }

    func handleSharedInboxOpen() {
        Task {
            await refreshPendingUploads()
            if pendingUploads.contains(where: { connection.companionController.needsArticleFetch(item: $0) }) {
                connection.status = "Saved link. Share it again while online, or paste article text before syncing to the Nano."
            } else {
                connection.status = "Saved article ready to edit or sync."
            }
        }
    }

    func savePendingUpload(_ item: PendingUpload, title: String, body: String) {
        Task {
            do {
                let snapshot = try await connection.companionController.updateDraft(
                    item: item,
                    title: title,
                    body: body
                )
                pendingUploads = snapshot.drafts
                editingArticle = nil
                connection.status = "Saved \(title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty ? item.title : title)."
            } catch {
                connection.lastConnectionError = error.localizedDescription
                connection.status = "Could not save article."
            }
        }
    }

    func syncPendingUploads() {
        let items = pendingUploads.filter { !connection.companionController.needsArticleFetch(item: $0) }
        guard !items.isEmpty else {
            connection.status = "No fetched articles are ready. Share links while online, or paste article text first."
            return
        }
        Task {
            await connection.run(
                items.count == 1 ? "Syncing \(items[0].title)" : "Syncing saved articles",
                requiresConnection: true
            ) { [self] in
                let snapshot = try await connection.companionController.syncPendingUploads(
                    baseUrl: connection.address,
                    items: items
                )
                self.pendingUploads = snapshot.drafts
                // Note: books list in LibraryViewModel will be updated via notification if we added one,
                // or we can just rely on the next refresh.
                connection.status = items.count == 1 ? "Synced \(items[0].title)." : "Synced saved articles."
                NotificationCenter.default.post(name: .nanoLibraryUpdated, object: snapshot.books)
            }
        }
    }

    func syncPendingUpload(_ item: PendingUpload) {
        guard !connection.companionController.needsArticleFetch(item: item) else {
            connection.status = "Add article text before syncing this link to the Nano."
            return
        }
        Task {
            await connection.run("Syncing \(item.title)", requiresConnection: true) { [self] in
                let snapshot = try await connection.companionController.syncPendingUploads(
                    baseUrl: connection.address,
                    items: [item]
                )
                self.pendingUploads = snapshot.drafts
                connection.status = "Synced \(item.title)."
                NotificationCenter.default.post(name: .nanoLibraryUpdated, object: snapshot.books)
            }
        }
    }

    func deletePendingUploads(at offsets: IndexSet) {
        Task {
            do {
                let ids = offsets.compactMap { index in
                    index < pendingUploads.count ? pendingUploads[index].id : nil
                }
                let snapshot = try await connection.companionController.deleteDrafts(ids: ids)
                pendingUploads = snapshot.drafts
            } catch {
                connection.lastConnectionError = error.localizedDescription
            }
        }
    }
}

extension NSNotification.Name {
    static let nanoLibraryUpdated = NSNotification.Name("nanoLibraryUpdated")
}
