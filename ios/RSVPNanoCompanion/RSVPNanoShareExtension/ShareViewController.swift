import UIKit
import UniformTypeIdentifiers

final class ShareViewController: UIViewController {
    private let titleLabel = UILabel()
    private let detailLabel = UILabel()
    private let doneButton = UIButton(type: .system)
    private var isFinished = false

    override func viewDidLoad() {
        super.viewDidLoad()
        configureView()
        Task {
            await sendSharedContent()
        }
    }

    private func configureView() {
        view.backgroundColor = .systemBackground

        titleLabel.text = "RSVP Nano"
        titleLabel.font = .preferredFont(forTextStyle: .title2)
        titleLabel.adjustsFontForContentSizeCategory = true

        detailLabel.text = "Preparing shared content..."
        detailLabel.font = .preferredFont(forTextStyle: .body)
        detailLabel.textColor = .secondaryLabel
        detailLabel.numberOfLines = 0
        detailLabel.adjustsFontForContentSizeCategory = true

        doneButton.setTitle("Done", for: .normal)
        doneButton.addTarget(self, action: #selector(done), for: .touchUpInside)

        let stack = UIStackView(arrangedSubviews: [titleLabel, detailLabel, doneButton])
        stack.axis = .vertical
        stack.alignment = .fill
        stack.spacing = 18
        stack.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stack)

        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: view.layoutMarginsGuide.leadingAnchor),
            stack.trailingAnchor.constraint(equalTo: view.layoutMarginsGuide.trailingAnchor),
            stack.centerYAnchor.constraint(equalTo: view.centerYAnchor),
        ])
    }

    private func sendSharedContent() async {
        do {
            let shared = try await sharedInput()
            updateDetail("Formatting \(shared.title)...")
            let article = ArticleFormatter.article(title: shared.title, source: shared.source, htmlOrText: shared.text)
            let file = try RsvpConverter.rsvpFile(
                title: article.title,
                author: "",
                source: article.source,
                events: ArticleFormatter.events(from: article)
            )
            _ = try PendingUploadStore().save(file, source: article.source)
            updateDetail("Saved \(file.title). Connect to RSVP Nano to sync it.")
        } catch {
            updateDetail(error.localizedDescription)
        }
    }

    private func sharedInput() async throws -> SharedInput {
        let providers = extensionContext?.inputItems
            .compactMap { $0 as? NSExtensionItem }
            .flatMap { $0.attachments ?? [] } ?? []

        if let provider = providers.first(where: { $0.hasItemConformingToTypeIdentifier(UTType.propertyList.identifier) }),
           let page = try await loadSafariPage(from: provider) {
            let selected = page.selection.trimmingCharacters(in: .whitespacesAndNewlines)
            return SharedInput(
                title: page.title.isEmpty ? fallbackTitle(for: page.url) : page.title,
                source: page.url.absoluteString,
                text: selected.isEmpty ? page.html : selected
            )
        }

        if let provider = providers.first(where: { $0.hasItemConformingToTypeIdentifier(UTType.url.identifier) }),
           let url = try await loadURL(from: provider) {
            return try await inputFromURL(url)
        }

        if let provider = providers.first(where: { $0.hasItemConformingToTypeIdentifier(UTType.plainText.identifier) }),
           let text = try await loadText(from: provider) {
            let title = RsvpConverter.titleFromText(text, fallback: "Shared Text")
            return SharedInput(title: title, source: "Shared text", text: text)
        }

        throw ShareError.noContent
    }

    private func inputFromURL(_ url: URL) async throws -> SharedInput {
        do {
            let (data, _) = try await URLSession.shared.data(from: url)
            let text = String(data: data, encoding: .utf8) ?? String(data: data, encoding: .isoLatin1) ?? ""
            let title = RsvpConverter.titleFromText(text, fallback: fallbackTitle(for: url))
            return SharedInput(title: title, source: url.absoluteString, text: text)
        } catch {
            return SharedInput(title: fallbackTitle(for: url), source: url.absoluteString, text: url.absoluteString)
        }
    }

    private func loadSafariPage(from provider: NSItemProvider) async throws -> SafariPage? {
        try await withCheckedThrowingContinuation { continuation in
            provider.loadItem(forTypeIdentifier: UTType.propertyList.identifier, options: nil) { item, error in
                if let error {
                    continuation.resume(throwing: error)
                    return
                }

                let dictionary: [String: Any]?
                if let item = item as? [String: Any] {
                    dictionary = item
                } else if let item = item as? NSDictionary {
                    dictionary = item as? [String: Any]
                } else {
                    dictionary = nil
                }

                let results = dictionary?[NSExtensionJavaScriptPreprocessingResultsKey] as? [String: Any]
                let urlString = results?["URL"] as? String ?? ""
                let url = URL(string: urlString) ?? URL(string: "https://example.com")!
                continuation.resume(returning: SafariPage(
                    url: url,
                    title: results?["title"] as? String ?? "",
                    selection: results?["selection"] as? String ?? "",
                    html: results?["HTML"] as? String ?? ""
                ))
            }
        }
    }

    private func loadURL(from provider: NSItemProvider) async throws -> URL? {
        try await withCheckedThrowingContinuation { continuation in
            provider.loadItem(forTypeIdentifier: UTType.url.identifier, options: nil) { item, error in
                if let error {
                    continuation.resume(throwing: error)
                    return
                }
                if let url = item as? URL {
                    continuation.resume(returning: url)
                } else if let url = item as? NSURL {
                    continuation.resume(returning: url as URL)
                } else if let text = item as? String {
                    continuation.resume(returning: URL(string: text))
                } else {
                    continuation.resume(returning: nil)
                }
            }
        }
    }

    private func loadText(from provider: NSItemProvider) async throws -> String? {
        try await withCheckedThrowingContinuation { continuation in
            provider.loadItem(forTypeIdentifier: UTType.plainText.identifier, options: nil) { item, error in
                if let error {
                    continuation.resume(throwing: error)
                    return
                }
                if let text = item as? String {
                    continuation.resume(returning: text)
                } else if let data = item as? Data {
                    continuation.resume(returning: String(data: data, encoding: .utf8))
                } else {
                    continuation.resume(returning: nil)
                }
            }
        }
    }

    private func fallbackTitle(for url: URL) -> String {
        if let host = url.host, !host.isEmpty {
            return host
        }
        return url.deletingPathExtension().lastPathComponent.isEmpty ? "Shared Article" : url.deletingPathExtension().lastPathComponent
    }

    private func updateDetail(_ value: String) {
        Task { @MainActor in
            detailLabel.text = value
        }
    }

    @objc private func done() {
        guard !isFinished else {
            return
        }
        isFinished = true
        extensionContext?.completeRequest(returningItems: nil)
    }
}

private struct SharedInput {
    let title: String
    let source: String
    let text: String
}

private struct SafariPage {
    let url: URL
    let title: String
    let selection: String
    let html: String
}

private enum ShareError: LocalizedError {
    case noContent

    var errorDescription: String? {
        "No text or URL was shared."
    }
}
