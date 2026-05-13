import Foundation

struct NanoInfo: Decodable {
    let name: String
    let mode: String
    let baseUrl: String
    let networkSsid: String?
    let pairingCode: String
    let uploadPath: String
}

struct NanoBooksResponse: Decodable {
    let books: [NanoBook]
}

struct NanoBook: Decodable, Identifiable {
    let name: String
    let title: String?
    let author: String?
    let bytes: Int
    let progressPercent: Int?

    var id: String { name }

    var displayTitle: String {
        title?.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty == false ? title! : name
    }

    var detailLabel: String {
        let cleanedAuthor = author?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        let filename = displayTitle == name ? nil : name
        return [cleanedAuthor.isEmpty ? nil : cleanedAuthor, filename, byteLabel]
            .compactMap { $0 }
            .joined(separator: " · ")
    }

    var byteLabel: String {
        ByteCountFormatter.string(fromByteCount: Int64(bytes), countStyle: .file)
    }
}

struct NanoUploadResponse: Decodable {
    let ok: Bool
    let path: String?
    let error: String?
}
