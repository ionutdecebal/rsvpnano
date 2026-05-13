import Foundation

struct SharedArticle {
    let title: String
    let source: String
    let text: String
}

enum ArticleFetchError: LocalizedError {
    case invalidURL
    case articleTooLarge
    case emptyArticle

    var errorDescription: String? {
        switch self {
        case .invalidURL:
            return "The article URL is not valid."
        case .articleTooLarge:
            return "The page is too large to fetch safely."
        case .emptyArticle:
            return "No readable article text was found."
        }
    }
}

enum ArticleFetchService {
    private static let maxFetchedBytes = 900_000
    private static let maxTextCharacters = 250_000

    static func fetch(title: String, source: String) async throws -> SharedArticle {
        guard let url = URL(string: source), ["http", "https"].contains(url.scheme?.lowercased()) else {
            throw ArticleFetchError.invalidURL
        }

        return try await Task.detached(priority: .userInitiated) {
            var request = URLRequest(url: url, timeoutInterval: 15)
            request.setValue("Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 Safari/604.1", forHTTPHeaderField: "User-Agent")
            let (data, response) = try await URLSession.shared.data(for: request)
            if let response = response as? HTTPURLResponse,
               let length = response.value(forHTTPHeaderField: "Content-Length"),
               let bytes = Int(length),
               bytes > maxFetchedBytes {
                throw ArticleFetchError.articleTooLarge
            }
            guard data.count <= maxFetchedBytes else {
                throw ArticleFetchError.articleTooLarge
            }
            let limited = data.prefix(maxFetchedBytes)
            let raw = RsvpConverter.decodeText(Data(limited)) ?? String(data: limited, encoding: .utf8) ?? ""
            let clipped = String(raw.prefix(maxTextCharacters))
            let article = ArticleFormatter.article(title: title, source: source, htmlOrText: clipped)
            guard !article.text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
                throw ArticleFetchError.emptyArticle
            }
            return article
        }.value
    }
}

enum ArticleFormatter {
    static func article(title: String, source: String, htmlOrText: String) -> SharedArticle {
        let title = articleTitle(title: title, source: source, htmlOrText: htmlOrText)

        if looksLikeHTML(htmlOrText) {
            let focused = focusedHTML(from: htmlOrText)
            let text = RsvpConverter.readableText(from: focused)
            return SharedArticle(title: title, source: source, text: text)
        }

        return SharedArticle(title: title, source: source, text: RsvpConverter.readableText(from: htmlOrText))
    }

    static func events(from article: SharedArticle) -> [RsvpEvent] {
        if looksLikeHTML(article.text) {
            return RsvpConverter.htmlEvents(article.text)
        }
        return RsvpConverter.textEvents(article.text)
    }

    private static func focusedHTML(from html: String) -> String {
        let cleaned = removingBlocks(
            from: html,
            tags: ["script", "style", "svg", "nav", "header", "footer", "aside", "form", "noscript"]
        )

        for tag in ["article", "main", "body"] {
            if let match = firstElementContent(in: cleaned, tag: tag) {
                return match
            }
        }
        return cleaned
    }

    private static func looksLikeHTML(_ value: String) -> Bool {
        let lowered = value.lowercased()
        return lowered.contains("<html") || lowered.contains("<body") || lowered.contains("<article") ||
            lowered.contains("<main") || lowered.contains("<p")
    }

    private static func fallbackTitle(from source: String) -> String {
        guard let url = URL(string: source), let host = url.host, !host.isEmpty else {
            return "Shared Article"
        }
        return host
    }

    private static func articleTitle(title: String, source: String, htmlOrText: String) -> String {
        let cleanedTitle = RsvpConverter.cleanedLine(title)
        if !cleanedTitle.isEmpty, !isPlaceholderTitle(cleanedTitle, source: source) {
            return cleanedTitle
        }
        if looksLikeHTML(htmlOrText), let htmlTitle = htmlTitle(from: htmlOrText) {
            return htmlTitle
        }
        return RsvpConverter.titleFromText(htmlOrText, fallback: fallbackTitle(from: source))
    }

    private static func isPlaceholderTitle(_ title: String, source: String) -> Bool {
        guard let url = URL(string: source) else {
            return false
        }
        let host = url.host ?? ""
        return title == source || title == url.absoluteString || title == host || title == "www.\(host)"
    }

    private static func htmlTitle(from html: String) -> String? {
        guard let title = firstElementContent(in: html, tag: "title") else {
            return nil
        }
        let cleaned = RsvpConverter.cleanedLine(RsvpConverter.readableText(from: title))
        return cleaned.isEmpty ? nil : String(cleaned.prefix(120))
    }

    private static func firstElementContent(in value: String, tag: String) -> String? {
        guard let openStart = value.range(of: "<\(tag)", options: [.caseInsensitive]),
              let openEnd = value[openStart.lowerBound...].range(of: ">"),
              let close = value[openEnd.upperBound...].range(of: "</\(tag)>", options: [.caseInsensitive]) else {
            return nil
        }
        return String(value[openEnd.upperBound..<close.lowerBound])
    }

    private static func removingBlocks(from value: String, tags: [String]) -> String {
        var result = value
        for tag in tags {
            var searchStart = result.startIndex
            var removedCount = 0
            while removedCount < 80,
                  let open = result[searchStart...].range(of: "<\(tag)", options: [.caseInsensitive]),
                  let close = result[open.lowerBound...].range(of: "</\(tag)>", options: [.caseInsensitive]),
                  let closeEnd = result[close.upperBound...].range(of: ">") {
                result.replaceSubrange(open.lowerBound..<closeEnd.upperBound, with: " ")
                searchStart = open.lowerBound
                removedCount += 1
                if searchStart == result.endIndex {
                    break
                }
            }
        }
        return result
    }
}
