import UIKit
import UniformTypeIdentifiers

final class ShareViewController: UIViewController {
    private static let maxSharedTextCharacters = 300_000

    private let titleField = UITextField()
    private let sourceLabel = UILabel()
    private let textView = UITextView()
    private let statusLabel = UILabel()
    private let saveButton = UIButton(type: .system)
    private let cancelButton = UIButton(type: .system)

    private var sharedSource = ""
    private var shouldOpenAppAfterSave = false
    private var hasSaved = false
    private var isFinished = false
    private var sourceIsURL = false

    override func viewDidLoad() {
        super.viewDidLoad()
        configureView()
        Task {
            await loadSharedContent()
        }
    }

    private func configureView() {
        view.backgroundColor = .systemBackground

        let headingLabel = UILabel()
        headingLabel.text = "RSVP Nano"
        headingLabel.font = .preferredFont(forTextStyle: .title2)
        headingLabel.adjustsFontForContentSizeCategory = true

        titleField.placeholder = "Article title"
        titleField.borderStyle = .roundedRect
        titleField.clearButtonMode = .whileEditing
        titleField.font = .preferredFont(forTextStyle: .body)
        titleField.adjustsFontForContentSizeCategory = true

        sourceLabel.font = .preferredFont(forTextStyle: .caption1)
        sourceLabel.textColor = .secondaryLabel
        sourceLabel.numberOfLines = 2
        sourceLabel.adjustsFontForContentSizeCategory = true

        textView.font = .preferredFont(forTextStyle: .body)
        textView.adjustsFontForContentSizeCategory = true
        textView.layer.borderColor = UIColor.separator.cgColor
        textView.layer.borderWidth = 1
        textView.layer.cornerRadius = 8
        textView.textContainerInset = UIEdgeInsets(top: 10, left: 8, bottom: 10, right: 8)
        textView.isEditable = true

        statusLabel.text = "Loading shared content..."
        statusLabel.font = .preferredFont(forTextStyle: .footnote)
        statusLabel.textColor = .secondaryLabel
        statusLabel.numberOfLines = 0
        statusLabel.adjustsFontForContentSizeCategory = true

        cancelButton.setTitle("Cancel", for: .normal)
        cancelButton.addTarget(self, action: #selector(cancel), for: .touchUpInside)

        saveButton.setTitle("Save", for: .normal)
        saveButton.titleLabel?.font = .preferredFont(forTextStyle: .headline)
        saveButton.addTarget(self, action: #selector(save), for: .touchUpInside)
        saveButton.isEnabled = false

        let buttonStack = UIStackView(arrangedSubviews: [cancelButton, saveButton])
        buttonStack.axis = .horizontal
        buttonStack.distribution = .equalSpacing

        let stack = UIStackView(arrangedSubviews: [
            headingLabel,
            titleField,
            sourceLabel,
            textView,
            statusLabel,
            buttonStack,
        ])
        stack.axis = .vertical
        stack.spacing = 12
        stack.translatesAutoresizingMaskIntoConstraints = false

        view.addSubview(stack)

        NSLayoutConstraint.activate([
            stack.topAnchor.constraint(equalTo: view.layoutMarginsGuide.topAnchor, constant: 12),
            stack.leadingAnchor.constraint(equalTo: view.layoutMarginsGuide.leadingAnchor),
            stack.trailingAnchor.constraint(equalTo: view.layoutMarginsGuide.trailingAnchor),
            stack.bottomAnchor.constraint(equalTo: view.layoutMarginsGuide.bottomAnchor, constant: -12),
            textView.heightAnchor.constraint(greaterThanOrEqualToConstant: 220),
        ])
    }

    private func loadSharedContent() async {
        do {
            let shared = try await sharedInput()
            await MainActor.run {
                sharedSource = shared.source
                sourceIsURL = shared.isURL
                shouldOpenAppAfterSave = shared.shouldOpenAppAfterSave
                titleField.text = shared.title
                sourceLabel.text = shared.source
                textView.text = shared.text
                statusLabel.text = "\(shared.diagnostic) · \(wordCount(in: shared.text)) words. Edit before saving."
                saveButton.isEnabled = true
            }
        } catch {
            await MainActor.run {
                statusLabel.text = error.localizedDescription
                saveButton.isEnabled = false
            }
        }
    }

    private func sharedInput() async throws -> SharedInput {
        let items = extensionContext?.inputItems.compactMap { $0 as? NSExtensionItem } ?? []
        let providers = items.flatMap { $0.attachments ?? [] }
        let itemTitle = items.compactMap { $0.attributedTitle?.string }.firstNonEmpty ?? ""
        let itemText = items.compactMap { $0.attributedContentText?.string }.firstNonEmpty ?? ""

        if let provider = providers.first(where: { urlTypeIdentifier(from: $0) != nil }),
           let typeIdentifier = urlTypeIdentifier(from: provider),
           let url = try await loadURL(from: provider, typeIdentifier: typeIdentifier) {
            return SharedInput(
                title: urlDraftTitle(itemTitle: itemTitle, url: url),
                source: url.absoluteString,
                text: "",
                isURL: true,
                diagnostic: "URL only. Save to fetch in the app.",
                shouldOpenAppAfterSave: true
            )
        }

        if !itemText.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            let title = [itemTitle, RsvpConverter.titleFromText(itemText, fallback: "Shared Text")].firstNonEmpty ?? "Shared Text"
            return SharedInput(title: title, source: "Shared text", text: Self.clipped(itemText), isURL: false, diagnostic: "Host text")
        }

        if let provider = providers.first(where: { $0.hasItemConformingToTypeIdentifier(UTType.plainText.identifier) }),
           let text = try await loadText(from: provider),
           !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            let title = [itemTitle, RsvpConverter.titleFromText(text, fallback: "Shared Text")].firstNonEmpty ?? "Shared Text"
            return SharedInput(title: title, source: "Shared text", text: text, isURL: false, diagnostic: "Plain text")
        }

        throw ShareError.noContent(providerTypes: providerTypeSummary(providers))
    }

    private func urlTypeIdentifier(from provider: NSItemProvider) -> String? {
        for identifier in [UTType.url.identifier, "public.url", "public.file-url"] {
            if provider.hasItemConformingToTypeIdentifier(identifier) {
                return identifier
            }
        }
        return nil
    }

    private func loadURL(from provider: NSItemProvider, typeIdentifier: String) async throws -> URL? {
        try await withCheckedThrowingContinuation { continuation in
            provider.loadItem(forTypeIdentifier: typeIdentifier, options: nil) { item, error in
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
                    continuation.resume(returning: Self.clipped(text))
                } else if let data = item as? Data {
                    continuation.resume(returning: String(data: data, encoding: .utf8).map(Self.clipped))
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

    private func urlDraftTitle(itemTitle: String, url: URL) -> String {
        let cleanedTitle = itemTitle.trimmingCharacters(in: .whitespacesAndNewlines)
        let source = url.absoluteString
        let host = url.host ?? ""
        if !cleanedTitle.isEmpty,
           cleanedTitle != host,
           cleanedTitle != "www.\(host)",
           !source.contains(cleanedTitle) {
            return cleanedTitle
        }
        return source
    }

    private static func clipped(_ value: String) -> String {
        if value.count <= maxSharedTextCharacters {
            return value
        }
        return String(value.prefix(maxSharedTextCharacters))
    }

    private func wordCount(in value: String) -> Int {
        value.split { $0.isWhitespace }.count
    }

    private func providerTypeSummary(_ providers: [NSItemProvider]) -> String {
        let identifiers = providers.flatMap(\.registeredTypeIdentifiers)
        guard !identifiers.isEmpty else {
            return "none"
        }
        return identifiers.prefix(6).joined(separator: ", ")
    }

    @objc private func save() {
        if hasSaved {
            finish()
            return
        }

        let title = titleField.text?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        var text = textView.text.trimmingCharacters(in: .whitespacesAndNewlines)
        if text.isEmpty, sourceIsURL {
            text = sharedSource
        }
        guard !text.isEmpty else {
            statusLabel.text = "Add some text before saving."
            return
        }

        saveButton.isEnabled = false
        statusLabel.text = "Saving..."

        Task {
            do {
                let store = PendingUploadStore()
                let savedItem = try store.saveDraft(title: title, source: sharedSource, body: text)
                let savedCount = try store.all().count
                let inboxPath = try store.rootURL.lastPathComponent
                await MainActor.run {
                    hasSaved = true
                    titleField.isEnabled = false
                    textView.isEditable = false
                    saveButton.isEnabled = true
                    saveButton.setTitle("Done", for: .normal)
                    cancelButton.isHidden = true
                    statusLabel.text = "Saved \(savedItem.title) to \(inboxPath): \(ByteCountFormatter.string(fromByteCount: Int64(savedItem.bytes), countStyle: .file)). Inbox now has \(savedCount)."
                    if shouldOpenAppAfterSave {
                        openContainingApp()
                    }
                }
            } catch {
                await MainActor.run {
                    statusLabel.text = error.localizedDescription
                    saveButton.isEnabled = true
                }
            }
        }
    }

    @objc private func cancel() {
        finish()
    }

    private func finish() {
        guard !isFinished else {
            return
        }
        isFinished = true
        extensionContext?.completeRequest(returningItems: nil)
    }

    private func openContainingApp() {
        guard let url = URL(string: "rsvpnano://inbox") else {
            return
        }
        extensionContext?.open(url) { [weak self] _ in
            self?.finish()
        }
    }
}

private struct SharedInput {
    let title: String
    let source: String
    let text: String
    let isURL: Bool
    let diagnostic: String
    var shouldOpenAppAfterSave = false
}

private enum ShareError: LocalizedError {
    case noContent(providerTypes: String)

    var errorDescription: String? {
        switch self {
        case .noContent(let providerTypes):
            return "No text or URL was shared. Types: \(providerTypes)"
        }
    }
}

private extension Array where Element == String {
    var firstNonEmpty: String? {
        first { !$0.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }
    }
}
