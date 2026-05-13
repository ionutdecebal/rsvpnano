import Compression
import Foundation

enum EpubConverter {
    static func convert(data: Data, filename: String) throws -> RsvpBookFile {
        let zip = try ZipArchive(data: data)
        let opfPath = try containerRootfile(in: zip)
        let package = try parsePackage(zip: zip, opfPath: opfPath)

        var events: [RsvpEvent] = []
        for (index, spinePath) in package.spinePaths.enumerated() {
            guard let chapterData = try zip.data(for: spinePath),
                  let markup = RsvpConverter.decodeText(chapterData) else {
                continue
            }
            var chapterEvents = RsvpConverter.htmlEvents(markup)
            if !chapterEvents.containsChapter {
                chapterEvents.insert(.chapter(fallbackChapterTitle(path: spinePath, index: index + 1)), at: 0)
            }
            if chapterEvents.containsText {
                events.append(contentsOf: chapterEvents)
            }
        }

        if events.isEmpty {
            throw RsvpConversionError.unsupportedEpub
        }

        return try RsvpConverter.rsvpFile(
            title: package.title.isEmpty ? RsvpConverter.filenameWithoutExtension(filename) : package.title,
            author: package.author,
            source: filename,
            events: events
        )
    }

    private static func containerRootfile(in zip: ZipArchive) throws -> String {
        guard let data = try zip.data(for: "META-INF/container.xml"),
              let xml = RsvpConverter.decodeText(data) else {
            throw RsvpConversionError.unsupportedEpub
        }
        let parser = RootfileParser()
        try parser.parse(xml)
        guard let path = parser.rootfilePath else {
            throw RsvpConversionError.unsupportedEpub
        }
        return normalizeZipPath(path)
    }

    private static func parsePackage(zip: ZipArchive, opfPath: String) throws -> EpubPackage {
        guard let data = try zip.data(for: opfPath),
              let xml = RsvpConverter.decodeText(data) else {
            throw RsvpConversionError.unsupportedEpub
        }

        let parser = PackageParser(opfPath: opfPath)
        try parser.parse(xml)
        let paths = parser.spinePaths.isEmpty ? parser.manifestContentPaths : parser.spinePaths
        if paths.isEmpty {
            throw RsvpConversionError.unsupportedEpub
        }
        return EpubPackage(title: parser.title, author: parser.author, spinePaths: paths)
    }

    private static func fallbackChapterTitle(path: String, index: Int) -> String {
        let name = URL(fileURLWithPath: path).deletingPathExtension().lastPathComponent
            .replacingOccurrences(of: "[_-]+", with: " ", options: .regularExpression)
        let cleaned = RsvpConverter.cleanedLine(name)
        return cleaned.isEmpty ? "Chapter \(index)" : cleaned
    }

    static func zipJoin(base: String, href: String) -> String {
        let withoutFragment = href.components(separatedBy: "#").first?
            .components(separatedBy: "?").first ?? href
        let decoded = withoutFragment.removingPercentEncoding ?? withoutFragment
        if decoded.hasPrefix("/") {
            return collapseZipPath(decoded)
        }
        return collapseZipPath(zipDirname(base) + decoded)
    }

    static func normalizeZipPath(_ path: String) -> String {
        path.replacingOccurrences(of: "\\", with: "/")
            .replacingOccurrences(of: "^/+", with: "", options: .regularExpression)
    }

    private static func zipDirname(_ path: String) -> String {
        let normalized = normalizeZipPath(path)
        guard let slash = normalized.lastIndex(of: "/") else {
            return ""
        }
        return String(normalized[...slash])
    }

    private static func collapseZipPath(_ path: String) -> String {
        var parts: [String] = []
        for part in normalizeZipPath(path).split(separator: "/") {
            if part == "." {
                continue
            }
            if part == ".." {
                _ = parts.popLast()
                continue
            }
            parts.append(String(part))
        }
        return parts.joined(separator: "/")
    }
}

private struct EpubPackage {
    let title: String
    let author: String
    let spinePaths: [String]
}

private extension Array where Element == RsvpEvent {
    var containsText: Bool {
        contains {
            if case .text = $0 {
                return true
            }
            return false
        }
    }

    var containsChapter: Bool {
        contains {
            if case .chapter = $0 {
                return true
            }
            return false
        }
    }
}

private final class RootfileParser: NSObject, XMLParserDelegate {
    private(set) var rootfilePath: String?

    func parse(_ xml: String) throws {
        let parser = XMLParser(data: Data(xml.utf8))
        parser.delegate = self
        if !parser.parse() {
            throw parser.parserError ?? RsvpConversionError.unsupportedEpub
        }
    }

    func parser(_ parser: XMLParser, didStartElement elementName: String, namespaceURI: String?, qualifiedName qName: String?, attributes attributeDict: [String: String] = [:]) {
        if localName(elementName) == "rootfile", let fullPath = attributeDict["full-path"] {
            rootfilePath = fullPath
        }
    }
}

private final class PackageParser: NSObject, XMLParserDelegate {
    private let opfPath: String
    private var manifest: [String: (path: String, mediaType: String)] = [:]
    private var activeElement = ""
    private var textBuffer = ""

    private(set) var title = ""
    private(set) var author = ""
    private(set) var spinePaths: [String] = []
    private(set) var manifestContentPaths: [String] = []

    init(opfPath: String) {
        self.opfPath = opfPath
    }

    func parse(_ xml: String) throws {
        let parser = XMLParser(data: Data(xml.utf8))
        parser.delegate = self
        if !parser.parse() {
            throw parser.parserError ?? RsvpConversionError.unsupportedEpub
        }
    }

    func parser(_ parser: XMLParser, didStartElement elementName: String, namespaceURI: String?, qualifiedName qName: String?, attributes attributeDict: [String: String] = [:]) {
        let name = localName(elementName)
        if name == "title" || name == "creator" {
            activeElement = name
            textBuffer = ""
        }

        if name == "item",
           let id = attributeDict["id"],
           let href = attributeDict["href"] {
            let mediaType = attributeDict["media-type"] ?? ""
            let path = EpubConverter.zipJoin(base: opfPath, href: href)
            manifest[id] = (path, mediaType)
            if isContentDocument(path: path, mediaType: mediaType) {
                manifestContentPaths.append(path)
            }
        }

        if name == "itemref",
           let idref = attributeDict["idref"],
           let item = manifest[idref],
           isContentDocument(path: item.path, mediaType: item.mediaType) {
            spinePaths.append(item.path)
        }
    }

    func parser(_ parser: XMLParser, foundCharacters string: String) {
        if activeElement == "title" || activeElement == "creator" {
            textBuffer += string
        }
    }

    func parser(_ parser: XMLParser, didEndElement elementName: String, namespaceURI: String?, qualifiedName qName: String?) {
        let name = localName(elementName)
        if name == activeElement {
            let cleaned = RsvpConverter.cleanedLine(textBuffer)
            if name == "title", title.isEmpty {
                title = cleaned
            } else if name == "creator", author.isEmpty {
                author = cleaned
            }
            activeElement = ""
            textBuffer = ""
        }
    }

    private func isContentDocument(path: String, mediaType: String) -> Bool {
        let loweredPath = path.lowercased()
        let loweredType = mediaType.lowercased()
        return loweredType == "application/xhtml+xml" ||
            loweredType == "text/html" ||
            loweredPath.hasSuffix(".xhtml") ||
            loweredPath.hasSuffix(".html") ||
            loweredPath.hasSuffix(".htm")
    }
}

private func localName(_ value: String) -> String {
    value.lowercased().components(separatedBy: ":").last ?? value.lowercased()
}

private struct ZipArchive {
    private let data: Data
    private let entries: [String: ZipEntry]

    init(data: Data) throws {
        self.data = data
        self.entries = try ZipArchive.readEntries(data: data)
    }

    func data(for path: String) throws -> Data? {
        let normalized = EpubConverter.normalizeZipPath(path).lowercased()
        guard let entry = entries[normalized] else {
            return nil
        }

        let localOffset = Int(entry.localHeaderOffset)
        guard data.uint32(at: localOffset) == 0x04034b50 else {
            throw RsvpConversionError.unsupportedEpub
        }
        let nameLength = Int(data.uint16(at: localOffset + 26))
        let extraLength = Int(data.uint16(at: localOffset + 28))
        let payloadOffset = localOffset + 30 + nameLength + extraLength
        guard payloadOffset >= 0, payloadOffset + Int(entry.compressedSize) <= data.count else {
            throw RsvpConversionError.unsupportedEpub
        }
        let compressed = data.subdata(in: payloadOffset..<(payloadOffset + Int(entry.compressedSize)))

        switch entry.method {
        case 0:
            return compressed
        case 8:
            return inflate(compressed, expectedSize: Int(entry.uncompressedSize))
        default:
            throw RsvpConversionError.unsupportedEpub
        }
    }

    private static func readEntries(data: Data) throws -> [String: ZipEntry] {
        guard let eocd = findEndOfCentralDirectory(data: data) else {
            throw RsvpConversionError.unsupportedEpub
        }

        let entryCount = Int(data.uint16(at: eocd + 10))
        let centralOffset = Int(data.uint32(at: eocd + 16))
        var offset = centralOffset
        var entries: [String: ZipEntry] = [:]

        for _ in 0..<entryCount {
            guard offset + 46 <= data.count, data.uint32(at: offset) == 0x02014b50 else {
                throw RsvpConversionError.unsupportedEpub
            }

            let method = data.uint16(at: offset + 10)
            let compressedSize = data.uint32(at: offset + 20)
            let uncompressedSize = data.uint32(at: offset + 24)
            let nameLength = Int(data.uint16(at: offset + 28))
            let extraLength = Int(data.uint16(at: offset + 30))
            let commentLength = Int(data.uint16(at: offset + 32))
            let localHeaderOffset = data.uint32(at: offset + 42)
            let nameStart = offset + 46
            let nameEnd = nameStart + nameLength
            guard nameEnd <= data.count else {
                throw RsvpConversionError.unsupportedEpub
            }

            if let name = String(data: data.subdata(in: nameStart..<nameEnd), encoding: .utf8) {
                let normalized = EpubConverter.normalizeZipPath(name).lowercased()
                entries[normalized] = ZipEntry(
                    method: method,
                    compressedSize: compressedSize,
                    uncompressedSize: uncompressedSize,
                    localHeaderOffset: localHeaderOffset
                )
            }

            offset = nameEnd + extraLength + commentLength
        }

        return entries
    }

    private static func findEndOfCentralDirectory(data: Data) -> Int? {
        let minimum = 22
        guard data.count >= minimum else {
            return nil
        }
        let lowerBound = max(0, data.count - 65_557)
        var offset = data.count - minimum
        while offset >= lowerBound {
            if data.uint32(at: offset) == 0x06054b50 {
                return offset
            }
            offset -= 1
        }
        return nil
    }

    private func inflate(_ compressed: Data, expectedSize: Int) -> Data? {
        guard expectedSize > 0 else {
            return Data()
        }
        var output = Data(count: expectedSize)
        let decodedSize = output.withUnsafeMutableBytes { outputBuffer in
            compressed.withUnsafeBytes { inputBuffer in
                compression_decode_buffer(
                    outputBuffer.bindMemory(to: UInt8.self).baseAddress!,
                    expectedSize,
                    inputBuffer.bindMemory(to: UInt8.self).baseAddress!,
                    compressed.count,
                    nil,
                    COMPRESSION_ZLIB
                )
            }
        }
        guard decodedSize > 0 else {
            return nil
        }
        output.removeSubrange(decodedSize..<output.count)
        return output
    }
}

private struct ZipEntry {
    let method: UInt16
    let compressedSize: UInt32
    let uncompressedSize: UInt32
    let localHeaderOffset: UInt32
}

private extension Data {
    func uint16(at offset: Int) -> UInt16 {
        guard offset + 2 <= count else {
            return 0
        }
        return withUnsafeBytes { rawBuffer in
            let bytes = rawBuffer.bindMemory(to: UInt8.self)
            let b0 = UInt16(bytes[offset])
            let b1 = UInt16(bytes[offset + 1]) << 8
            return b0 | b1
        }
    }

    func uint32(at offset: Int) -> UInt32 {
        guard offset + 4 <= count else {
            return 0
        }
        return withUnsafeBytes { rawBuffer in
            let bytes = rawBuffer.bindMemory(to: UInt8.self)
            let b0 = UInt32(bytes[offset])
            let b1 = UInt32(bytes[offset + 1]) << 8
            let b2 = UInt32(bytes[offset + 2]) << 16
            let b3 = UInt32(bytes[offset + 3]) << 24
            return b0 | b1 | b2 | b3
        }
    }
}
