import SwiftUI

struct ConnectionStatusBar: View {
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
