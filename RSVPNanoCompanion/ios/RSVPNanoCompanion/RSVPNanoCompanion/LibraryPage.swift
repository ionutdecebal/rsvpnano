import SwiftUI
import UIKit
import shared

struct LibraryPage: View {
    @ObservedObject var viewModel: LibraryViewModel
    @ObservedObject var connection: NanoConnectionManager = .shared
    var openWifiSettings: () -> Void

    var body: some View {
        if connection.isConnected {
            libraryList
        } else {
            ConnectionInstructionsView(
                viewModel: viewModel,
                connection: connection,
                openWifiSettings: openWifiSettings
            )
        }
    }

    private var libraryList: some View {
        List {
            if let info = connection.info {
                Section("Reader") {
                    LabeledContent("Name", value: info.name)
                    LabeledContent("Wi-Fi", value: info.networkSsid ?? "Connected")
                    LabeledContent("Library", value: viewModel.librarySummary)
                }
            }

            let bookItems = viewModel.books.filter { !$0.isArticle }
            let articleItems = viewModel.books.filter(\.isArticle)

            Section("Books") {
                if viewModel.books.isEmpty {
                    VStack(alignment: .leading, spacing: 6) {
                        Text("No books reported yet.")
                        Text("Upload a book here after the SD card has a /books folder. New books are saved in /books/books.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                } else if bookItems.isEmpty {
                    Text("No books yet.")
                        .foregroundStyle(.secondary)
                } else {
                    ForEach(bookItems) { book in
                        LibraryBookRow(book: book)
                    }
                    .onDelete { offsets in
                        viewModel.deleteBooks(offsets.map { bookItems[$0] })
                    }
                }
            }

            Section("Articles") {
                if articleItems.isEmpty {
                    VStack(alignment: .leading, spacing: 6) {
                        Text("No articles synced yet.")
                        Text("Shared articles and RSS downloads are saved in /books/articles.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                } else {
                    ForEach(articleItems) { book in
                        LibraryBookRow(book: book)
                    }
                    .onDelete { offsets in
                        viewModel.deleteBooks(offsets.map { articleItems[$0] })
                    }
                }
            }

            Section {
                Button {
                    viewModel.showingPicker = true
                } label: {
                    Label("Upload File", systemImage: "doc.badge.plus")
                }
                .disabled(!connection.isConnected || connection.isBusy)

                Button {
                    viewModel.showingTextImport = true
                } label: {
                    Label("New Text", systemImage: "text.badge.plus")
                }
                .disabled(!connection.isConnected || connection.isBusy)

                Button {
                    viewModel.refreshBooks()
                } label: {
                    Label("Refresh Library", systemImage: "arrow.clockwise")
                }
                .disabled(!connection.isConnected || connection.isBusy)
            }
        }
    }
}

private struct ConnectionInstructionsView: View {
    @ObservedObject var viewModel: LibraryViewModel
    @ObservedObject var connection: NanoConnectionManager
    var openWifiSettings: () -> Void

    var body: some View {
        List {
            Section("Connect to RSVP Nano") {
                VStack(alignment: .leading, spacing: 16) {
                    Label("Open Companion sync on the reader", systemImage: "1.circle")
                        .font(.headline)
                    Text("On RSVP Nano, open the main menu and choose Companion sync.")
                        .foregroundStyle(.secondary)

                    Label("Join the Nano Wi-Fi", systemImage: "2.circle")
                        .font(.headline)
                    Text("Join the network shown on the reader. It starts with RSVP-Nano.")
                        .foregroundStyle(.secondary)

                    Label("Return to this app", systemImage: "3.circle")
                        .font(.headline)
                    Text("The app checks http://192.168.4.1 automatically when it becomes active again.")
                        .foregroundStyle(.secondary)
                }
                .padding(.vertical, 8)

                Button {
                    openWifiSettings()
                } label: {
                    Label("Join Nano Wi-Fi", systemImage: "wifi")
                }

                Button {
                    connection.connectDefault()
                } label: {
                    Label("Check Now", systemImage: "network")
                }
                .disabled(connection.isBusy)
            }

            Section("Fallback") {
                VStack(alignment: .leading, spacing: 8) {
                    Text("If iOS does not show Wi-Fi settings from the app, open Settings manually, join the RSVP-Nano network shown on the reader, then return here.")
                        .foregroundStyle(.secondary)
                }

                Button {
                    openWifiSettings()
                } label: {
                    Label("Open Wi-Fi Settings", systemImage: "gear")
                }

                Button {
                    connection.showManualAddressEntry()
                } label: {
                    Label("Enter IP Address", systemImage: "number")
                }

                if connection.showAddressEntry {
                    Text("RSVP Nano was not found at http://192.168.4.1. If the reader shows a different address, enter it here.")
                        .font(.caption)
                        .foregroundStyle(.secondary)

                    TextField("Reader address or IP", text: $connection.address)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                        .keyboardType(.URL)

                    Button {
                        connection.connect()
                    } label: {
                        Label("Connect to This Address", systemImage: "network")
                    }
                    .disabled(connection.isBusy)
                }
            }

            Section {
                HStack {
                    if connection.isBusy {
                        ProgressView()
                    }
                    Text(connection.status)
                        .foregroundStyle(.secondary)
                }

                if connection.hasAttemptedConnection, let error = connection.lastConnectionError {
                    Text(error)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }
}

struct LibraryBookRow: View {
    let book: NanoBook

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(alignment: .firstTextBaseline, spacing: 10) {
                Text(book.displayTitle)
                    .frame(maxWidth: .infinity, alignment: .leading)
                if let progressPercent = book.progressPercent {
                    Text("\(max(0, min(100, progressPercent)))%")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.secondary)
                }
            }
            if let progressPercent = book.progressPercent {
                ProgressView(value: Double(max(0, min(100, progressPercent))), total: 100)
            }
            if !book.detailLabel.isEmpty {
                Text(book.detailLabel)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }
}
