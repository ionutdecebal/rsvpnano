import SwiftUI

struct TextImportView: View {
    var onImport: (RsvpBookFile) -> Void
    var onCancel: () -> Void

    @State private var title = ""
    @State private var source = ""
    @State private var text = ""
    @State private var errorMessage: String?

    var body: some View {
        NavigationStack {
            Form {
                Section("Details") {
                    TextField("Title", text: $title)
                    TextField("Source or URL", text: $source)
                        .textInputAutocapitalization(.never)
                        .keyboardType(.URL)
                }

                Section("Text") {
                    TextEditor(text: $text)
                        .frame(minHeight: 220)
                }

                if let errorMessage {
                    Section {
                        Text(errorMessage)
                            .foregroundStyle(.red)
                    }
                }
            }
            .navigationTitle("New Text")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel", action: onCancel)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Upload") {
                        importText()
                    }
                    .disabled(text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                }
            }
        }
    }

    private func importText() {
        do {
            let fallbackTitle = title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
                ? RsvpConverter.titleFromText(text, fallback: "Untitled")
                : title
            let file = try RsvpConverter.rsvpFile(title: fallbackTitle, source: source, text: text)
            onImport(file)
        } catch {
            errorMessage = error.localizedDescription
        }
    }
}
