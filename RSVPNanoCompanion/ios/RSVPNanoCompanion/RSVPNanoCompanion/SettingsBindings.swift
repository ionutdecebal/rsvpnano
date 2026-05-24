import SwiftUI
import shared

extension SettingsPage {
    func settingsControlLabel(_ text: String) -> some View {
        Text(text)
            .font(.caption.weight(.semibold))
            .foregroundStyle(.secondary)
    }

    func sliderSetting(_ title: String, value: Binding<Int>, range: ClosedRange<Int>, step: Int, label: String) -> some View {
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

    func wpmBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.reading.wpm ?? settings.reading.wpm },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withWpm(value: Int32(value)))
            }
        )
    }

    func pauseModeBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.reading.pauseMode ?? settings.reading.pauseMode },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withPauseMode(value: value))
            }
        )
    }

    func readerModeBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.reading.readerMode ?? settings.reading.readerMode },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withReaderMode(value: value))
            }
        )
    }

    func pacingLongBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.reading.pacing.longWordMs ?? settings.reading.pacing.longWordMs },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withPacingLongWordMs(value: Int32(value)))
            }
        )
    }

    func pacingComplexBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.reading.pacing.complexWordMs ?? settings.reading.pacing.complexWordMs },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withPacingComplexWordMs(value: Int32(value)))
            }
        )
    }

    func pacingPunctuationBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.reading.pacing.punctuationMs ?? settings.reading.pacing.punctuationMs },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withPacingPunctuationMs(value: Int32(value)))
            }
        )
    }

    func brightnessBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.display.brightnessIndex ?? settings.display.brightnessIndex },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withBrightnessIndex(value: Int32(value)))
            }
        )
    }

    func handednessBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.display.handedness ?? settings.display.handedness },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withHandedness(value: value))
            }
        )
    }

    func footerMetricBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.display.footerMetric ?? settings.display.footerMetric },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withFooterMetric(value: value))
            }
        )
    }

    func batteryLabelBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.display.batteryLabel ?? settings.display.batteryLabel },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withBatteryLabel(value: value))
            }
        )
    }

    func readingBatteryBinding(for settings: NanoSettings) -> Binding<Bool> {
        Binding(
            get: { viewModel.deviceSettings?.display.readingBattery ?? settings.display.readingBattery },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withReadingBattery(value: value))
            }
        )
    }

    func readingChapterBinding(for settings: NanoSettings) -> Binding<Bool> {
        Binding(
            get: { viewModel.deviceSettings?.display.readingChapter ?? settings.display.readingChapter },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withReadingChapter(value: value))
            }
        )
    }

    func readingProgressBinding(for settings: NanoSettings) -> Binding<Bool> {
        Binding(
            get: { viewModel.deviceSettings?.display.readingProgress ?? settings.display.readingProgress },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withReadingProgress(value: value))
            }
        )
    }

    func appearanceModeBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: {
                (viewModel.deviceSettings ?? settings).appearanceMode
            },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withAppearanceMode(value: value))
            }
        )
    }

    func phantomWordsBinding(for settings: NanoSettings) -> Binding<Bool> {
        Binding(
            get: { viewModel.deviceSettings?.display.phantomWords ?? settings.display.phantomWords },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withPhantomWords(value: value))
            }
        )
    }

    func fontSizeBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.display.fontSizeIndex ?? settings.display.fontSizeIndex },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withFontSizeIndex(value: Int32(value)))
            }
        )
    }

    func typefaceBinding(for settings: NanoSettings) -> Binding<String> {
        Binding(
            get: { viewModel.deviceSettings?.typography.typeface ?? settings.typography.typeface },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withTypeface(value: value))
            }
        )
    }

    func focusHighlightBinding(for settings: NanoSettings) -> Binding<Bool> {
        Binding(
            get: { viewModel.deviceSettings?.typography.focusHighlight ?? settings.typography.focusHighlight },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withFocusHighlight(value: value))
            }
        )
    }

    func trackingBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.typography.tracking ?? settings.typography.tracking },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withTracking(value: Int32(value)))
            }
        )
    }

    func anchorBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.typography.anchorPercent ?? settings.typography.anchorPercent },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withAnchorPercent(value: Int32(value)))
            }
        )
    }

    func guideWidthBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.typography.guideWidth ?? settings.typography.guideWidth },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withGuideWidth(value: Int32(value)))
            }
        )
    }

    func guideGapBinding(for settings: NanoSettings) -> Binding<Int> {
        Binding(
            get: { viewModel.deviceSettings?.typography.guideGap ?? settings.typography.guideGap },
            set: { value in
                guard let current = viewModel.deviceSettings else { return }
                viewModel.saveSettings(current.withGuideGap(value: Int32(value)))
            }
        )
    }
}
