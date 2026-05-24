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
