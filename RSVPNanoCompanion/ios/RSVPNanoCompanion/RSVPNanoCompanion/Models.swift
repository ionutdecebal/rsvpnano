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

extension shared.PendingUpload: Identifiable {
    var bytes: Int {
        Data(body.utf8).count
    }

    var byteLabel: String {
        ByteCountFormatter.string(fromByteCount: Int64(bytes), countStyle: .file)
    }

    var displayDate: String {
        shared.SharedAppUtils.shared.formatCreatedAt(iso8601: createdAt)
    }

    var needsArticleFetch: Bool {
        guard let sourceUrl = sourceUrl,
              ["http", "https"].contains(URL(string: sourceUrl)?.scheme?.lowercased()) else {
            return false
        }
        return body.trimmingCharacters(in: .whitespacesAndNewlines) == sourceUrl.trimmingCharacters(in: .whitespacesAndNewlines)
    }
}

// Keep Swift UI code concise while shared remains the source of truth.
typealias NanoInfo = shared.NanoInfo
typealias NanoBook = shared.NanoBook
typealias PendingUpload = shared.PendingUpload
typealias NanoWifiSettings = shared.NanoWifiSettings
typealias NanoSettings = shared.NanoSettings
