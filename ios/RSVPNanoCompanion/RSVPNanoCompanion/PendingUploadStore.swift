import Foundation

enum SharedInbox {
    static let appGroupIdentifier = "group.com.rsvpnano.companion"
}

struct PendingUpload: Codable, Identifiable {
    let id: UUID
    let title: String
    let source: String
    let filename: String
    let createdAt: Date
    let bytes: Int
}

enum PendingUploadStoreError: LocalizedError {
    case sharedContainerUnavailable
    case missingFile

    var errorDescription: String? {
        switch self {
        case .sharedContainerUnavailable:
            return "The shared article inbox is not available."
        case .missingFile:
            return "The saved article file is missing."
        }
    }
}

struct PendingUploadStore {
    private let fileManager = FileManager.default

    private var rootURL: URL {
        get throws {
            guard let url = fileManager.containerURL(forSecurityApplicationGroupIdentifier: SharedInbox.appGroupIdentifier) else {
                throw PendingUploadStoreError.sharedContainerUnavailable
            }
            let inbox = url.appendingPathComponent("PendingUploads", isDirectory: true)
            try fileManager.createDirectory(at: inbox, withIntermediateDirectories: true)
            return inbox
        }
    }

    private var indexURL: URL {
        get throws {
            try rootURL.appendingPathComponent("index.json")
        }
    }

    func all() throws -> [PendingUpload] {
        let url = try indexURL
        guard fileManager.fileExists(atPath: url.path) else {
            return []
        }
        let data = try Data(contentsOf: url)
        return try JSONDecoder().decode([PendingUpload].self, from: data)
            .sorted { $0.createdAt > $1.createdAt }
    }

    func save(_ file: RsvpBookFile, source: String) throws -> PendingUpload {
        let item = PendingUpload(
            id: UUID(),
            title: file.title,
            source: source,
            filename: file.filename,
            createdAt: Date(),
            bytes: file.data.count
        )

        let dataURL = try fileURL(for: item)
        try file.data.write(to: dataURL, options: .atomic)

        var items = try all()
        items.insert(item, at: 0)
        try write(items)
        return item
    }

    func data(for item: PendingUpload) throws -> Data {
        let url = try fileURL(for: item)
        guard fileManager.fileExists(atPath: url.path) else {
            throw PendingUploadStoreError.missingFile
        }
        return try Data(contentsOf: url)
    }

    func delete(_ item: PendingUpload) throws {
        let items = try all().filter { $0.id != item.id }
        let url = try fileURL(for: item)
        if fileManager.fileExists(atPath: url.path) {
            try fileManager.removeItem(at: url)
        }
        try write(items)
    }

    func delete(at offsets: IndexSet) throws {
        let items = try all()
        for index in offsets where index < items.count {
            try delete(items[index])
        }
    }

    private func fileURL(for item: PendingUpload) throws -> URL {
        try rootURL.appendingPathComponent("\(item.id.uuidString)-\(item.filename)")
    }

    private func write(_ items: [PendingUpload]) throws {
        let data = try JSONEncoder().encode(items.sorted { $0.createdAt > $1.createdAt })
        try data.write(to: try indexURL, options: .atomic)
    }
}
