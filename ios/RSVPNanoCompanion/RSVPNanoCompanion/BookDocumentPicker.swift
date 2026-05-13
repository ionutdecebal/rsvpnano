import SwiftUI
import UniformTypeIdentifiers

struct PickedBookFile {
    let filename: String
    let data: Data
}

struct BookDocumentPicker: UIViewControllerRepresentable {
    var onPick: (PickedBookFile) -> Void
    var onCancel: () -> Void

    func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
        let rsvpType = UTType(filenameExtension: "rsvp") ?? .data
        let epubType = UTType(filenameExtension: "epub") ?? .data
        let markdownType = UTType(filenameExtension: "md") ?? .plainText
        let markdownLongType = UTType(filenameExtension: "markdown") ?? .plainText
        let xhtmlType = UTType(filenameExtension: "xhtml") ?? .html
        let picker = UIDocumentPickerViewController(
            forOpeningContentTypes: [rsvpType, epubType, markdownType, markdownLongType, .plainText, .html, xhtmlType, .data]
        )
        picker.allowsMultipleSelection = false
        picker.delegate = context.coordinator
        return picker
    }

    func updateUIViewController(_ uiViewController: UIDocumentPickerViewController, context: Context) {}

    func makeCoordinator() -> Coordinator {
        Coordinator(onPick: onPick, onCancel: onCancel)
    }

    final class Coordinator: NSObject, UIDocumentPickerDelegate {
        private let onPick: (PickedBookFile) -> Void
        private let onCancel: () -> Void

        init(onPick: @escaping (PickedBookFile) -> Void, onCancel: @escaping () -> Void) {
            self.onPick = onPick
            self.onCancel = onCancel
        }

        func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
            guard let url = urls.first else {
                onCancel()
                return
            }

            let didAccess = url.startAccessingSecurityScopedResource()
            defer {
                if didAccess {
                    url.stopAccessingSecurityScopedResource()
                }
            }

            do {
                let data = try Data(contentsOf: url)
                onPick(PickedBookFile(filename: url.lastPathComponent, data: data))
            } catch {
                onCancel()
            }
        }

        func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
            onCancel()
        }
    }
}
