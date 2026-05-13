import Foundation

struct RsvpBookFile {
    let filename: String
    let data: Data
    let title: String
    let wordCount: Int
    let chapterCount: Int

    init(filename: String, data: Data, title: String, wordCount: Int = 0, chapterCount: Int = 0) {
        self.filename = filename
        self.data = data
        self.title = title
        self.wordCount = wordCount
        self.chapterCount = chapterCount
    }
}

enum RsvpConversionError: LocalizedError {
    case emptyText
    case unreadableText
    case unsupportedEpub

    var errorDescription: String? {
        switch self {
        case .emptyText:
            return "There is no readable text to convert."
        case .unreadableText:
            return "This file is not readable as text yet."
        case .unsupportedEpub:
            return "This EPUB could not be converted locally."
        }
    }
}

enum RsvpConverter {
    fileprivate static let wrapWidth = 96
    private static let blockTags = [
        "address", "article", "aside", "blockquote", "body", "br", "dd", "div", "dl", "dt",
        "figcaption", "figure", "footer", "header", "hr", "li", "main", "ol", "p", "pre",
        "section", "table", "tbody", "td", "tfoot", "th", "thead", "tr", "ul",
    ]
    private static let skipTags = ["head", "math", "nav", "script", "style", "svg"]
    private static let asciiReplacements: [(Character, String)] = [
        ("\u{00a0}", " "), ("\u{1680}", " "), ("\u{180e}", " "), ("\u{2000}", " "), ("\u{2001}", " "),
        ("\u{2002}", " "), ("\u{2003}", " "), ("\u{2004}", " "), ("\u{2005}", " "), ("\u{2006}", " "),
        ("\u{2007}", " "), ("\u{2008}", " "), ("\u{2009}", " "), ("\u{200a}", " "), ("\u{2028}", " "),
        ("\u{2029}", " "), ("\u{202f}", " "), ("\u{205f}", " "), ("\u{3000}", " "),
        ("\u{2018}", "'"), ("\u{2019}", "'"), ("\u{201a}", "'"), ("\u{201b}", "'"), ("\u{2032}", "'"),
        ("\u{2035}", "'"), ("\u{201c}", "\""), ("\u{201d}", "\""), ("\u{201e}", "\""),
        ("\u{201f}", "\""), ("\u{00ab}", "\""), ("\u{00bb}", "\""), ("\u{2039}", "'"),
        ("\u{203a}", "'"), ("\u{2033}", "\""), ("\u{2036}", "\""), ("\u{300c}", "\""),
        ("\u{300d}", "\""), ("\u{300e}", "\""), ("\u{300f}", "\""), ("\u{2010}", "-"),
        ("\u{2011}", "-"), ("\u{2012}", "-"), ("\u{2013}", "-"), ("\u{2014}", "-"),
        ("\u{2015}", "-"), ("\u{2043}", "-"), ("\u{2212}", "-"), ("\u{2026}", "..."),
        ("\u{2022}", "*"), ("\u{00b7}", "*"), ("\u{2219}", "*"), ("\u{00a9}", "(c)"),
        ("\u{00ae}", "(r)"), ("\u{2122}", "TM"), ("\u{fb00}", "ff"), ("\u{fb01}", "fi"),
        ("\u{fb02}", "fl"), ("\u{fb03}", "ffi"), ("\u{fb04}", "ffl"), ("\u{fb05}", "st"),
        ("\u{fb06}", "st"), ("\u{fffd}", ""),
    ]

    static func bookFile(data: Data, filename: String) throws -> RsvpBookFile {
        if filename.lowercased().hasSuffix(".rsvp") {
            return RsvpBookFile(filename: filename, data: data, title: filenameWithoutExtension(filename))
        }

        if filename.lowercased().hasSuffix(".epub") {
            return try EpubConverter.convert(data: data, filename: filename)
        }

        guard let rawText = decodeText(data) else {
            throw RsvpConversionError.unreadableText
        }

        let events: [RsvpEvent]
        let title: String
        if looksLikeHTML(rawText) {
            title = titleFromText(rawText, fallback: filenameWithoutExtension(filename))
            events = htmlEvents(rawText)
        } else {
            title = filenameWithoutExtension(filename)
            events = textEvents(rawText)
        }
        return try rsvpFile(title: title, author: "", source: filename, events: events)
    }

    static func rsvpFile(title: String, source: String, text: String) throws -> RsvpBookFile {
        try rsvpFile(title: title, author: "", source: source, events: textEvents(text))
    }

    static func rsvpFile(title: String, author: String, source: String, events: [RsvpEvent]) throws -> RsvpBookFile {
        let writer = RsvpWriter(title: title, author: author, source: source)
        for event in events {
            switch event {
            case .chapter(let title):
                writer.addChapter(title)
            case .text(let text):
                writer.beginParagraph()
                writer.addText(text)
            }
        }
        return try writer.finalize(fallbackChapterTitle: title)
    }

    static func readableText(from value: String) -> String {
        var text = value
        if looksLikeHTML(text) {
            text = stripHTML(text)
        }
        text = text.replacingOccurrences(of: "\r\n", with: "\n")
        text = text.replacingOccurrences(of: "\r", with: "\n")
        return paragraphs(from: text).joined(separator: "\n\n")
    }

    static func titleFromText(_ text: String, fallback: String) -> String {
        if looksLikeHTML(text), let title = firstMatch(in: text, pattern: "<title[^>]*>(.*?)</title>") {
            let cleaned = cleanedLine(stripHTML(title))
            if !cleaned.isEmpty {
                return cleaned
            }
        }

        for line in readableText(from: text).components(separatedBy: .newlines) {
            let cleaned = cleanedLine(line)
            if !cleaned.isEmpty {
                return String(cleaned.prefix(80))
            }
        }
        return fallback
    }

    static func htmlEvents(_ markup: String) -> [RsvpEvent] {
        var text = markup
        for tag in skipTags {
            text = text.replacingOccurrences(of: "(?is)<\(tag)\\b.*?</\(tag)>", with: " ", options: .regularExpression)
        }
        text = text.replacingOccurrences(of: "(?is)<h[1-6][^>]*>(.*?)</h[1-6]>", with: "\n@chapter $1\n", options: .regularExpression)
        text = text.replacingOccurrences(of: "(?i)</?(\(blockTags.joined(separator: "|")))\\b[^>]*>", with: "\n", options: .regularExpression)
        text = text.replacingOccurrences(of: "(?is)<[^>]+>", with: " ", options: .regularExpression)
        text = decodeEntities(text)

        var events: [RsvpEvent] = []
        for rawLine in text.components(separatedBy: .newlines) {
            let line = cleanedLine(rawLine)
            if line.isEmpty {
                continue
            }
            if line.lowercased().hasPrefix("@chapter ") {
                let value = cleanedLine(String(line.dropFirst("@chapter ".count)))
                if !value.isEmpty {
                    events.append(.chapter(value))
                }
            } else {
                events.append(.text(line))
            }
        }
        return events
    }

    static func textEvents(_ text: String) -> [RsvpEvent] {
        var events: [RsvpEvent] = []
        var paragraph: [String] = []

        func flushParagraph() {
            let text = cleanedLine(paragraph.joined(separator: " "))
            paragraph.removeAll()
            if !text.isEmpty {
                events.append(.text(text))
            }
        }

        for rawLine in text.components(separatedBy: .newlines) {
            let line = cleanedLine(rawLine)
            if let chapter = chapterTitle(from: line) {
                flushParagraph()
                events.append(.chapter(chapter))
            } else if line.isEmpty {
                flushParagraph()
            } else {
                paragraph.append(line)
            }
        }
        flushParagraph()
        return events
    }

    static func filenameSafe(_ value: String) -> String {
        let allowed = CharacterSet.alphanumerics.union(CharacterSet(charactersIn: " -_"))
        let mapped = value.unicodeScalars.map { allowed.contains($0) ? Character($0) : "-" }
        let collapsed = String(mapped).replacingOccurrences(of: "\\s+", with: " ", options: .regularExpression)
        let trimmed = collapsed.trimmingCharacters(in: .whitespacesAndNewlines)
        return trimmed.isEmpty ? "Untitled" : String(trimmed.prefix(80))
    }

    static func decodeText(_ data: Data) -> String? {
        if data.starts(with: [0xef, 0xbb, 0xbf]) {
            return String(data: Data(data.dropFirst(3)), encoding: .utf8)
        }
        if data.starts(with: [0xff, 0xfe]) {
            return String(data: Data(data.dropFirst(2)), encoding: .utf16LittleEndian)
        }
        if data.starts(with: [0xfe, 0xff]) {
            return String(data: Data(data.dropFirst(2)), encoding: .utf16BigEndian)
        }

        let initialEncoding = detectUtf16WithoutBom(data) ?? .utf8
        let decoded = String(data: data, encoding: initialEncoding)
            ?? String(data: data, encoding: .windowsCP1252)
            ?? String(data: data, encoding: .isoLatin1)
        guard let decoded else {
            return nil
        }
        if let declared = sniffDeclaredEncoding(decoded),
           declared != initialEncoding,
           let redecode = String(data: data, encoding: declared) {
            return redecode
        }
        return decoded
    }

    static func filenameWithoutExtension(_ filename: String) -> String {
        let url = URL(fileURLWithPath: filename)
        let stem = url.deletingPathExtension().lastPathComponent
        return stem.isEmpty ? filename : stem
    }

    private static func looksLikeHTML(_ value: String) -> Bool {
        let lowered = value.lowercased()
        return lowered.contains("<html") || lowered.contains("<body") || lowered.contains("<p") ||
            lowered.contains("<article") || lowered.contains("<br")
    }

    private static func stripHTML(_ value: String) -> String {
        var text = value
        for tag in skipTags {
            text = text.replacingOccurrences(of: "(?is)<\(tag)\\b.*?</\(tag)>", with: " ", options: .regularExpression)
        }
        text = text.replacingOccurrences(of: "(?i)</?(\(blockTags.joined(separator: "|"))|h[1-6])\\b[^>]*>", with: "\n", options: .regularExpression)
        text = text.replacingOccurrences(of: "(?is)<[^>]+>", with: " ", options: .regularExpression)
        return decodeEntities(text)
    }

    static func decodeEntities(_ value: String) -> String {
        var text = value
        let replacements = [
            "&amp;": "&",
            "&lt;": "<",
            "&gt;": ">",
            "&quot;": "\"",
            "&#39;": "'",
            "&apos;": "'",
            "&nbsp;": " ",
        ]
        for (entity, replacement) in replacements {
            text = text.replacingOccurrences(of: entity, with: replacement)
        }
        text = replaceNumericEntities(in: text)
        return text
    }

    private static func paragraphs(from text: String) -> [String] {
        text.components(separatedBy: .newlines)
            .map(cleanedLine)
            .filter { !$0.isEmpty }
    }

    static func cleanedLine(_ value: String) -> String {
        normalizedText(value)
            .trimmingCharacters(in: .whitespacesAndNewlines)
    }

    static func directiveValue(_ value: String) -> String {
        cleanedLine(value).replacingOccurrences(of: "\n", with: " ")
    }

    static func cleanWordTokens(_ text: String) -> [String] {
        cleanedLine(text)
            .components(separatedBy: .whitespacesAndNewlines)
            .filter { token in
                !token.isEmpty && token.unicodeScalars.contains { CharacterSet.alphanumerics.contains($0) }
            }
    }

    static func chapterTitle(from line: String) -> String? {
        let trimmed = cleanedLine(line)
        if trimmed.isEmpty || trimmed.count > 64 {
            return nil
        }
        if trimmed.hasPrefix("#") {
            let title = cleanedLine(trimmed.replacingOccurrences(of: "^#+", with: "", options: .regularExpression))
            return title.isEmpty ? nil : title
        }
        let lowered = trimmed.lowercased()
        if lowered.hasPrefix("chapter ") || lowered.hasPrefix("part ") || lowered.hasPrefix("book ") {
            return trimmed
        }
        return nil
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

    private static func normalizedText(_ text: String) -> String {
        var value = text
        value = String(value.map { asciiReplacement(for: $0) ?? String($0) }.joined())
        value = value.replacingOccurrences(of: "[\\r\\n\\t]+", with: " ", options: .regularExpression)
        value = value.precomposedStringWithCanonicalMapping
        return value.replacingOccurrences(of: "\\s+", with: " ", options: .regularExpression)
    }

    private static func asciiReplacement(for character: Character) -> String? {
        asciiReplacements.first { $0.0 == character }?.1
    }

    private static func detectUtf16WithoutBom(_ data: Data) -> String.Encoding? {
        guard data.count >= 4 else {
            return nil
        }
        if data[0] == 0x3c, data[1] == 0x00, data[2] == 0x3f, data[3] == 0x00 {
            return .utf16LittleEndian
        }
        if data[0] == 0x00, data[1] == 0x3c, data[2] == 0x00, data[3] == 0x3f {
            return .utf16BigEndian
        }
        return nil
    }

    private static func sniffDeclaredEncoding(_ text: String) -> String.Encoding? {
        let head = String(text.prefix(512))
        guard let value = firstMatch(in: head, pattern: #"(?i)(?:encoding|charset)\s*=\s*["']?\s*([^"'>\s/]+)"#) else {
            return nil
        }
        switch value.lowercased() {
        case "utf-8", "utf8":
            return .utf8
        case "utf-16", "utf16":
            return .utf16
        case "utf-16le":
            return .utf16LittleEndian
        case "utf-16be":
            return .utf16BigEndian
        case "windows-1252", "cp1252":
            return .windowsCP1252
        case "iso-8859-1", "latin1":
            return .isoLatin1
        default:
            return nil
        }
    }

    private static func replaceNumericEntities(in text: String) -> String {
        guard let regex = try? NSRegularExpression(pattern: #"&#(x[0-9a-fA-F]+|\d+);"#) else {
            return text
        }
        let nsText = text as NSString
        var result = ""
        var cursor = 0
        let range = NSRange(location: 0, length: nsText.length)
        for match in regex.matches(in: text, range: range) {
            result += nsText.substring(with: NSRange(location: cursor, length: match.range.location - cursor))
            let token = nsText.substring(with: match.range(at: 1))
            let scalarValue: UInt32?
            if token.lowercased().hasPrefix("x") {
                scalarValue = UInt32(token.dropFirst(), radix: 16)
            } else {
                scalarValue = UInt32(token, radix: 10)
            }
            if let scalarValue, let scalar = UnicodeScalar(scalarValue) {
                result += String(Character(scalar))
            }
            cursor = match.range.location + match.range.length
        }
        result += nsText.substring(from: cursor)
        return result
    }
}

enum RsvpEvent {
    case chapter(String)
    case text(String)
}

final class RsvpWriter {
    private let title: String
    private let author: String
    private var lines: [String]
    private var wordCount = 0
    private var chapterCount = 0
    private var lineWords: [String] = []
    private var lineLength = 0
    private var lastChapter = ""

    init(title: String, author: String, source: String) {
        self.title = RsvpConverter.directiveValue(title).isEmpty ? "Untitled" : RsvpConverter.directiveValue(title)
        self.author = RsvpConverter.directiveValue(author)
        self.lines = ["@rsvp 1", "@title \(self.title)"]
        if !self.author.isEmpty {
            lines.append("@author \(self.author)")
        }
        let cleanedSource = RsvpConverter.directiveValue(source)
        if !cleanedSource.isEmpty {
            lines.append("@source \(cleanedSource)")
        }
        lines.append("")
    }

    func addChapter(_ value: String) {
        let chapter = RsvpConverter.directiveValue(value)
        guard !chapter.isEmpty, chapter != lastChapter else {
            return
        }
        flushLine()
        if lines.last != "" {
            lines.append("")
        }
        lines.append("@chapter \(chapter)")
        chapterCount += 1
        lastChapter = chapter
    }

    func beginParagraph() {
        flushLine()
        if wordCount > 0 {
            if lines.last != "" {
                lines.append("")
            }
            lines.append("@para")
        }
    }

    func addText(_ text: String) {
        for word in RsvpConverter.cleanWordTokens(text) {
            let projected = lineWords.isEmpty ? word.count : lineLength + 1 + word.count
            if !lineWords.isEmpty && projected > RsvpConverter.wrapWidth {
                flushLine()
            }
            lineWords.append(word)
            lineLength = lineWords.count == 1 ? word.count : lineLength + 1 + word.count
            wordCount += 1
        }
    }

    func finalize(fallbackChapterTitle: String) throws -> RsvpBookFile {
        flushLine()
        guard wordCount > 0 else {
            throw RsvpConversionError.emptyText
        }
        if chapterCount == 0 {
            let chapter = "@chapter \(RsvpConverter.directiveValue(fallbackChapterTitle))"
            if let index = lines.firstIndex(of: "") {
                lines.insert(chapter, at: index)
            } else {
                lines.append(chapter)
            }
        }

        let body = lines.joined(separator: "\n").trimmingCharacters(in: .whitespacesAndNewlines) + "\n"
        return RsvpBookFile(
            filename: "\(RsvpConverter.filenameSafe(title)).rsvp",
            data: Data(body.utf8),
            title: title,
            wordCount: wordCount,
            chapterCount: max(chapterCount, 1)
        )
    }

    private func flushLine() {
        guard !lineWords.isEmpty else {
            return
        }
        var line = lineWords.joined(separator: " ")
        if line.hasPrefix("@") {
            line = "@\(line)"
        }
        lines.append(line)
        lineWords.removeAll()
        lineLength = 0
    }
}
