import SwiftUI
import shared

struct ContentView: View {
    @StateObject private var connection = NanoConnectionManager.shared
    @StateObject private var libraryViewModel = LibraryViewModel()
    @StateObject private var settingsViewModel = SettingsViewModel()
    @StateObject private var inboxViewModel = InboxViewModel()
    
    @State private var selectedPage: CompanionPage = .library
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                switch selectedPage {
                case .library:
                    LibraryPage(
                        viewModel: libraryViewModel,
                        openWifiSettings: openWifiSettings
                    )
                case .articles:
                    ArticlesPage(
                        viewModel: inboxViewModel,
                        settingsViewModel: settingsViewModel
                    )
                case .settings:
                    SettingsPage(
                        viewModel: settingsViewModel
                    )
                case .help:
                    HelpPage()
                }

                Divider()
                customTabBar
            }
            .navigationTitle(selectedPage.rawValue)
            .navigationBarTitleDisplayMode(.inline)
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
            .onChange(of: scenePhase) { _, phase in
                if phase == .active {
                    connection.recheckConnectionAfterForeground(showBusy: false)
                }
            }
            .onOpenURL { url in
                if url.scheme == "rsvpnano", url.host == "inbox" {
                    selectedPage = .articles
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
                    VStack(spacing: 4) {
                        Image(systemName: page.iconName)
                            .font(.system(size: 20))
                        Text(page.rawValue)
                            .font(.caption2)
                    }
                    .foregroundColor(selectedPage == page ? .accentColor : .secondary)
                    .frame(maxWidth: .infinity)
                }
                Spacer()
            }
        }
        .padding(.vertical, 5)
        .background(.bar)
    }

    private func openWifiSettings() {
        guard let url = URL(string: UIApplication.openSettingsURLString) else {
            return
        }
        UIApplication.shared.open(url)
    }
}

private enum CompanionPage: String, CaseIterable, Identifiable {
    case library = "Library"
    case articles = "Articles"
    case settings = "Settings"
    case help = "Help"

    var id: String { rawValue }

    var iconName: String {
        switch self {
        case .library: return "books.vertical"
        case .articles: return "doc.text"
        case .settings: return "slider.horizontal.3"
        case .help: return "questionmark.circle"
        }
    }
}
