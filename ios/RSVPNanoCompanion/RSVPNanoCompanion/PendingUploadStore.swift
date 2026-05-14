import Foundation

enum SharedInbox {
    static let appGroupIdentifier = "group.com.rsvpnano.companion"
}

struct PendingUpload: Codable, Identifiable {
    let id: UUID
    let title: String
    let source: String
    let body: String
    let createdAt: Date

    var bytes: Int {
        Data(body.utf8).count
    }

    var needsArticleFetch: Bool {
        guard let url = URL(string: source), ["http", "https"].contains(url.scheme?.lowercased()) else {
            return false
        }
        return body.trimmingCharacters(in: .whitespacesAndNewlines) == source.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    init(id: UUID = UUID(), title: String, source: String, body: String, createdAt: Date = Date()) {
        self.id = id
        self.title = title
        self.source = source
        self.body = body
        self.createdAt = createdAt
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        id = try container.decode(UUID.self, forKey: .id)
        title = try container.decode(String.self, forKey: .title)
        source = try container.decode(String.self, forKey: .source)
        body = try container.decodeIfPresent(String.self, forKey: .body) ?? ""
        createdAt = try container.decode(Date.self, forKey: .createdAt)
    }
}

enum PendingUploadStoreError: LocalizedError {
    case sharedContainerUnavailable
    case emptyDraft
    case saveVerificationFailed

    var errorDescription: String? {
        switch self {
        case .sharedContainerUnavailable:
            return "The shared article inbox is not available. Check that App Groups are enabled for the app and share extension."
        case .emptyDraft:
            return "Add some text before saving."
        case .saveVerificationFailed:
            return "The article was written but could not be verified in the shared inbox."
        }
    }
}

struct PendingUploadStore {
    private let fileManager = FileManager.default

    var rootURL: URL {
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
            try rootURL.appendingPathComponent("drafts.json")
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

    func saveDraft(title: String, source: String, body: String) throws -> PendingUpload {
        let cleanedTitle = title.trimmingCharacters(in: .whitespacesAndNewlines)
        let cleanedBody = body.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !cleanedBody.isEmpty else {
            throw PendingUploadStoreError.emptyDraft
        }

        let item = PendingUpload(
            title: cleanedTitle.isEmpty ? "Shared Article" : cleanedTitle,
            source: source.trimmingCharacters(in: .whitespacesAndNewlines),
            body: cleanedBody
        )

        var items = try all()
        items.insert(item, at: 0)
        try write(items)
        guard try all().contains(where: { $0.id == item.id }) else {
            throw PendingUploadStoreError.saveVerificationFailed
        }
        return item
    }

    func bookFile(for item: PendingUpload) throws -> RsvpBookFile {
        let article = ArticleFormatter.article(title: item.title, source: item.source, htmlOrText: item.body)
        return try RsvpConverter.rsvpFile(
            title: article.title,
            author: "",
            source: article.source,
            events: ArticleFormatter.events(from: article)
        )
    }

    func update(_ item: PendingUpload, title: String, body: String) throws {
        let cleanedBody = body.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !cleanedBody.isEmpty else {
            throw PendingUploadStoreError.emptyDraft
        }
        let cleanedTitle = title.trimmingCharacters(in: .whitespacesAndNewlines)
        let updated = PendingUpload(
            id: item.id,
            title: cleanedTitle.isEmpty ? item.title : cleanedTitle,
            source: item.source,
            body: cleanedBody,
            createdAt: item.createdAt
        )
        try write(all().map { $0.id == item.id ? updated : $0 })
    }

    func delete(_ item: PendingUpload) throws {
        try write(all().filter { $0.id != item.id })
    }

    func delete(at offsets: IndexSet) throws {
        let items = try all()
        let ids = Set(offsets.compactMap { index in
            index < items.count ? items[index].id : nil
        })
        try write(items.filter { !ids.contains($0.id) })
    }

    private func write(_ items: [PendingUpload]) throws {
        let data = try JSONEncoder().encode(items.sorted { $0.createdAt > $1.createdAt })
        try data.write(to: try indexURL, options: .atomic)
    }
}
