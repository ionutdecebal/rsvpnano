import Foundation

enum NanoClientError: LocalizedError {
    case invalidBaseURL
    case invalidResponse
    case deviceRejected(String)

    var errorDescription: String? {
        switch self {
        case .invalidBaseURL:
            return "The device address is not valid."
        case .invalidResponse:
            return "The device returned an unexpected response."
        case .deviceRejected(let message):
            return message
        }
    }
}

struct NanoClient {
    var baseURLString: String

    private var baseURL: URL? {
        var value = baseURLString.trimmingCharacters(in: .whitespacesAndNewlines)
        if !value.lowercased().hasPrefix("http://") && !value.lowercased().hasPrefix("https://") {
            value = "http://" + value
        }
        return URL(string: value)
    }

    func fetchInfo() async throws -> NanoInfo {
        let url = try endpoint("/api/info")
        let (data, response) = try await URLSession.shared.data(from: url)
        try validate(response)
        return try JSONDecoder().decode(NanoInfo.self, from: data)
    }

    func fetchBooks() async throws -> [NanoBook] {
        let url = try endpoint("/api/books")
        let (data, response) = try await URLSession.shared.data(from: url)
        try validate(response)
        return try JSONDecoder().decode(NanoBooksResponse.self, from: data).books
    }

    func fetchSettings() async throws -> NanoSettings {
        let url = try endpoint("/api/settings")
        let (data, response) = try await URLSession.shared.data(from: url)
        try validate(response)
        return try JSONDecoder().decode(NanoSettings.self, from: data)
    }

    func updateSettings(_ settings: NanoSettings) async throws -> NanoSettings {
        var request = URLRequest(url: try endpoint("/api/settings"))
        request.httpMethod = "PATCH"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONEncoder().encode(settings)

        let (data, response) = try await URLSession.shared.data(for: request)
        guard let http = response as? HTTPURLResponse else {
            throw NanoClientError.invalidResponse
        }
        if !(200..<300).contains(http.statusCode) {
            if let decoded = try? JSONDecoder().decode(NanoUploadResponse.self, from: data),
               let error = decoded.error {
                throw NanoClientError.deviceRejected(error)
            }
            throw NanoClientError.invalidResponse
        }
        return try JSONDecoder().decode(NanoSettings.self, from: data)
    }

    func fetchRssFeeds() async throws -> NanoRssFeeds {
        let url = try endpoint("/api/rss-feeds")
        let (data, response) = try await URLSession.shared.data(from: url)
        try validate(response)
        return try JSONDecoder().decode(NanoRssFeeds.self, from: data)
    }

    func updateRssFeeds(_ feeds: [String]) async throws -> NanoRssFeeds {
        var request = URLRequest(url: try endpoint("/api/rss-feeds"))
        request.httpMethod = "PUT"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONEncoder().encode(NanoRssFeeds(ok: true, feeds: feeds))

        let (data, response) = try await URLSession.shared.data(for: request)
        guard let http = response as? HTTPURLResponse else {
            throw NanoClientError.invalidResponse
        }
        if !(200..<300).contains(http.statusCode) {
            if let decoded = try? JSONDecoder().decode(NanoUploadResponse.self, from: data),
               let error = decoded.error {
                throw NanoClientError.deviceRejected(error)
            }
            throw NanoClientError.invalidResponse
        }
        return try JSONDecoder().decode(NanoRssFeeds.self, from: data)
    }

    func uploadBook(data: Data, filename: String, category: String = "book") async throws -> NanoUploadResponse {
        let boundary = "RSVPNanoBoundary-\(UUID().uuidString)"
        var request = URLRequest(url: try endpoint("/api/books?name=\(queryEncoded(filename))&category=\(queryEncoded(category))"))
        request.httpMethod = "POST"
        request.setValue("multipart/form-data; boundary=\(boundary)", forHTTPHeaderField: "Content-Type")
        request.httpBody = multipartBody(data: data, filename: filename, boundary: boundary)

        let (responseData, response) = try await URLSession.shared.data(for: request)
        guard let http = response as? HTTPURLResponse else {
            throw NanoClientError.invalidResponse
        }

        let decoded = try JSONDecoder().decode(NanoUploadResponse.self, from: responseData)
        if !(200..<300).contains(http.statusCode) || !decoded.ok {
            throw NanoClientError.deviceRejected(decoded.error ?? "Upload failed.")
        }
        return decoded
    }

    func deleteBook(named filename: String) async throws -> NanoUploadResponse {
        var request = URLRequest(url: try endpoint("/api/books?name=\(queryEncoded(filename))"))
        request.httpMethod = "DELETE"

        let (responseData, response) = try await URLSession.shared.data(for: request)
        guard let http = response as? HTTPURLResponse else {
            throw NanoClientError.invalidResponse
        }

        let decoded = try JSONDecoder().decode(NanoUploadResponse.self, from: responseData)
        if !(200..<300).contains(http.statusCode) || !decoded.ok {
            throw NanoClientError.deviceRejected(decoded.error ?? "Delete failed.")
        }
        return decoded
    }

    private func endpoint(_ path: String) throws -> URL {
        guard let baseURL else {
            throw NanoClientError.invalidBaseURL
        }
        return URL(string: path, relativeTo: baseURL)!.absoluteURL
    }

    private func queryEncoded(_ value: String) -> String {
        var allowed = CharacterSet.urlQueryAllowed
        allowed.remove(charactersIn: "&+=?/")
        return value.addingPercentEncoding(withAllowedCharacters: allowed) ?? value
    }

    private func validate(_ response: URLResponse) throws {
        guard let http = response as? HTTPURLResponse, (200..<300).contains(http.statusCode) else {
            throw NanoClientError.invalidResponse
        }
    }

    private func multipartBody(data: Data, filename: String, boundary: String) -> Data {
        var body = Data()
        body.appendString("--\(boundary)\r\n")
        body.appendString("Content-Disposition: form-data; name=\"file\"; filename=\"\(filename)\"\r\n")
        body.appendString("Content-Type: application/octet-stream\r\n\r\n")
        body.append(data)
        body.appendString("\r\n--\(boundary)--\r\n")
        return body
    }
}

private extension Data {
    mutating func appendString(_ value: String) {
        append(Data(value.utf8))
    }
}
