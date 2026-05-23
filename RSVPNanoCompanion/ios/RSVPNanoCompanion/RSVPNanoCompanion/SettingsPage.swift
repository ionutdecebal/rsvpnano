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

                Section {
                    Text("Changes are saved to the reader. Exit sync on the device to apply every setting on-screen.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
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
        Section("Reader Connection") {
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
        }
    }

    private var settingsSummarySection: some View {
        Section("Device Settings") {
            Text(connection.isConnected ? "Load reader settings to edit them here." : "Connect to the reader to edit settings.")
                .foregroundStyle(.secondary)

            Button {
                viewModel.refreshSettings()
            } label: {
                Label("Load Settings", systemImage: "slider.horizontal.3")
            }
            .buttonStyle(.borderedProminent)
            .disabled(!connection.isConnected || connection.isBusy)
        }
    }

    private var wordPacingSettingsSection: some View {
        Section("Word Pacing") {
            if let settings = viewModel.deviceSettings {
                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Reading Mode")
                    Picker("Reading Mode", selection: readerModeBinding(for: settings)) {
                        Text("One Word").tag("rsvp")
                        Text("Scroll Text").tag("scroll")
                    }
                    .pickerStyle(.segmented)
                }
                .disabled(connection.isBusy)

                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Pause Behaviour")
                    Picker("Pause Behaviour", selection: pauseModeBinding(for: settings)) {
                        Text("At Sentence End").tag("sentence_end")
                        Text("Immediately").tag("instant")
                    }
                    .pickerStyle(.segmented)
                }
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
        }
    }

    private var displaySettingsSection: some View {
        Section("Display") {
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
                        Text("Percent Read").tag("percentage")
                        Text("Chapter Time").tag("chapter_time")
                        Text("Book Time").tag("book_time")
                    }
                }
                .disabled(connection.isBusy)

                VStack(alignment: .leading, spacing: 8) {
                    settingsControlLabel("Battery Label")
                    Picker("Battery Label", selection: batteryLabelBinding(for: settings)) {
                        Text("Percentage").tag("percent")
                        Text("Time Remaining").tag("time_remaining")
                        Text("Voltage").tag("voltage")
                    }
                    .pickerStyle(.segmented)
                }
                .disabled(connection.isBusy)

                Toggle("Show Battery While Reading", isOn: readingBatteryBinding(for: settings))
                    .disabled(connection.isBusy)

                Toggle("Show Chapter While Reading", isOn: readingChapterBinding(for: settings))
                    .disabled(connection.isBusy)

                Toggle("Show Book Percent While Reading", isOn: readingProgressBinding(for: settings))
                    .disabled(connection.isBusy)
            }
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
            Text("Used by the reader for RSS and OTA updates.")
        }
    }

    private var typographySettingsSection: some View {
        Section("Typography") {
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
        }
    }

    private func settingsControlLabel(_ text: String) -> some View {
        Text(text)
            .font(.caption.weight(.semibold))
            .foregroundStyle(.secondary)
    }

    private func sliderSetting(_ title: String, value: Binding<Int>, range: ClosedRange<Int>, step: Int, label: String) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            LabeledContent(title, value: label)
            Slider(
                value: Binding(
                    get: { Double(value.wrappedValue) },
                    set: { next in
                        let snapped = Int((next / Double(step)).rounded()) * step
                        value.wrappedValue = min(max(snapped, range.lowerBound), range.upperBound)
                    }
                ),
                in: Double(range.lowerBound)...Double(range.upperBound),
                step: Double(step)
            )
        }
        .disabled(connection.isBusy)
    }

    private func wpmBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.reading.wpm ?? settings.reading.wpm },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withWpm(value: Int32(snappedWpm(value))))
            }
        )
    }

    private func snappedWpm(_ value: Int) -> Int {
        let clamped = min(max(value, 10), 1000)
        if clamped <= 100 {
            return min(max(((clamped + 5) / 10) * 10, 10), 100)
        }
        return min(max(100 + (((clamped - 100 + 12) / 25) * 25), 100), 1000)
    }

    private func pauseModeBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.reading.pauseMode ?? settings.reading.pauseMode },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withPauseMode(value: value))
            }
        )
    }

    private func readerModeBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.reading.readerMode ?? settings.reading.readerMode },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withReaderMode(value: value))
            }
        )
    }

    private func pacingLongBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.reading.pacing.longWordMs ?? settings.reading.pacing.longWordMs },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withPacingLongWordMs(value: Int32(value)))
            }
        )
    }

    private func pacingComplexBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.reading.pacing.complexWordMs ?? settings.reading.pacing.complexWordMs },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withPacingComplexWordMs(value: Int32(value)))
            }
        )
    }

    private func pacingPunctuationBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.reading.pacing.punctuationMs ?? settings.reading.pacing.punctuationMs },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withPacingPunctuationMs(value: Int32(value)))
            }
        )
    }

    private func brightnessBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.display.brightnessIndex ?? settings.display.brightnessIndex },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withBrightnessIndex(value: Int32(value)))
            }
        )
    }

    private func handednessBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.display.handedness ?? settings.display.handedness },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withHandedness(value: value))
            }
        )
    }

    private func footerMetricBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.display.footerMetric ?? settings.display.footerMetric },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withFooterMetric(value: value))
            }
        )
    }

    private func batteryLabelBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.display.batteryLabel ?? settings.display.batteryLabel },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withBatteryLabel(value: value))
            }
        )
    }

    private func readingBatteryBinding(for settings: NanoSettings) -> Binding<Bool> {
        Binding(
            get: { viewModel.deviceSettings?.display.readingBattery ?? settings.display.readingBattery },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withReadingBattery(value: value))
            }
        )
    }

    private func readingChapterBinding(for settings: NanoSettings) -> Binding<Bool> {
        Binding(
            get: { viewModel.deviceSettings?.display.readingChapter ?? settings.display.readingChapter },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withReadingChapter(value: value))
            }
        )
    }

    private func readingProgressBinding(for settings: NanoSettings) -> Binding<Bool> {
        Binding(
            get: { viewModel.deviceSettings?.display.readingProgress ?? settings.display.readingProgress },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withReadingProgress(value: value))
            }
        )
    }

    private func appearanceModeBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: {
                let display = viewModel.deviceSettings?.display ?? settings.display
                if display.nightMode {
                    return "night"
                }
                return display.darkMode ? "dark" : "light"
            },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(
                    current.withAppearance(
                        darkMode: value == "dark" || value == "night",
                        nightMode: value == "night"
                    )
                )
            }
        )
    }

    private func phantomWordsBinding(for settings: NanoSettings) -> Binding<Bool> {
        Binding(
            get: { viewModel.deviceSettings?.display.phantomWords ?? settings.display.phantomWords },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withPhantomWords(value: value))
            }
        )
    }

    private func fontSizeBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.display.fontSizeIndex ?? settings.display.fontSizeIndex },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withFontSizeIndex(value: Int32(value)))
            }
        )
    }

    private func typefaceBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.typography.typeface ?? settings.typography.typeface },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withTypeface(value: value))
            }
        )
    }

    private func focusHighlightBinding(for settings: NanoSettings) -> Binding<Bool> {
        Binding(
            get: { viewModel.deviceSettings?.typography.focusHighlight ?? settings.typography.focusHighlight },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withFocusHighlight(value: value))
            }
        )
    }

    private func trackingBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.typography.tracking ?? settings.typography.tracking },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withTracking(value: Int32(value)))
            }
        )
    }

    private func anchorBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.typography.anchorPercent ?? settings.typography.anchorPercent },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withAnchorPercent(value: Int32(value)))
            }
        )
    }

    private func guideWidthBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.typography.guideWidth ?? settings.typography.guideWidth },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withGuideWidth(value: Int32(value)))
            }
        )
    }

    private func guideGapBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.typography.guideGap ?? settings.typography.guideGap },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withGuideGap(value: Int32(value)))
            }
        )
    }
}
