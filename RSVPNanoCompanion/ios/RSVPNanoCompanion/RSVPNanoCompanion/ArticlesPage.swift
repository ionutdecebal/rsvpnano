import SwiftUI
import shared

struct ArticlesPage: View {
    @ObservedObject var viewModel: InboxViewModel
    @ObservedObject var connection: NanoConnectionManager = .shared

    var body: some View {
        List {
            workflowSection
            savedArticlesSection
        }
        .listStyle(.insetGrouped)
    }

    private var workflowSection: some View {
        Section {
            VStack(alignment: .leading, spacing: 8) {
                Label("Save while online", systemImage: "square.and.arrow.up")
                    .font(.headline)
                Text("Share an article to RSVP Nano before joining the reader Wi-Fi. Sync it later from this screen.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            .padding(.vertical, 4)
        }
    }

    @ViewBuilder
    private var savedArticlesSection: some View {
        Section("Saved Articles") {
            if viewModel.pendingUploads.isEmpty {
                VStack(alignment: .leading, spacing: 6) {
                    Text("No saved articles yet.")
                    Text("Use Share -> RSVP Nano from Safari, Chrome, or another app, then tap Save in the share sheet.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            } else {
                ForEach(viewModel.pendingUploads) { item in
                    VStack(alignment: .leading, spacing: 10) {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(item.title)
                                .foregroundStyle(.primary)
                            Text("\(item.displayDate) · \(pendingDetailLabel(for: item))")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }

                        HStack(spacing: 10) {
                            Button {
                                viewModel.editingArticle = item
                            } label: {
                                Label("Preview/Edit", systemImage: "pencil.and.list.clipboard")
                                    .frame(maxWidth: .infinity)
                            }
                            .buttonStyle(.bordered)

                            Button {
                                viewModel.syncPendingUpload(item)
                            } label: {
                                Label("Sync", systemImage: "arrow.up.doc")
                                    .frame(maxWidth: .infinity)
                            }
                            .buttonStyle(.borderedProminent)
                            .disabled(item.needsArticleFetch || !connection.isConnected || connection.isBusy)
                        }

                        if item.needsArticleFetch {
                            Text("Needs article text before sync.")
                                .font(.caption)
                                .foregroundStyle(.orange)
                        }
                    }
                }
                .onDelete(perform: viewModel.deletePendingUploads)

                Button {
                    viewModel.syncPendingUploads()
                } label: {
                    Label("Sync Saved Articles", systemImage: "arrow.up.doc")
                }
                .buttonStyle(.borderedProminent)
                .disabled(!connection.isConnected || connection.isBusy || !viewModel.pendingUploads.contains { !$0.needsArticleFetch })
            }
        }
    }

    private func pendingDetailLabel(for item: PendingUpload) -> String {
        let size = ByteCountFormatter.string(fromByteCount: Int64(item.bytes), countStyle: .file)
        let words = item.body.split { $0.isWhitespace }.count
        let detail = item.needsArticleFetch ? "link saved" : (words == 1 ? "\(words) word" : "\(words) words")
        guard let sourceUrl = item.sourceUrl, !sourceUrl.isEmpty else {
            return "\(detail) · \(size)"
        }
        return "\(detail) · \(size) · \(sourceHost(for: sourceUrl))"
    }

    private func sourceHost(for sourceUrl: String) -> String {
        guard let host = URL(string: sourceUrl)?.host, !host.isEmpty else {
            return "source saved"
        }
        return host
    }
}
