import SwiftUI
import shared

struct SettingsPage: View {
    @ObservedObject var viewModel: SettingsViewModel
    @ObservedObject var connection: NanoConnectionManager = .shared

    var body: some View {
        List {
            connectionSection
            if viewModel.deviceSettings == nil {
                settingsSummarySection
            } else {
                wordPacingSettingsSection
                displaySettingsSection
                typographySettingsSection
                wifiSettingsSection

                saveStatusSection
            }
        }
        .listStyle(.insetGrouped)
        .refreshable {
            if connection.isConnected {
                viewModel.refreshSettings()
                viewModel.refreshWifiSettings()
            }
        }
    }

    private var connectionSection: some View {
        Section {
            TextField("Reader address", text: $connection.address)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .keyboardType(.URL)

            Button {
                connection.connectDefault()
            } label: {
                Label("Reset to 192.168.4.1", systemImage: "wifi")
            }
            .buttonStyle(.bordered)
            .tint(.secondary)
            .disabled(connection.isBusy)
        } header: {
            Text("Reader Connection")
        } footer: {
            Text("Connect to the Nano and set the fallback API address.")
        }
    }

    private var settingsSummarySection: some View {
        Section {
            Text(connection.isConnected ? "Load reader settings to edit them here." : "Connect to the Nano to edit reader settings.")
                .foregroundStyle(.secondary)

            Button {
                viewModel.refreshSettings()
            } label: {
                Label("Load Settings", systemImage: "slider.horizontal.3")
            }
            .buttonStyle(.borderedProminent)
            .disabled(!connection.isConnected || connection.isBusy)
        } header: {
            Text("Device Settings")
        }
    }

    private var wordPacingSettingsSection: some View {
        Section {
            if let settings = viewModel.deviceSettings {
                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Reading Mode")
                    Picker("Reading Mode", selection: readerModeBinding(for: settings)) {
                        Text("One word").tag("rsvp")
                        Text("Scroll").tag("scroll")
                    }
                    .pickerStyle(.segmented)
                }
                .disabled(connection.isBusy)

                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Pause Behavior")
                    Picker("Pause Behavior", selection: pauseModeBinding(for: settings)) {
                        Text("Sentence end").tag("sentence_end")
                        Text("Immediate").tag("instant")
                    }
                    .pickerStyle(.segmented)
                }
                .disabled(connection.isBusy)

                Toggle("Accurate time estimate", isOn: accurateTimeEstimateBinding(for: settings))
                    .disabled(connection.isBusy)

                sliderSetting("Base Speed", value: wpmBinding(for: settings), range: 10...1000, step: 1, label: "\(settings.reading.wpm) WPM")
                sliderSetting("Long Words", value: pacingLongBinding(for: settings), range: 0...600, step: 50, label: "\(settings.reading.pacing.longWordMs) ms")
                sliderSetting("Complexity", value: pacingComplexBinding(for: settings), range: 0...600, step: 50, label: "\(settings.reading.pacing.complexWordMs) ms")
                sliderSetting("Punctuation", value: pacingPunctuationBinding(for: settings), range: 0...600, step: 50, label: "\(settings.reading.pacing.punctuationMs) ms")

                Button {
                    viewModel.saveSettings(
                        settings
                            .withPacingLongWordMs(value: 0)
                            .withPacingComplexWordMs(value: 0)
                            .withPacingPunctuationMs(value: 0)
                    )
                } label: {
                    Label("Reset Pacing", systemImage: "arrow.counterclockwise")
                }
                .disabled(connection.isBusy)
            }
        } header: {
            Text("Word Pacing")
        } footer: {
            Text("Reading speed, pause timing, and RSVP behavior.")
        }
    }

    private var displaySettingsSection: some View {
        Section {
            if let settings = viewModel.deviceSettings {
                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Display Mode")
                    Picker("Display Mode", selection: appearanceModeBinding(for: settings)) {
                        Text("Light").tag("light")
                        Text("Dark").tag("dark")
                        Text("Night").tag("night")
                    }
                    .pickerStyle(.segmented)
                }
                .disabled(connection.isBusy)

                sliderSetting("Brightness", value: brightnessBinding(for: settings), range: 0...4, step: 1, label: "\(settings.display.brightnessIndex + 1) / 5")

                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Reader Hand")
                    Picker("Reader Hand", selection: handednessBinding(for: settings)) {
                        Text("Left").tag("left")
                        Text("Right").tag("right")
                    }
                    .pickerStyle(.segmented)
                }
                .disabled(connection.isBusy)

                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Footer Label")
                    Picker("Footer Label", selection: footerMetricBinding(for: settings)) {
                        Text("Percent").tag("percentage")
                        Text("Chapter time").tag("chapter_time")
                        Text("Book time").tag("book_time")
                    }
                }
                .disabled(connection.isBusy)

                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Battery Label")
                    Picker("Battery Label", selection: batteryLabelBinding(for: settings)) {
                        Text("Percent").tag("percent")
                        Text("Time left").tag("time_remaining")
                        Text("Voltage").tag("voltage")
                    }
                    .pickerStyle(.segmented)
                }
                .disabled(connection.isBusy)

                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Screensaver")
                    Picker("Screensaver", selection: screensaverBinding(for: settings)) {
                        Text("Life").tag(0)
                        Text("Maze").tag(2)
                        Text("Voronoi").tag(3)
                        Text("Screen off").tag(6)
                    }
                }
                .disabled(connection.isBusy)

                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Standby Timer")
                    Picker("Standby Timer", selection: standbyTimerBinding(for: settings)) {
                        Text("Never").tag(0)
                        Text("1 min").tag(1)
                        Text("5 min").tag(2)
                        Text("10 min").tag(3)
                        Text("30 min").tag(4)
                    }
                }
                .disabled(connection.isBusy)

                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Language")
                    Picker("Language", selection: languageBinding(for: settings)) {
                        Text("English").tag(0)
                        Text("Espanol").tag(1)
                        Text("Francais").tag(2)
                        Text("Deutsch").tag(3)
                        Text("Romana").tag(4)
                        Text("Polski").tag(5)
                    }
                }
                .disabled(connection.isBusy)

                Toggle("Battery while reading", isOn: readingBatteryBinding(for: settings))
                    .disabled(connection.isBusy)

                Toggle("Chapter while reading", isOn: readingChapterBinding(for: settings))
                    .disabled(connection.isBusy)

                Toggle("Book percent while reading", isOn: readingProgressBinding(for: settings))
                    .disabled(connection.isBusy)
            }
        } header: {
            Text("Display")
        } footer: {
            Text("Screen mode, standby behavior, and reader status labels.")
        }
    }

    private var wifiSettingsSection: some View {
        Section {
            if let wifi = viewModel.wifiSettings {
                LabeledContent("Saved Network", value: wifi.configured ? wifi.ssid : "Not set")
                if wifi.passwordSet {
                    Text("A password is saved on the reader. The app cannot read it back.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }

            TextField("Network name", text: $viewModel.wifiSsidDraft)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()

            SecureField("Password", text: $viewModel.wifiPasswordDraft)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()

            HStack(spacing: 10) {
                Button {
                    viewModel.saveWifiSettings()
                } label: {
                    Label("Save Wi-Fi", systemImage: "wifi")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .tint(.accentColor)
                .disabled(connection.isBusy || viewModel.wifiSsidDraft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)

                Button(role: .destructive) {
                    viewModel.forgetWifiSettings()
                } label: {
                    Label("Forget", systemImage: "trash")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .tint(.red)
                .disabled(connection.isBusy || viewModel.wifiSettings?.configured != true)
            }

        } header: {
            Text("Wi-Fi")
        } footer: {
            Text("Saved on the Nano for RSS and OTA updates.")
        }
    }

    private var typographySettingsSection: some View {
        Section {
            if let settings = viewModel.deviceSettings {
                Picker("Typeface", selection: typefaceBinding(for: settings)) {
                    Text("Standard").tag("standard")
                    Text("Atkinson").tag("atkinson")
                    Text("OpenDyslexic").tag("open_dyslexic")
                }
                .disabled(connection.isBusy)

                Toggle("Focus Highlight", isOn: focusHighlightBinding(for: settings))
                    .disabled(connection.isBusy)

                Toggle("Phantom Words", isOn: phantomWordsBinding(for: settings))
                    .disabled(connection.isBusy)

                sliderSetting("Font Size", value: fontSizeBinding(for: settings), range: 0...2, step: 1, label: "\(settings.display.fontSizeIndex + 1) / 3")
                sliderSetting("Tracking", value: trackingBinding(for: settings), range: -2...3, step: 1, label: "\(settings.typography.tracking)")
                sliderSetting("Anchor", value: anchorBinding(for: settings), range: 30...40, step: 1, label: "\(settings.typography.anchorPercent)%")
                sliderSetting("Guide Width", value: guideWidthBinding(for: settings), range: 12...30, step: 2, label: "\(settings.typography.guideWidth)")
                sliderSetting("Guide Gap", value: guideGapBinding(for: settings), range: 2...8, step: 1, label: "\(settings.typography.guideGap)")
            }
        } header: {
            Text("Typography")
        } footer: {
            Text("Typeface, focus markers, and word placement.")
        }
    }

    private var saveStatusSection: some View {
        Section {
            Label {
                Text("Changes are saved to the reader. Exit sync on the device to apply every setting on-screen.")
            } icon: {
                Image(systemName: "checkmark.circle")
            }
            .font(.caption)
            .foregroundStyle(.secondary)
        }
    }
}
