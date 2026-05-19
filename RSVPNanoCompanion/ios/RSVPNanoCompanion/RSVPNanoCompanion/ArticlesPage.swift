import SwiftUI
import shared

struct ArticlesPage: View {
    @ObservedObject var viewModel: InboxViewModel
    @ObservedObject var settingsViewModel: SettingsViewModel
    @ObservedObject var connection: NanoConnectionManager = .shared

    var body: some View {
        List {
            savedArticlesSection
            syncedArticlesSection
            rssFeedsSection

            Section("Article Workflow") {
                VStack(alignment: .leading, spacing: 6) {
                    Label("Share from the browser", systemImage: "square.and.arrow.up")
                    Text("Use Share -> RSVP Nano while your phone still has internet. The app saves fetched article text locally; then connect to the Nano Wi-Fi when you are ready to sync.")
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
                            Text("This is only a saved link. Share it again while online, or paste article text in Preview/Edit before syncing.")
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
                .disabled(!connection.isConnected || connection.isBusy || !viewModel.pendingUploads.contains { !$0.needsArticleFetch })
            }
        }
    }

    private var syncedArticlesSection: some View {
        // We need library books here. 
        // For now, we can pass them in or article synced is a separate concern.
        // Actually, let's keep it simple: LibraryViewModel owns the synced list.
        // But for parity with previous UI, we can inject the LibraryViewModel or just the list.
        
        Section {
            Text("Go to Library to see articles already on the SD card.")
                .foregroundStyle(.secondary)
        } header: {
            Text("Synced Articles")
        }
    }

    private var rssFeedsSection: some View {
        Section {
            if settingsViewModel.rssFeeds.isEmpty {
                VStack(alignment: .leading, spacing: 6) {
                    Text("No RSS feeds saved.")
                    Text("Add feed URLs now, then sync them to the reader when Companion sync is connected.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            } else {
                ForEach(settingsViewModel.rssFeeds, id: \.self) { feed in
                    HStack(alignment: .firstTextBaseline, spacing: 10) {
                        Text(feed)
                            .font(.caption)
                            .textSelection(.enabled)
                            .frame(maxWidth: .infinity, alignment: .leading)
                        Text(settingsViewModel.syncedRssFeeds.contains(feed) ? "Synced" : "Pending")
                            .font(.caption.weight(.semibold))
                            .foregroundStyle(settingsViewModel.syncedRssFeeds.contains(feed) ? .green : .orange)
                    }
                }
                .onDelete(perform: settingsViewModel.deleteRssFeeds)
            }

            TextField("https://example.com/feed.xml", text: $settingsViewModel.rssFeedDraft)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .keyboardType(.URL)

            HStack(spacing: 10) {
                Button {
                    settingsViewModel.addRssFeed()
                } label: {
                    Label(connection.isConnected ? "Add & Sync" : "Add Feed", systemImage: "plus")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .disabled(connection.isBusy || settingsViewModel.rssFeedDraft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)

                Button {
                    connection.isConnected ? settingsViewModel.syncRssFeeds() : connection.connect()
                } label: {
                    Label(connection.isConnected ? "Sync Feeds" : "Connect", systemImage: "arrow.triangle.2.circlepath")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .disabled(connection.isBusy || (connection.isConnected && settingsViewModel.rssFeeds.isEmpty))
            }

            if connection.isConnected {
                Button {
                    settingsViewModel.refreshRssFeeds()
                } label: {
                    Label("Reload From Reader", systemImage: "arrow.down.circle")
                }
                .disabled(connection.isBusy)
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
