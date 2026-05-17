import Foundation
import shared

enum SharedInbox {
    static let appGroupIdentifier = "group.com.rsvpnano.companion"
}

extension shared.NanoInfo {
    // Shared NanoInfo now has name, mode, baseUrl, networkSsid, pairingCode, uploadPath
}

extension shared.NanoBook: Identifiable {
    public var id: String { apiName }

    var displayTitle: String {
        title?.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty == false ? title! : filename
    }

    var filename: String {
        apiName.split(separator: "/").last.map(String.init) ?? apiName
    }

    var detailLabel: String {
        let cleanedAuthor = author?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        let pathLabel = displayTitle == filename ? nil : apiName
        return [cleanedAuthor.isEmpty ? nil : cleanedAuthor, pathLabel, byteLabel]
            .compactMap { $0 }
            .joined(separator: " · ")
    }

    var isArticle: Bool {
        category == "article" || apiName.lowercased().hasPrefix("articles/")
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

// Typealiases to maintain compatibility in the Swift codebase while using shared types
typealias NanoInfo = shared.NanoInfo
typealias NanoBook = shared.NanoBook
typealias NanoUploadResponse = shared.NanoUploadResponse
typealias NanoRssFeeds = shared.NanoRssFeeds
typealias NanoWifiSettings = shared.NanoWifiSettings
typealias NanoWifiUpdate = shared.NanoWifiUpdate
typealias NanoSettings = shared.NanoSettings
typealias NanoBooksResponse = shared.NanoBooksResponse
typealias RsvpBookFile = shared.RsvpBookFile
