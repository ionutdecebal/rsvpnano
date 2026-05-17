import Foundation
import shared

enum SharedInbox {
    static let appGroupIdentifier = "group.com.rsvpnano.companion"
}

extension shared.NanoBook: Identifiable {
    var displayTitle: String {
        title?.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty == false ? title! : filename
    }

    var filename: String {
        id.split(separator: "/").last.map(String.init) ?? id
    }

    var detailLabel: String {
        let cleanedAuthor = author?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        let pathLabel = displayTitle == filename ? nil : id
        return [cleanedAuthor.isEmpty ? nil : cleanedAuthor, pathLabel, byteLabel]
            .compactMap { $0 }
            .joined(separator: " · ")
    }

    var isArticle: Bool {
        category == "article" || id.lowercased().hasPrefix("articles/")
    }

    var byteLabel: String {
        ByteCountFormatter.string(fromByteCount: Int64(bytes), countStyle: .file)
    }
}

struct PendingUpload: Identifiable {
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
}

// Keep Swift UI code concise while shared remains the source of truth.
typealias NanoInfo = shared.NanoInfo
typealias NanoBook = shared.NanoBook
typealias NanoWifiSettings = shared.NanoWifiSettings
typealias NanoSettings = shared.NanoSettings
