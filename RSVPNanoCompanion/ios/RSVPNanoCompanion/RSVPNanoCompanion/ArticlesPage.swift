import SwiftUI
import shared

struct ArticlesPage: View {
    @ObservedObject var viewModel: NanoViewModel

    var body: some View {
        List {
            savedArticlesSection
            syncedArticlesSection
            rssFeedsSection

            Section("Article Workflow") {
                VStack(alignment: .leading, spacing: 6) {
                    Label("Share from the browser", systemImage: "square.and.arrow.up")
                    Text("Use Share -> RSVP Nano from Safari, Chrome, or another app. URL-only articles can be fetched and renamed in the app before syncing.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Label("Edit before sync", systemImage: "pencil")
                    Text("Saved article drafts keep their title, source URL, and body text locally until you sync or delete them.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
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

                            if item.needsArticleFetch {
                                Button {
                                    viewModel.fetchArticleText(for: item)
                                } label: {
                                    Label("Fetch", systemImage: "doc.text.magnifyingglass")
                                        .frame(maxWidth: .infinity)
                                }
                                .buttonStyle(.borderedProminent)
                                .disabled(viewModel.isBusy)
                            } else {
                                Button {
                                    viewModel.syncPendingUpload(item)
                                } label: {
                                    Label("Sync", systemImage: "arrow.up.doc")
                                        .frame(maxWidth: .infinity)
                                }
                                .buttonStyle(.borderedProminent)
                                .disabled(!viewModel.canUpload)
                            }
                        }
                    }
                }
                .onDelete(perform: viewModel.deletePendingUploads)

                Button {
                    viewModel.syncPendingUploads()
                } label: {
                    Label("Sync Saved Articles", systemImage: "arrow.up.doc")
                }
                .disabled(!viewModel.canSyncPending)
            }
        }
    }

    private var syncedArticlesSection: some View {
        let articleItems = viewModel.books.filter(\.isArticle)

        return Section {
            if !viewModel.isConnected {
                Text("Connect to Companion sync to see articles already on the SD card.")
                    .foregroundStyle(.secondary)
            } else if articleItems.isEmpty {
                Text("No synced articles on the SD card yet.")
                    .foregroundStyle(.secondary)
            } else {
                ForEach(articleItems) { article in
                    VStack(alignment: .leading, spacing: 8) {
                        HStack(alignment: .firstTextBaseline, spacing: 10) {
                            LibraryBookRow(book: article)
                            Text("Synced")
                                .font(.caption.weight(.semibold))
                                .foregroundStyle(.green)
                        }
                    }
                }
                .onDelete { offsets in
                    viewModel.deleteBooks(offsets.map { articleItems[$0] })
                }
            }

            if viewModel.isConnected {
                Button {
                    viewModel.refreshBooks()
                } label: {
                    Label("Refresh Synced Articles", systemImage: "arrow.clockwise")
                }
                .disabled(viewModel.isBusy)
            }
        } header: {
            Text("Synced Articles")
        } footer: {
            Text("These are articles already saved on the reader in /books/articles.")
        }
    }

    private var rssFeedsSection: some View {
        Section {
            if viewModel.rssFeeds.isEmpty {
                VStack(alignment: .leading, spacing: 6) {
                    Text("No RSS feeds saved.")
                    Text("Add feed URLs now, then sync them to the reader when Companion sync is connected.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            } else {
                ForEach(viewModel.rssFeeds, id: \.self) { feed in
                    HStack(alignment: .firstTextBaseline, spacing: 10) {
                        Text(feed)
                            .font(.caption)
                            .textSelection(.enabled)
                            .frame(maxWidth: .infinity, alignment: .leading)
                        Text(viewModel.syncedRssFeeds.contains(feed) ? "Synced" : "Pending")
                            .font(.caption.weight(.semibold))
                            .foregroundStyle(viewModel.syncedRssFeeds.contains(feed) ? .green : .orange)
                    }
                }
                .onDelete(perform: viewModel.deleteRssFeeds)
            }

            TextField("https://example.com/feed.xml", text: $viewModel.rssFeedDraft)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .keyboardType(.URL)

            HStack(spacing: 10) {
                Button {
                    viewModel.addRssFeed()
                } label: {
                    Label(viewModel.isConnected ? "Add & Sync" : "Add Feed", systemImage: "plus")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .disabled(viewModel.isBusy || viewModel.rssFeedDraft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)

                Button {
                    viewModel.isConnected ? viewModel.syncRssFeeds() : viewModel.connect()
                } label: {
                    Label(viewModel.isConnected ? "Sync Feeds" : "Connect", systemImage: "arrow.triangle.2.circlepath")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .disabled(viewModel.isBusy || (viewModel.isConnected && viewModel.rssFeeds.isEmpty))
            }

            if viewModel.isConnected {
                Button {
                    viewModel.refreshRssFeeds()
                } label: {
                    Label("Reload From Reader", systemImage: "arrow.down.circle")
                }
                .disabled(viewModel.isBusy)
            }
        } header: {
            Text("RSS Feeds")
        } footer: {
            Text("Feeds marked Pending are saved on this iPhone. Sync writes the full list to /config/rss.conf on the reader.")
        }
    }

    private func pendingDetailLabel(for item: PendingUpload) -> String {
        let size = ByteCountFormatter.string(fromByteCount: Int64(item.bytes), countStyle: .file)
        let words = item.body.split { $0.isWhitespace }.count
        let detail = item.needsArticleFetch ? "link saved" : (words == 1 ? "\(words) word" : "\(words) words")
        guard let sourceUrl = item.sourceUrl, !sourceUrl.isEmpty else {
            return "\(detail) · \(size)"
        }
        return "\(detail) · \(size) · \(sourceUrl)"
    }
}
