import SwiftUI
import shared

struct ContentView: View {
    @StateObject private var connection = NanoConnectionManager.shared
    @StateObject private var libraryViewModel = LibraryViewModel()
    @StateObject private var settingsViewModel = SettingsViewModel()
    @StateObject private var inboxViewModel = InboxViewModel()
    
    @State private var selectedPage: CompanionPage = .library
    @State private var showingHelp = false
    @State private var showingUploadPicker = false
    @State private var showingRssFeeds = false
    @State private var toastMessage: String?
    @Namespace private var tabBarNamespace
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                ConnectionStatusBar(
                    connection: connection,
                    openWifiSettings: openWifiSettings,
                    showActionToast: showTransientToast
                )
                Divider()

                ZStack {
                    switch selectedPage {
                    case .library:
                        LibraryPage(
                            viewModel: libraryViewModel,
                            inboxViewModel: inboxViewModel,
                            onShowUpload: {
                                showingUploadPicker = true
                            }
                        )
                    case .settings:
                        SettingsPage(
                            viewModel: settingsViewModel
                        )
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)

                Divider()
                customTabBar
            }
            .overlay(alignment: .bottom) {
                if let toastMessage {
                    Text(toastMessage)
                        .font(.footnote.weight(.semibold))
                        .foregroundStyle(.white)
                        .padding(.horizontal, 14)
                        .padding(.vertical, 10)
                        .background(.black.opacity(0.82), in: Capsule())
                        .padding(.bottom, 70)
                        .transition(.move(edge: .bottom).combined(with: .opacity))
                }
            }
            .navigationTitle("RSVP Nano")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button {
                        showingHelp = true
                    } label: {
                        Image(systemName: "questionmark.circle")
                    }
                    .accessibilityLabel("Help")
                }
            }
            .sheet(isPresented: $showingHelp) {
                NavigationStack {
                    HelpPage()
                        .navigationTitle("Help")
                        .navigationBarTitleDisplayMode(.inline)
                        .toolbar {
                            ToolbarItem(placement: .topBarTrailing) {
                                Button("Done") {
                                    showingHelp = false
                                }
                            }
                        }
                }
            }
            .sheet(isPresented: $libraryViewModel.showingPicker) {
                BookDocumentPicker { file in
                    libraryViewModel.upload(file)
                }
            }
            .sheet(isPresented: $libraryViewModel.showingTextImport) {
                TextImportView { title, text in
                    libraryViewModel.upload(
                        shared.ImportPreparation.shared.rsvpFileForText(
                            title: title,
                            source: "Manual entry",
                            text: text,
                            fallbackTitle: "Imported Text"
                        )
                    )
                }
            }
            .sheet(item: $inboxViewModel.editingArticle) { item in
                ArticleEditorView(item: item) { title, body in
                    inboxViewModel.savePendingUpload(item, title: title, body: body)
                } onCancel: {
                    inboxViewModel.editingArticle = nil
                }
            }
            .onAppear {
                Task {
                    await connection.connectOnce(showBusy: false)
                }
            }
            .confirmationDialog("Upload", isPresented: $showingUploadPicker, titleVisibility: .visible) {
                Button("Upload Book") {
                    libraryViewModel.showingPicker = true
                }
                .disabled(!connection.isConnected || connection.isBusy)

                Button("Add Article") {
                    inboxViewModel.showingAddArticle = true
                }

                Button("Add RSS Feed") {
                    showingRssFeeds = true
                }
            }
            .sheet(isPresented: $inboxViewModel.showingAddArticle) {
                AddArticleSheet { title, url, body in
                    inboxViewModel.saveArticle(title: title, sourceUrl: url, body: body)
                }
            }
            .sheet(isPresented: $showingRssFeeds) {
                NavigationStack {
                    RssFeedsSheet(settingsViewModel: settingsViewModel)
                        .navigationTitle("RSS Feeds")
                        .navigationBarTitleDisplayMode(.inline)
                        .toolbar {
                            ToolbarItem(placement: .topBarTrailing) {
                                Button("Done") {
                                    showingRssFeeds = false
                                }
                            }
                        }
                }
            }
            .onChange(of: scenePhase) { _, phase in
                if phase == .active {
                    connection.recheckConnectionAfterForeground(showBusy: false)
                }
            }
            .onChange(of: connection.status) { _, status in
                showToastIfNeeded(status)
            }
            .onOpenURL { url in
                if url.scheme == "rsvpnano", url.host == "inbox" {
                    selectedPage = .library
                    inboxViewModel.handleSharedInboxOpen()
                }
            }
        }
    }

    private var customTabBar: some View {
        HStack {
            ForEach(CompanionPage.allCases) { page in
                Spacer()
                Button {
                    selectedPage = page
                } label: {
                    ZStack {
                        if selectedPage == page {
                            RoundedRectangle(cornerRadius: 18, style: .continuous)
                                .fill(Color.accentColor.opacity(0.16))
                                .matchedGeometryEffect(id: "tabIndicator", in: tabBarNamespace)
                        }
                        VStack(spacing: 4) {
                            Image(systemName: page.iconName)
                                .font(.system(size: 20))
                            Text(page.rawValue)
                                .font(.caption2)
                        }
                        .foregroundColor(selectedPage == page ? .accentColor : .secondary)
                    }
                    .frame(height: 48)
                    .frame(maxWidth: .infinity)
                }
                Spacer()
            }
        }
                .padding(.vertical, 5)
                .background(.bar)
                .animation(.spring(response: 0.32, dampingFraction: 0.82), value: selectedPage)
    }

    private func openWifiSettings() {
        guard let url = URL(string: UIApplication.openSettingsURLString) else {
            return
        }
        UIApplication.shared.open(url)
    }

    private func showToastIfNeeded(_ status: String) {
        let toastPrefixes = [
            "Uploaded",
            "Synced",
            "RSS",
            "Saved",
            "Wi-Fi",
            "Shared",
            "Reader disconnected"
        ]
        guard toastPrefixes.contains(where: { status.hasPrefix($0) }) else {
            return
        }
        withAnimation {
            toastMessage = status
        }
        Task {
            try? await Task.sleep(nanoseconds: 2_500_000_000)
            await MainActor.run {
                withAnimation {
                    if toastMessage == status {
                        toastMessage = nil
                    }
                }
            }
        }
    }

    private func showTransientToast(_ message: String) {
        withAnimation {
            toastMessage = message
        }
        Task {
            try? await Task.sleep(nanoseconds: 1_600_000_000)
            await MainActor.run {
                withAnimation {
                    if toastMessage == message {
                        toastMessage = nil
                    }
                }
            }
        }
    }
}

private enum CompanionPage: String, CaseIterable, Identifiable {
    case library = "Library"
    case settings = "Settings"

    var id: String { rawValue }

    var iconName: String {
        switch self {
        case .library: return "books.vertical"
        case .settings: return "slider.horizontal.3"
        }
    }
}

private struct ConnectionStatusBar: View {
    @ObservedObject var connection: NanoConnectionManager
    var openWifiSettings: () -> Void
    var showActionToast: (String) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            if connection.isConnected {
                HStack(spacing: 8) {
                    Image(systemName: "checkmark.circle.fill")
                        .font(.caption)
                        .foregroundStyle(.green)
                    Text("Connected")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.primary)
                    Spacer(minLength: 0)
                }
                .padding(.horizontal, 16)
                .padding(.vertical, 4)
            } else {
                disconnectedContent
                    .padding(.horizontal, 16)
                    .padding(.vertical, 10)
            }
        }
        .background(connectionSurface)
    }

    private var disconnectedContent: some View {
        VStack(alignment: .leading, spacing: 8) {
            let statusLabel = connectionStatusLabel
            HStack(alignment: .center, spacing: 10) {
                Image(systemName: "exclamationmark.triangle.fill")
                    .foregroundStyle(.orange)
                    .frame(width: 20, alignment: .center)

                Text(statusLabel)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(.primary)
                    .lineLimit(1)
                    .frame(maxWidth: .infinity, alignment: .leading)

                if connection.isBusy {
                    ProgressView()
                }

                Button {
                    openWifiSettings()
                } label: {
                    Label("Wi-Fi", systemImage: "wifi")
                }
                .buttonStyle(.borderedProminent)
                .tint(.accentColor)

                Button {
                    showActionToast("Checking for reader")
                    connection.connectDefault()
                } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .buttonStyle(.bordered)
                .tint(.secondary)
                .accessibilityLabel("Check connection")
                .disabled(connection.isBusy)
            }

            HStack(spacing: 12) {
                Button {
                    connection.showManualAddressEntry()
                } label: {
                    Text(connection.showAddressEntry ? "Hide manual connection" : "Connect manually")
                }
                .buttonStyle(.plain)
                .foregroundStyle(.tint)
            }

            if connection.showAddressEntry {
                HStack(spacing: 8) {
                    TextField("Reader address", text: $connection.address)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                        .keyboardType(.URL)
                        .textFieldStyle(.roundedBorder)

                    Button("Check") {
                        connection.connect()
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(connection.isBusy)
                }
            }
        }
    }

    private var connectionStatusLabel: String {
        if connection.isConnected {
            return "Connected"
        }
        if connection.status.hasPrefix("Looking for") {
            return "Connecting"
        }
        if connection.status.hasPrefix("Could not find") {
            return "Not found"
        }
        if connection.status.hasPrefix("Reader disconnected") {
            return "Disconnected"
        }
        if connection.status.hasPrefix("Still waiting") || connection.status.hasPrefix("Waiting") {
            return "Waiting"
        }
        return "Disconnected"
    }

    private var connectionSurface: Color {
        if connection.isConnected {
            return Color.green.opacity(0.12)
        }
        if connection.hasAttemptedConnection {
            return Color.orange.opacity(0.12)
        }
        return Color(.secondarySystemGroupedBackground)
    }
}

private struct AddArticleSheet: View {
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

private struct RssFeedsSheet: View {
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
