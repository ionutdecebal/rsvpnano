import SwiftUI
import shared

@MainActor
final class LibraryViewModel: ObservableObject {
    @Published var books: [NanoBook] = []
    @Published var showingPicker = false
    @Published var showingTextImport = false
    @Published var librarySummary: String = ""
    
    private let connection: NanoConnectionManager
    private var connectionObserver: Any?
    private var disconnectionObserver: Any?
    private var libraryUpdatedObserver: Any?
    
    init(connection: NanoConnectionManager = .shared) {
        self.connection = connection
        
        connectionObserver = NotificationCenter.default.addObserver(
            forName: .nanoDeviceConnected,
            object: nil,
            queue: .main
        ) { [weak self] notification in
            if let snapshot = notification.object as? shared.CompanionConnectSnapshot {
                self?.applySnapshot(snapshot.device)
            }
        }
        
        disconnectionObserver = NotificationCenter.default.addObserver(
            forName: .nanoDeviceDisconnected,
            object: nil,
            queue: .main
        ) { [weak self] _ in
            self?.books = []
            self?.librarySummary = ""
        }

        libraryUpdatedObserver = NotificationCenter.default.addObserver(
            forName: .nanoLibraryUpdated,
            object: nil,
            queue: .main
        ) { [weak self] notification in
            if let books = notification.object as? [NanoBook] {
                self?.books = books
                // Recalculate summary if needed or just wait for next refresh
            }
        }
    }
    
    func refreshBooks() {
        Task {
            await connection.run("Refreshing", requiresConnection: true) { [self] in
                // We need RSS feeds from the settings/inbox model if we wanted to merge them here,
                // but for now we focus on books.
                let snapshot = try await connection.companionController.refreshDevice(
                    baseUrl: connection.address,
                    localRssFeeds: [] 
                )
                self.books = snapshot.books
                self.librarySummary = snapshot.summaryText
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
            await connection.run(
                booksToDelete.count == 1 ? "Deleting \(booksToDelete[0].displayTitle)" : "Deleting books",
                requiresConnection: true
            ) { [self] in
                let snapshot = try await connection.companionController.deleteBooks(
                    baseUrl: connection.address,
                    bookIds: booksToDelete.map(\.id)
                )
                self.books = snapshot.books
                self.librarySummary = snapshot.summaryText
            }
        }
    }
    
    func upload(_ file: PickedBookFile) {
        Task {
            await connection.run("Preparing \(file.filename)", requiresConnection: true) { [self] in
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
            await connection.run("Uploading \(file.title)", requiresConnection: true) { [self] in
                try await self.uploadConverted(file)
            }
        }
    }

    private func uploadConverted(_ file: shared.RsvpBookFile) async throws {
        let snapshot = try await connection.companionController.uploadBook(
            baseUrl: connection.address, 
            file: file, 
            category: "book"
        )
        self.books = snapshot.books
        self.librarySummary = snapshot.summaryText
        connection.status = uploadStatus(for: file)
    }

    private func uploadStatus(for file: shared.RsvpBookFile) -> String {
        guard file.wordCount > 0 else {
            return "Uploaded \(file.title) into /books/books."
        }
        let wordLabel = file.wordCount == 1 ? "word" : "words"
        let chapterLabel = file.chapterCount == 1 ? "chapter" : "chapters"
        return "Uploaded \(file.title) into /books/books: \(file.wordCount) \(wordLabel), \(file.chapterCount) \(chapterLabel)."
    }
    
    private func applySnapshot(_ device: shared.NanoDeviceSnapshot) {
        self.books = device.books
        self.librarySummary = device.summaryText
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
