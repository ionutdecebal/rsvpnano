import SwiftUI

struct AddArticleSheet: View {
    var onSave: (String, String, String) -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var url = ""
    @State private var title = ""
    @State private var body = ""
    @State private var showingBody = false

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("https://example.com/article", text: $url)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                        .keyboardType(.URL)

                    TextField("Title", text: $title)
                }

                Section {
                    Button {
                        showingBody.toggle()
                    } label: {
                        Label(showingBody ? "Hide Body" : "Edit Body", systemImage: "pencil.and.list.clipboard")
                    }

                    if showingBody {
                        TextEditor(text: $body)
                            .frame(minHeight: 180)
                    }
                }
            }
            .navigationTitle("Add Article")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button("Cancel") {
                        dismiss()
                    }
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Save") {
                        onSave(title, url, body)
                        dismiss()
                    }
                    .disabled(!canSave)
                }
            }
        }
    }

    private var canSave: Bool {
        if showingBody && !body.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            return !title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
        }
        guard let scheme = URL(string: url.trimmingCharacters(in: .whitespacesAndNewlines))?.scheme?.lowercased() else {
            return false
        }
        return scheme == "http" || scheme == "https"
    }
}

struct RssFeedsSheet: View {
    @ObservedObject var settingsViewModel: SettingsViewModel
    @ObservedObject var connection: NanoConnectionManager = .shared

    var body: some View {
        List {
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
                            Label(feed, systemImage: "dot.radiowaves.left.and.right")
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

            } header: {
                Text("RSS Feeds")
            } footer: {
                Text("Feeds marked Pending are saved on this iPhone. Sync writes the full list to /config/rss.conf on the reader.")
            }
        }
        .listStyle(.insetGrouped)
        .refreshable {
            if connection.isConnected {
                settingsViewModel.refreshRssFeeds()
            }
        }
    }
}
