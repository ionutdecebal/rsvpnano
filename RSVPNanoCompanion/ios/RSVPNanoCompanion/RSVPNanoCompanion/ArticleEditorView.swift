import SwiftUI

struct ArticleEditorView: View {
    let item: PendingUpload
    var onSave: (String, String) -> Void
    var onCancel: () -> Void

    @State private var title: String
    @State private var articleBody: String

    init(item: PendingUpload, onSave: @escaping (String, String) -> Void, onCancel: @escaping () -> Void) {
        self.item = item
        self.onSave = onSave
        self.onCancel = onCancel
        _title = State(initialValue: item.title)
        _articleBody = State(initialValue: item.needsArticleFetch ? "" : item.body)
    }

    private var wordCount: Int {
        articleBody.split { $0.isWhitespace }.count
    }

    private var canSave: Bool {
        !articleBody.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }

    var body: some View {
        NavigationStack {
            List {
                Section("Title") {
                    TextField("Article title", text: $title)
                }

                if let sourceUrl = item.sourceUrl, !sourceUrl.isEmpty {
                    Section("Source") {
                        Text(sourceUrl)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .textSelection(.enabled)
                    }
                }

                Section {
                    TextEditor(text: $articleBody)
                        .frame(minHeight: 320)
                        .font(.body)
                        .autocorrectionDisabled()
                } header: {
                    Text("Article")
                } footer: {
                    Text(item.needsArticleFetch ? "Fetch article text first, or paste text here manually." : "\(wordCount) words")
                }
            }
            .navigationTitle("Article")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel", action: onCancel)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        onSave(title, articleBody)
                    }
                    .disabled(!canSave)
                }
            }
        }
    }
}
