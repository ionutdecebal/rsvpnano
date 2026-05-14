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
    let category: String?

    var id: String { name }

    var displayTitle: String {
        title?.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty == false ? title! : filename
    }

    var filename: String {
        name.split(separator: "/").last.map(String.init) ?? name
    }

    var detailLabel: String {
        let cleanedAuthor = author?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        let pathLabel = displayTitle == filename ? nil : name
        return [cleanedAuthor.isEmpty ? nil : cleanedAuthor, pathLabel, byteLabel]
            .compactMap { $0 }
            .joined(separator: " · ")
    }

    var isArticle: Bool {
        category == "article" || name.lowercased().hasPrefix("articles/")
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

struct NanoRssFeeds: Codable {
    var ok: Bool
    var feeds: [String]
}

struct NanoSettings: Codable {
    var ok: Bool
    var version: Int
    var reading: Reading
    var display: Display
    var typography: Typography
    var limits: Limits?

    struct Reading: Codable {
        var wpm: Int
        var readerMode: String
        var pauseMode: String
        var accurateTimeEstimate: Bool
        var pacing: Pacing
    }

    struct Pacing: Codable {
        var longWordMs: Int
        var complexWordMs: Int
        var punctuationMs: Int
    }

    struct Display: Codable {
        var brightnessIndex: Int
        var darkMode: Bool
        var nightMode: Bool
        var handedness: String
        var footerMetric: String
        var batteryLabel: String
        var language: Int
        var phantomWords: Bool
        var fontSizeIndex: Int
    }

    struct Typography: Codable {
        var typeface: String
        var focusHighlight: Bool
        var tracking: Int
        var anchorPercent: Int
        var guideWidth: Int
        var guideGap: Int
    }

    struct Limits: Codable {
        var wpm: RangeLimit?
        var brightnessIndex: RangeLimit?
        var pacingMs: RangeLimit?
        var tracking: RangeLimit?
        var anchorPercent: RangeLimit?
        var guideWidth: RangeLimit?
        var guideGap: RangeLimit?
    }

    struct RangeLimit: Codable {
        var min: Int
        var max: Int
    }
}
