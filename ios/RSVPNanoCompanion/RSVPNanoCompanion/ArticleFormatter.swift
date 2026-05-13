import Foundation

struct SharedArticle {
    let title: String
    let source: String
    let text: String
}

enum ArticleFormatter {
    static func article(title: String, source: String, htmlOrText: String) -> SharedArticle {
        let title = RsvpConverter.cleanedLine(title).isEmpty
            ? RsvpConverter.titleFromText(htmlOrText, fallback: fallbackTitle(from: source))
            : title

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
        let cleaned = html
            .replacingOccurrences(of: "(?is)<script.*?</script>", with: " ", options: .regularExpression)
            .replacingOccurrences(of: "(?is)<style.*?</style>", with: " ", options: .regularExpression)
            .replacingOccurrences(of: "(?is)<svg.*?</svg>", with: " ", options: .regularExpression)
            .replacingOccurrences(of: "(?is)<(nav|header|footer|aside|form|noscript)[^>]*>.*?</\\1>", with: " ", options: .regularExpression)

        for pattern in [
            "(?is)<article[^>]*>(.*?)</article>",
            "(?is)<main[^>]*>(.*?)</main>",
            "(?is)<[^>]+role=[\"']main[\"'][^>]*>(.*?)</[^>]+>",
            "(?is)<body[^>]*>(.*?)</body>",
        ] {
            if let match = firstMatch(in: cleaned, pattern: pattern) {
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

    private static func firstMatch(in value: String, pattern: String) -> String? {
        guard let regex = try? NSRegularExpression(pattern: pattern),
              let match = regex.firstMatch(in: value, range: NSRange(value.startIndex..., in: value)),
              match.numberOfRanges > 1,
              let range = Range(match.range(at: 1), in: value) else {
            return nil
        }
        return String(value[range])
    }
}
