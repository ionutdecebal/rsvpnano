#include <esp_log.h>

#include <Arduino.h>
#include <algorithm>
#include <esp_heap_caps.h>
#include <memory>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "board/BoardStorage.h"

#include "board/Board.h"
#include "board/BoardConfig.h"
#include "board/BoardInput.h"
#include "converter/EpubConverter.h"
#include "fonts/FontCatalog.h"
#include "hash/Fnv1a.h"
#include "input/Input.h"
#include "logging/Logger.h"
#include "reader/ReadingLoop.h"
#include "settings/NvsSecurity.h"
#include "storage/StorageManager.h"
#include "storage/fs/SdCard.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"
#include "storage/index/IndexedBook.h"
#include "storage/library/BookLibrary.h"
#include "text/BidiText.h"
#include "text/UnicodeText.h"
#include "ui/Theme.h"
#include "ui/Ui.h"
#include "ui/screens/ReaderScreen.h"
#include "ui/screens/Screens.h"
#include "usb/UsbMassStorageManager.h"

namespace {

    constexpr const char* kBenchmarkDir = "/benchmark";
    constexpr const char* kStorageMarkerPath = "/benchmark/.rsvpnano-benchmark";
    constexpr const char* kRunReadyPath = "/benchmark/.run-ready";
    constexpr const char* kSdWritePath = "/benchmark/sd-write.bin";
    constexpr const char* kDraculaEpubPath = "/benchmark/Dracula-epub.epub";
    constexpr const char* kDraculaRsvpPath = "/benchmark/Dracula-epub.rsvp";
    constexpr const char* kMultilingualRsvpPath = "/benchmark/multilingual.rsvp";
    constexpr const char* kVerticalEpubPath = "/benchmark/vertical-cjk.epub";
    constexpr const char* kVerticalRsvpPath = "/benchmark/vertical-cjk.rsvp";
    constexpr size_t kSdProbeBytes = 256UL * 1024UL;
    constexpr size_t kSdChunkBytes = 4096;
    constexpr size_t kSdRandomIterations = 64;
    constexpr size_t kCpuIterations = 64;
    constexpr size_t kRenderIterations = 8;
    constexpr size_t kReadingSessionWords = 320;
    constexpr size_t kReadingScrubJumps = 24;

    ui::Context gDisplay(Board::Display::gfx());
    ui::themes::Theme gTheme = ui::themes::defaultTheme();
    ui::fonts::AlphaTextRenderer<640> gText(Board::Display::gfx());
    FontCatalog gFonts;
#if RSVP_USB_MSC_ENABLED
    UsbMassStorageManager gBenchmarkStorage;
#endif
    bool gDisplayReady = false;

    struct TextSample {
        std::string_view id;
        std::string_view locale;
        std::string_view paragraph;
        std::string_view word;
        std::string_view nextWord;
        bool rightToLeft = false;
    };

    constexpr TextSample kLatinSample{"latin",
                                      "en",
                                      "Comfortable reading should remain quick on every page.",
                                      "Comfortable",
                                      "reading",
                                      false};
    constexpr TextSample kHebrewSample{"hebrew", "he", "קריאה עברית נוחה וברורה עם נִקּוּד מלא.", "נִקּוּד", "עברית", true};
    constexpr TextSample kArabicSample{"arabic",  "ar",      "القراءة العربية واضحة ومريحة مع التَّشْكِيلِ.",
                                       "التَّشْكِيلِ", "العربية", true};
    constexpr TextSample kCjkSample{"cjk", "ja", "日本語と中文の文章を快適に読みます。", "日本語と中文", "文章", false};
    constexpr TextSample kChineseSample{"han",      "zh-Hans",  "中文文章应当快速清晰地显示。",
                                        "中文阅读", "文章显示", false};
    constexpr TextSample kMathSample{"math", "en", "∀x∈ℝ, x²≥0 and ∫₀¹x²dx=⅓.", "∀x∈ℝ", "x²≥0", false};
    constexpr std::string_view kMixedParagraph =
        "English 123 — עברית עם נִקּוּד — العربية مع التَّشْكِيلِ — 日本語と中文 — ∀x∈ℝ.";

    void showStatus(const char* title, const char* line1 = "", const char* line2 = "") {
        ESP_LOGI("bench", "screen title=%s line1=%s line2=%s", title, line1, line2);
        if (gDisplayReady) {
            screens::status(gDisplay, title, line1, line2);
        }
    }

    bool prepareBenchmarkStorage() {
#if RSVP_USB_MSC_ENABLED
        bool mounted = false;
        if (!SdCard::mount(mounted)) {
            ESP_LOGE("bench", "storage_prepare_mount_failed");
            return true;
        }

        if (auto created = StorageFiles::ensureDirectory(kBenchmarkDir); !created) {
            ESP_LOGE("bench", "storage_prepare_directory_failed error=%s", created.error().message().c_str());
            Board::Storage::end();
            return true;
        }

        auto& filesystem = Board::Storage::filesystem();
        if (StorageFiles::fileExists(kRunReadyPath)) {
            filesystem.remove(kRunReadyPath);
            Board::Storage::end();
            ESP_LOGI("bench", "storage_prepared");
            return true;
        }

        File marker = filesystem.open(kStorageMarkerPath, FILE_WRITE);
        if (!marker || marker.isDirectory()) {
            if (marker)
                marker.close();
            ESP_LOGE("bench", "storage_marker_write_failed");
            Board::Storage::end();
            return true;
        }
        marker.printf("RSVP Nano benchmark\nboard=%s\nready=benchmark/.run-ready\n", Board::Config::BOARD_ID);
        marker.flush();
        marker.close();
        Board::Storage::end();

        showStatus("Benchmark storage", "Copying fixtures", "Safely eject when ready");
        if (!gBenchmarkStorage.begin(true)) {
            ESP_LOGE("bench", "storage_export_failed status=%s", gBenchmarkStorage.statusMessage());
            return true;
        }

        ESP_LOGI("bench", "storage_ready marker=benchmark/.rsvpnano-benchmark");
        constexpr std::string_view kRunCommand = "run\n";
        size_t matchedCommandBytes = 0;
        while (!gBenchmarkStorage.ejected() && matchedCommandBytes < kRunCommand.size()) {
            while (Serial.available() > 0 && matchedCommandBytes < kRunCommand.size()) {
                const char received = static_cast<char>(Serial.read());
                matchedCommandBytes = received == kRunCommand[matchedCommandBytes] ? matchedCommandBytes + 1
                                    : received == kRunCommand.front()              ? 1
                                                                                   : 0;
            }
            delay(20);
        }

        gBenchmarkStorage.end();
        delay(100);
        ESP.restart();
        return false;
#else
        return true;
#endif
    }

    ui::Rect renderArea() {
        const int16_t top = std::min<int16_t>(48, gDisplay.height() / 3);
        const int16_t bottom = std::min<int16_t>(32, gDisplay.height() / 4);
        return {8, top, static_cast<int16_t>(gDisplay.width() - 16),
                static_cast<int16_t>(gDisplay.height() - top - bottom)};
    }

    void showRenderScreen(std::string_view title, std::string_view detail) {
        const ui::Rect area = renderArea();
        gDisplay.invalidate();
        gDisplay.beginFrame(static_cast<uint8_t>(screens::Screen::Status));
        gDisplay.label({8, 4, static_cast<int16_t>(gDisplay.width() - 16), 20}, "Font benchmark", 2,
                       ui::themes::ColorRole::Accent, ui::TextAlign::Center);
        gDisplay.label({8, 26, static_cast<int16_t>(gDisplay.width() - 16), 18}, title, 2,
                       ui::themes::ColorRole::Foreground, ui::TextAlign::Center);
        const int16_t footerY = static_cast<int16_t>(area.y + area.h + 4);
        gDisplay.label({8, footerY, static_cast<int16_t>(gDisplay.width() - 16),
                        static_cast<int16_t>(gDisplay.height() - footerY)},
                       detail, 1, ui::themes::ColorRole::Muted, ui::TextAlign::Center);
        auto& gfx = Board::Display::gfx();
        gfx.drawFastHLine(area.x, area.y, area.w, gDisplay.color(ui::themes::ColorRole::Outline));
        gfx.drawFastHLine(area.x, static_cast<int16_t>(area.y + area.h - 1), area.w,
                          gDisplay.color(ui::themes::ColorRole::Outline));
        gDisplay.endFrame();
    }

    void clearRenderArea() {
        const ui::Rect area = renderArea();
        auto& gfx = Board::Display::gfx();
        gfx.fillRect(area.x, static_cast<int16_t>(area.y + 1), area.w, static_cast<int16_t>(area.h - 2),
                     gDisplay.color(ui::themes::ColorRole::Background));
        gfx.flush();
    }

    void logMetric(std::string_view name, bool ok, uint32_t elapsedUs, size_t iterations = 1, size_t bytes = 0,
                   uint32_t heapBefore = 0, uint32_t heapAfter = 0, uint32_t minimumHeap = 0,
                   size_t deadlineMisses = 0) {
        const uint32_t elapsedMs = (elapsedUs + 999U) / 1000U;
        const uint32_t averageUs = iterations > 0 ? elapsedUs / iterations : 0;
        const uint32_t rateKiBPerSecond = elapsedUs > 0 && bytes > 0
                                            ? static_cast<uint32_t>((static_cast<uint64_t>(bytes) * 1000000ULL)
                                                                    / (static_cast<uint64_t>(elapsedUs) * 1024ULL))
                                            : 0;
        ESP_LOGI("bench",
                 "metric=%.*s ok=%u ms=%lu us=%lu iterations=%lu avg_us=%lu bytes=%lu rate_kib_s=%lu "
                 "heap_before=%lu heap_after=%lu heap_min=%lu deadline_misses=%lu",
                 static_cast<int>(name.size()), name.data(), ok ? 1 : 0, static_cast<unsigned long>(elapsedMs),
                 static_cast<unsigned long>(elapsedUs), static_cast<unsigned long>(iterations),
                 static_cast<unsigned long>(averageUs), static_cast<unsigned long>(bytes),
                 static_cast<unsigned long>(rateKiBPerSecond), static_cast<unsigned long>(heapBefore),
                 static_cast<unsigned long>(heapAfter), static_cast<unsigned long>(minimumHeap),
                 static_cast<unsigned long>(deadlineMisses));
    }

    void fillBytes(uint8_t* buffer, size_t bytes, uint32_t offset) {
        for (size_t i = 0; i < bytes; ++i) {
            const uint32_t value = offset + static_cast<uint32_t>(i);
            buffer[i] = static_cast<uint8_t>((value * 33U) ^ (value >> 3) ^ 0xA5U);
        }
    }

    uint32_t checksumBytes(const uint8_t* buffer, size_t bytes) {
        return Fnv1a::hash(std::span{buffer, bytes});
    }

    bool benchmarkDisplayPush() {
        auto& gfx = Board::Display::gfx();
        gfx.fillScreen(gDisplay.color(ui::themes::ColorRole::Background));
        gfx.flush();
        return true;
    }

    bool benchmarkSdWriteRead() {
        if (auto created = StorageFiles::ensureDirectory(kBenchmarkDir); !created) {
            ESP_LOGE("bench", "sd_benchmark_directory_failed error=%s", created.error().message().c_str());
            return false;
        }

        uint8_t* buffer = static_cast<uint8_t*>(heap_caps_malloc(kSdChunkBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
        if (buffer == nullptr) {
            return false;
        }

        uint32_t expectedChecksum = Fnv1a::kOffsetBasis;
        File file = Board::Storage::filesystem().open(kSdWritePath, FILE_WRITE);
        if (!file) {
            heap_caps_free(buffer);
            return false;
        }

        for (size_t offset = 0; offset < kSdProbeBytes; offset += kSdChunkBytes) {
            const size_t chunk = min(kSdChunkBytes, kSdProbeBytes - offset);
            fillBytes(buffer, chunk, static_cast<uint32_t>(offset));
            expectedChecksum = checksumBytes(buffer, chunk) ^ (expectedChecksum * Fnv1a::kPrime);
            if (file.write(buffer, chunk) != chunk) {
                file.close();
                heap_caps_free(buffer);
                return false;
            }
        }
        file.flush();
        file.close();

        uint32_t actualChecksum = Fnv1a::kOffsetBasis;
        file = Board::Storage::filesystem().open(kSdWritePath, FILE_READ);
        if (!file) {
            heap_caps_free(buffer);
            return false;
        }

        for (size_t offset = 0; offset < kSdProbeBytes; offset += kSdChunkBytes) {
            const size_t chunk = min(kSdChunkBytes, kSdProbeBytes - offset);
            if (file.read(buffer, chunk) != static_cast<int>(chunk)) {
                file.close();
                heap_caps_free(buffer);
                return false;
            }
            actualChecksum = checksumBytes(buffer, chunk) ^ (actualChecksum * Fnv1a::kPrime);
        }
        file.close();
        heap_caps_free(buffer);
        return expectedChecksum == actualChecksum;
    }

    void reportEpubProgress(const EpubConverter::Options&, const char* line1, const char* line2, int progressPercent) {
        std::string percentLine = std::to_string(progressPercent) + "%";
        if (line2 != nullptr && line2[0] != '\0') {
            percentLine += " ";
            percentLine += line2;
        }
        showStatus("EPUB", line1 == nullptr ? "" : line1, percentLine.c_str());
    }

    bool benchmarkEpubConversion(const char* epubPath, const char* rsvpPath, const char* label) {
        if (!StorageFiles::fileExistsWithBytes(epubPath)) {
            ESP_LOGW("bench", "missing_epub path=%s", epubPath);
            showStatus("EPUB missing", label, "in /benchmark on SD");
            return false;
        }

        Board::Storage::filesystem().remove(rsvpPath);
        Board::Storage::filesystem()
            .remove(StoragePaths::siblingPathWithExtension(epubPath, StoragePaths::kTempExtension).c_str());
        Board::Storage::filesystem()
            .remove(StoragePaths::siblingPathWithExtension(epubPath, StoragePaths::kFailedExtension).c_str());

        EpubConverter::Options options;
        options.progressCallback = reportEpubProgress;
        options.progressTitle = "Benchmark";
        options.progressLabel = label;
        return EpubConverter::convertIfNeeded(epubPath, rsvpPath, options).has_value();
    }

    template<typename Operation>
    bool runTimed(std::string_view name, size_t iterations, Operation&& operation, size_t bytes = 0,
                  bool updateDisplay = true) {
        const std::string ownedName{name};
        if (updateDisplay)
            showStatus("Benchmark", ownedName.c_str(), "Running");
        const uint32_t heapBefore = ESP.getFreeHeap();
        const bool monitorHeap = heap_caps_monitor_local_minimum_free_size_start() == ESP_OK;
        const uint32_t startedUs = micros();
        bool ok = true;
        size_t completed = 0;
        while (completed < iterations && ok) {
            ok = operation();
            ++completed;
        }
        const uint32_t elapsedUs = micros() - startedUs;
        const uint32_t heapAfter = ESP.getFreeHeap();
        const uint32_t minimumHeap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
        if (monitorHeap)
            heap_caps_monitor_local_minimum_free_size_stop();
        logMetric(name, ok, elapsedUs, completed, bytes, heapBefore, heapAfter, minimumHeap);
        if (updateDisplay) {
            const std::string elapsed = std::to_string(elapsedUs / 1000U) + " ms";
            showStatus(ok ? "Benchmark OK" : "Benchmark failed", ownedName.c_str(), elapsed.c_str());
            delay(250);
        }
        return ok;
    }

    template<typename Operation>
    bool runTimed(std::string_view name, Operation&& operation, size_t bytes = 0) {
        return runTimed(name, 1, std::forward<Operation>(operation), bytes);
    }

    void logWpmSweep(std::string_view phase, std::span<const uint32_t> sortedSamples) {
        if (sortedSamples.empty())
            return;

        using Wpm = decltype(settings::ReadingSettings::wpm);
        const uint64_t total = std::accumulate(sortedSamples.begin(), sortedSamples.end(), uint64_t{0});
        const uint32_t averageUs = static_cast<uint32_t>(total / sortedSamples.size());
        const auto percentile = [&](size_t percent) {
            return sortedSamples[(sortedSamples.size() - 1) * percent / 100];
        };

        size_t transitionCount = 0;
        size_t previousMisses = sortedSamples.size() + 1;
        for (uint16_t wpm = Wpm::min(); wpm <= Wpm::max(); wpm += Wpm::step()) {
            const uint32_t deadlineUs = 60'000'000UL / wpm;
            const auto firstLate = std::ranges::upper_bound(sortedSamples, deadlineUs);
            const size_t misses = static_cast<size_t>(sortedSamples.end() - firstLate);
            if (misses == previousMisses)
                continue;
            previousMisses = misses;
            ++transitionCount;
            ESP_LOGI("bench", "wpm_transition phase=%.*s wpm=%u deadline_misses=%u", static_cast<int>(phase.size()),
                     phase.data(), static_cast<unsigned>(wpm), static_cast<unsigned>(misses));
        }
        ESP_LOGI("bench",
                 "wpm_sweep phase=%.*s min_wpm=%u max_wpm=%u step_wpm=%u transitions=%u samples=%u "
                 "avg_us=%lu p50_us=%lu p95_us=%lu p99_us=%lu max_us=%lu",
                 static_cast<int>(phase.size()), phase.data(), static_cast<unsigned>(Wpm::min()),
                 static_cast<unsigned>(Wpm::max()), static_cast<unsigned>(Wpm::step()),
                 static_cast<unsigned>(transitionCount), static_cast<unsigned>(sortedSamples.size()),
                 static_cast<unsigned long>(averageUs), static_cast<unsigned long>(percentile(50)),
                 static_cast<unsigned long>(percentile(95)), static_cast<unsigned long>(percentile(99)),
                 static_cast<unsigned long>(sortedSamples.back()));
    }

    void logLatencyDistribution(std::string_view name, bool ok, std::vector<uint32_t> samples, uint32_t deadlineUs = 0,
                                uint32_t heapBefore = 0, uint32_t heapAfter = 0, uint32_t minimumHeap = 0,
                                bool sweepWpm = false) {
        if (samples.empty()) {
            logMetric(name, false, 0, 0, 0, heapBefore, heapAfter, minimumHeap);
            return;
        }

        const size_t deadlineMisses =
            deadlineUs == 0 ? 0 : static_cast<size_t>(std::ranges::count_if(samples, [deadlineUs](uint32_t us) {
                return us > deadlineUs;
            }));
        const uint64_t total = std::accumulate(samples.begin(), samples.end(), uint64_t{0});
        std::ranges::sort(samples);
        const auto percentile = [&](size_t percent) {
            return samples[(samples.size() - 1) * percent / 100];
        };
        const std::string prefix{name};
        logMetric(name, ok, static_cast<uint32_t>(std::min<uint64_t>(total, UINT32_MAX)), samples.size(), 0, heapBefore,
                  heapAfter, minimumHeap, deadlineMisses);
        logMetric(prefix + "_p50", ok, percentile(50));
        logMetric(prefix + "_p95", ok, percentile(95));
        logMetric(prefix + "_p99", ok, percentile(99));
        logMetric(prefix + "_max", ok, samples.back());
        if (sweepWpm)
            logWpmSweep(name, samples);
    }

    bool openReadingFixture(screens::ReaderScreen& reader, std::string_view path, std::string_view metric) {
        reader.store.close();
        reader.session.metadata.clear();
        BookLibrary::Listing listing;
        listing.push_back({.path = std::string{path}});
        const bool opened = runTimed(metric, [&] {
            return IndexedBook::load(0, listing, reader.store, reader.session.metadata,
                                     {.allowIndexBuild = true, .allowEpubConversion = false});
        });
        if (!opened)
            return false;

        reader.session.state = {};
        ReadingLoop::setBookStore(reader.session, reader.store, millis());
        return true;
    }

    bool benchmarkSequentialReading(screens::ReaderScreen& reader, settings::ReadingSettings& settings,
                                    const StorageManager& storage, const Board::Power::BatteryState& battery,
                                    Preferences& preferences, std::string_view name, size_t requestedFrames,
                                    bool sweepWpm = false) {
        const size_t frameCount = std::min(requestedFrames, ReadingLoop::wordCount(reader.session));
        if (frameCount == 0)
            return false;

        using Wpm = decltype(settings::ReadingSettings::wpm);
        const uint16_t savedWpm = settings.wpm;
        if (sweepWpm)
            settings.wpm = Wpm::max();

        std::vector<uint32_t> frameLatency;
        std::vector<uint32_t> updateLatency;
        std::vector<uint32_t> cycleLatency;
        frameLatency.reserve(frameCount);
        updateLatency.reserve(frameCount - 1);
        cycleLatency.reserve(frameCount - 1);
        size_t slowestIndex = reader.session.state.wordIndex;
        uint32_t slowestUs = 0;
        uint32_t slowestDrawUs = 0;
        uint32_t slowestFlushUs = 0;
        ui::fonts::RFontFileCache::Stats slowestIo;
        bool ok = true;
        const uint32_t deadlineUs = 60'000'000UL / settings.wpm;
        const uint32_t acceptanceDeadlineUs = sweepWpm ? 60'000U : deadlineUs;
        bool frameDeadlineMet = true;
        bool cycleDeadlineMet = true;

        gDisplay.invalidate();
        ReadingLoop::start(reader.session, millis());
        const uint32_t heapBefore = ESP.getFreeHeap();
        const bool monitorHeap = heap_caps_monitor_local_minimum_free_size_start() == ESP_OK;
        for (size_t frame = 0; frame < frameCount; ++frame) {
            const size_t wordIndex = reader.session.state.wordIndex;
            if (sweepWpm)
                reader.fonts.resetFileCacheStats();
            const uint32_t startedUs = micros();
            gDisplay.beginFrame(static_cast<uint8_t>(screens::Screen::Reader));
            reader.draw(gDisplay, storage, battery, millis());
            const uint32_t drawUs = micros() - startedUs;
            const uint32_t flushStartedUs = micros();
            gDisplay.endFrame();
            const uint32_t flushUs = micros() - flushStartedUs;
            const uint32_t elapsedUs = micros() - startedUs;
            frameLatency.push_back(elapsedUs);
            if (elapsedUs > slowestUs) {
                slowestUs = elapsedUs;
                slowestIndex = wordIndex;
                slowestDrawUs = drawUs;
                slowestFlushUs = flushUs;
                if (sweepWpm)
                    slowestIo = reader.fonts.fileCacheStats();
            }
            if (elapsedUs > acceptanceDeadlineUs) {
                frameDeadlineMet = false;
                const std::string_view word = ReadingLoop::wordAt(reader.session, wordIndex);
                const std::string_view locale = reader.session.metadata.localeAt(wordIndex);
                const auto io = reader.fonts.fileCacheStats();
                ESP_LOGI("bench",
                         "reading_miss phase=%.*s index=%u us=%lu draw=%lu flush=%lu locale=%.*s "
                         "scripts=%lu bytes=%u font_reads=%lu blocks=%lu seeks=%lu loaded=%lu",
                         static_cast<int>(name.size()), name.data(), static_cast<unsigned>(wordIndex),
                         static_cast<unsigned long>(elapsedUs), static_cast<unsigned long>(drawUs),
                         static_cast<unsigned long>(flushUs), static_cast<int>(locale.size()), locale.data(),
                         static_cast<unsigned long>(UnicodeText::scriptsIn(word)), static_cast<unsigned>(word.size()),
                         static_cast<unsigned long>(io.logicalReads), static_cast<unsigned long>(io.blockReads),
                         static_cast<unsigned long>(io.seeks), static_cast<unsigned long>(io.loadedBytes));
            }

            if (frame + 1 == frameCount)
                break;
            const uint32_t duration = ReadingLoop::currentWordDurationMs(reader.session, settings);
            const size_t previousIndex = reader.session.state.wordIndex;
            if (sweepWpm)
                reader.fonts.resetFileCacheStats();
            const uint32_t updateStartedUs = micros();
            reader.update(preferences, millis());
            if (reader.session.state.wordIndex == previousIndex) {
                reader.session.lastAdvanceMs = millis() - duration;
                reader.update(preferences, millis());
            }
            const uint32_t updateUs = micros() - updateStartedUs;
            const uint32_t cycleUs = micros() - startedUs;
            updateLatency.push_back(updateUs);
            cycleLatency.push_back(cycleUs);
            if (cycleUs > acceptanceDeadlineUs) {
                cycleDeadlineMet = false;
                const auto io = reader.fonts.fileCacheStats();
                ESP_LOGI("bench",
                         "reading_cycle_miss phase=%.*s index=%u us=%lu frame=%lu update=%lu "
                         "font_reads=%lu blocks=%lu seeks=%lu loaded=%lu",
                         static_cast<int>(name.size()), name.data(), static_cast<unsigned>(wordIndex),
                         static_cast<unsigned long>(cycleUs), static_cast<unsigned long>(elapsedUs),
                         static_cast<unsigned long>(updateUs), static_cast<unsigned long>(io.logicalReads),
                         static_cast<unsigned long>(io.blockReads), static_cast<unsigned long>(io.seeks),
                         static_cast<unsigned long>(io.loadedBytes));
            }
            if (reader.session.state.wordIndex != previousIndex + 1) {
                ok = false;
                break;
            }
        }
        ReadingLoop::pause(reader.session);
        const uint32_t heapAfter = ESP.getFreeHeap();
        const uint32_t minimumHeap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
        if (monitorHeap)
            heap_caps_monitor_local_minimum_free_size_stop();

        const std::string prefix{name};
        logLatencyDistribution(prefix + "_frame", ok && frameDeadlineMet, std::move(frameLatency), acceptanceDeadlineUs,
                               heapBefore, heapAfter, minimumHeap, sweepWpm);
        logLatencyDistribution(prefix + "_update", ok, std::move(updateLatency));
        logLatencyDistribution(prefix + "_cycle", ok && cycleDeadlineMet, std::move(cycleLatency), acceptanceDeadlineUs,
                               0, 0, 0, sweepWpm);
        const std::string_view word = ReadingLoop::wordAt(reader.session, slowestIndex);
        const std::string_view locale = reader.session.metadata.localeAt(slowestIndex);
        ESP_LOGI("bench",
                 "reading_peak phase=%s index=%u us=%lu draw=%lu flush=%lu locale=%.*s scripts=%lu "
                 "bytes=%u font_reads=%lu blocks=%lu seeks=%lu loaded=%lu word=%.*s",
                 prefix.c_str(), static_cast<unsigned>(slowestIndex), static_cast<unsigned long>(slowestUs),
                 static_cast<unsigned long>(slowestDrawUs), static_cast<unsigned long>(slowestFlushUs),
                 static_cast<int>(locale.size()), locale.data(),
                 static_cast<unsigned long>(UnicodeText::scriptsIn(word)), static_cast<unsigned>(word.size()),
                 static_cast<unsigned long>(slowestIo.logicalReads), static_cast<unsigned long>(slowestIo.blockReads),
                 static_cast<unsigned long>(slowestIo.seeks), static_cast<unsigned long>(slowestIo.loadedBytes),
                 static_cast<int>(word.size()), word.data());
        settings.wpm = savedWpm;
        return ok && frameDeadlineMet && cycleDeadlineMet;
    }

    bool benchmarkPageScrubbing(screens::ReaderScreen& reader, const StorageManager& storage,
                                const Board::Power::BatteryState& battery, std::string_view name) {
        const size_t wordCount = ReadingLoop::wordCount(reader.session);
        if (wordCount == 0)
            return false;

        std::vector<uint32_t> latency;
        latency.reserve(kReadingScrubJumps);
        size_t slowestIndex = 0;
        uint32_t slowestUs = 0;
        uint32_t random = 0x9E3779B9U;
        gDisplay.invalidate();
        const uint32_t heapBefore = ESP.getFreeHeap();
        const bool monitorHeap = heap_caps_monitor_local_minimum_free_size_start() == ESP_OK;
        for (size_t jump = 0; jump < kReadingScrubJumps; ++jump) {
            random = random * 1664525U + 1013904223U;
            const size_t wordIndex = random % wordCount;
            const uint32_t startedUs = micros();
            ReadingLoop::seekTo(reader.session, wordIndex);
            gDisplay.beginFrame(static_cast<uint8_t>(screens::Screen::Reader));
            reader.draw(gDisplay, storage, battery, millis());
            gDisplay.endFrame();
            const uint32_t elapsedUs = micros() - startedUs;
            latency.push_back(elapsedUs);
            if (elapsedUs > slowestUs) {
                slowestUs = elapsedUs;
                slowestIndex = wordIndex;
            }
        }
        const uint32_t heapAfter = ESP.getFreeHeap();
        const uint32_t minimumHeap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
        if (monitorHeap)
            heap_caps_monitor_local_minimum_free_size_stop();

        const std::string prefix{name};
        logLatencyDistribution(prefix + "_frame", true, std::move(latency), 0, heapBefore, heapAfter, minimumHeap);
        ESP_LOGI("bench", "reading_peak phase=%s index=%u us=%lu locale=%.*s scripts=%lu", prefix.c_str(),
                 static_cast<unsigned>(slowestIndex), static_cast<unsigned long>(slowestUs),
                 static_cast<int>(reader.session.metadata.localeAt(slowestIndex).size()),
                 reader.session.metadata.localeAt(slowestIndex).data(),
                 static_cast<unsigned long>(UnicodeText::scriptsIn(ReadingLoop::wordAt(reader.session, slowestIndex))));
        return true;
    }

    bool benchmarkSdRandomReads(size_t bytesPerRead) {
        File file = Board::Storage::filesystem().open(kSdWritePath, FILE_READ);
        if (!file || bytesPerRead == 0 || bytesPerRead > kSdChunkBytes || file.size() < bytesPerRead) {
            if (file)
                file.close();
            return false;
        }
        uint8_t* buffer = static_cast<uint8_t*>(heap_caps_malloc(kSdChunkBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
        if (buffer == nullptr) {
            file.close();
            return false;
        }

        uint32_t state = 0x9E3779B9U ^ static_cast<uint32_t>(bytesPerRead);
        const uint32_t maximumOffset = static_cast<uint32_t>(file.size() - bytesPerRead);
        const std::string metric = "sd_random_read_" + std::to_string(bytesPerRead);
        const bool ok = runTimed(
            metric, kSdRandomIterations,
            [&] {
                state = state * 1664525U + 1013904223U;
                uint32_t offset = state % (maximumOffset + 1U);
                if (bytesPerRead >= 512)
                    offset &= ~511U;
                if (!file.seek(offset) || file.read(buffer, bytesPerRead) != bytesPerRead)
                    return false;
                for (size_t index = 0; index < bytesPerRead; ++index) {
                    const uint32_t value = offset + static_cast<uint32_t>(index);
                    const uint8_t expected = static_cast<uint8_t>((value * 33U) ^ (value >> 3) ^ 0xA5U);
                    if (buffer[index] != expected)
                        return false;
                }
                return true;
            },
            bytesPerRead * kSdRandomIterations, false);
        file.close();
        heap_caps_free(buffer);
        return ok;
    }

    void resetFontIo(const FontCatalog::Face& face) {
        if (face.raster.get().fileCache)
            face.raster.get().fileCache->resetStats();
    }

    FontCatalog::Face loadFaceTimed(size_t familyIndex, size_t sizeIndex, std::string_view metric) {
        const uint32_t heapBefore = ESP.getFreeHeap();
        const bool monitorHeap = heap_caps_monitor_local_minimum_free_size_start() == ESP_OK;
        const uint32_t startedUs = micros();
        FontCatalog::Face face = gFonts.loadFace(familyIndex, sizeIndex);
        const uint32_t elapsedUs = micros() - startedUs;
        const uint32_t heapAfter = ESP.getFreeHeap();
        const uint32_t minimumHeap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
        if (monitorHeap)
            heap_caps_monitor_local_minimum_free_size_stop();
        logMetric(metric, true, elapsedUs, 1, 0, heapBefore, heapAfter, minimumHeap);
        return face;
    }

    void logFontIo(std::string_view phase, const FontCatalog::Face& face) {
        const auto& font = face.raster.get();
        if (!font.fileCache) {
            ESP_LOGI("bench", "font_io phase=%.*s resident_metrics=%u resident_bitmap=%u",
                     static_cast<int>(phase.size()), phase.data(), font.glyphs != nullptr ? 1U : 0U,
                     font.bitmap != nullptr ? 1U : 0U);
            return;
        }
        const auto& stats = font.fileCache->stats();
        ESP_LOGI("bench",
                 "font_io phase=%.*s resident_metrics=%u resident_bitmap=%u logical_reads=%lu "
                 "block_reads=%lu seeks=%lu requested_bytes=%lu loaded_bytes=%lu",
                 static_cast<int>(phase.size()), phase.data(), font.glyphs != nullptr ? 1U : 0U,
                 font.bitmap != nullptr ? 1U : 0U, static_cast<unsigned long>(stats.logicalReads),
                 static_cast<unsigned long>(stats.blockReads), static_cast<unsigned long>(stats.seeks),
                 static_cast<unsigned long>(stats.requestedBytes), static_cast<unsigned long>(stats.loadedBytes));
    }

    const TextSample& sampleFor(const FontCatalog::Family& family) {
        if ((family.scriptMask & UnicodeText::ScriptArabic) != 0)
            return kArabicSample;
        if ((family.scriptMask & UnicodeText::ScriptHebrew) != 0)
            return kHebrewSample;
        if ((family.scriptMask & (UnicodeText::ScriptHiragana | UnicodeText::ScriptKatakana)) != 0)
            return kCjkSample;
        if ((family.scriptMask & UnicodeText::ScriptHan) != 0)
            return kChineseSample;
        if ((family.scriptMask & UnicodeText::ScriptMath) != 0)
            return kMathSample;
        return kLatinSample;
    }

    bool shape(const FontCatalog::Face& face, const TextSample& sample,
               std::vector<ui::fonts::PositionedGlyph>& glyphs) {
        if (!face.shaper)
            return false;
        const size_t offset = sample.paragraph.find(sample.word);
        if (offset == std::string_view::npos)
            return false;
        glyphs.clear();
        return face.shaper
            ->shape(sample.paragraph, offset, sample.word.size(), sample.rightToLeft, sample.locale, gText, glyphs)
            .has_value();
    }

    bool renderParagraph(const TextSample& sample, const ui::Rect& area, size_t sizeIndex, int16_t& baseline,
                         size_t fixedFamily = SIZE_MAX) {
        const int16_t left = static_cast<int16_t>(area.x + 4);
        const int16_t right = static_cast<int16_t>(area.x + area.w - 4);
        int16_t cursor = sample.rightToLeft ? right : left;
        size_t offset = 0;
        std::vector<ui::fonts::PositionedGlyph> glyphs;
        glyphs.reserve(32);
        while (offset < sample.paragraph.size()) {
            while (offset < sample.paragraph.size() && sample.paragraph[offset] == ' ')
                ++offset;
            if (offset == sample.paragraph.size())
                break;
            const size_t found = sample.paragraph.find(' ', offset);
            const size_t end = found == std::string_view::npos ? sample.paragraph.size() : found;
            const std::string_view word = sample.paragraph.substr(offset, end - offset);
            const uint32_t scripts = UnicodeText::scriptsIn(word);
            const size_t selected = fixedFamily == SIZE_MAX
                                      ? FontCatalog::selectFamily(gFonts.families(), "literata", sample.locale, scripts)
                                      : fixedFamily;
            FontCatalog::Face face = gFonts.loadFace(selected, sizeIndex);
            gText.setFont(face.raster.get());

            int16_t advance = 0;
            bool shaped = false;
            if (face.shaper) {
                glyphs.clear();
                const auto result = face.shaper->shape(sample.paragraph, offset, word.size(), sample.rightToLeft,
                                                       sample.locale, gText, glyphs);
                if (!result)
                    return false;
                advance = *result;
                shaped = true;
            } else {
                advance = gText.textAdvance(word);
            }
            const int16_t space = std::max<int16_t>(2, gText.glyphAdvance(' '));
            const bool wrap = sample.rightToLeft ? cursor - advance < left : cursor + advance > right;
            if (wrap) {
                baseline = static_cast<int16_t>(baseline + face.raster.get().yAdvance + 2);
                cursor = sample.rightToLeft ? right : left;
            }
            if (baseline >= area.y + area.h - 4)
                return true;
            const int16_t x = sample.rightToLeft ? static_cast<int16_t>(cursor - advance) : cursor;
            const int16_t drawn = shaped ? gText.drawGlyphs(glyphs, x, baseline) : gText.drawString(word, x, baseline);
            if (drawn < 0)
                return false;
            cursor = sample.rightToLeft ? static_cast<int16_t>(x - space) : static_cast<int16_t>(x + drawn + space);
            offset = end;
        }
        baseline = static_cast<int16_t>(baseline + 16);
        return true;
    }

    bool benchmarkBidi(std::string_view id, const TextSample& sample) {
        BidiText::Analysis analysis;
        BidiText::Line line;
        const std::string metric = "bidi_" + std::string{id} + "_paragraph";
        return runTimed(
            metric, kCpuIterations,
            [&] {
                if (!analysis.reset(sample.paragraph, sample.rightToLeft ? TextDirection::rtl : TextDirection::ltr))
                    return false;
                return analysis.resolve({0, sample.paragraph.size()}, line).has_value() && !line.empty();
            },
            0, false);
    }

    bool benchmarkFamily(size_t familyIndex) {
        const FontCatalog::Family& family = gFonts.families()[familyIndex];
        const TextSample& sample = sampleFor(family);
        const std::string prefix = "font_" + family.id;
        const ui::Rect area = renderArea();
        showRenderScreen(family.label, sample.id);

        FontCatalog::Face face = loadFaceTimed(familyIndex, 1, prefix + "_load_rsvp");
        gText.setFont(face.raster.get());
        gText.setTextColor(gDisplay.color(ui::themes::ColorRole::Foreground),
                           gDisplay.color(ui::themes::ColorRole::Background));
        bool ok = true;
        if (face.shaper) {
            std::vector<ui::fonts::PositionedGlyph> glyphs;
            glyphs.reserve(sample.word.size());
            resetFontIo(face);
            ok &= runTimed(
                prefix + "_shape_cold", 1,
                [&] {
                    return shape(face, sample, glyphs);
                },
                0, false);
            logFontIo(prefix + "_shape_cold", face);
            resetFontIo(face);
            ok &= runTimed(
                prefix + "_shape_warm", kCpuIterations,
                [&] {
                    return shape(face, sample, glyphs);
                },
                0, false);
            logFontIo(prefix + "_shape_warm", face);
            if (!glyphs.empty()) {
                clearRenderArea();
                gText.clearBitmapCache();
                resetFontIo(face);
                ok &= runTimed(
                    prefix + "_render_rsvp_cold", 1,
                    [&] {
                        return gText.drawGlyphs(glyphs, static_cast<int16_t>(area.x + 12),
                                                static_cast<int16_t>(area.y + area.h / 2))
                            >= 0;
                    },
                    0, false);
                logFontIo(prefix + "_render_rsvp_cold", face);
                resetFontIo(face);
                ok &= runTimed(
                    prefix + "_render_rsvp", kRenderIterations,
                    [&] {
                        return gText.drawGlyphs(glyphs, static_cast<int16_t>(area.x + 12),
                                                static_cast<int16_t>(area.y + area.h / 2))
                            >= 0;
                    },
                    0, false);
                logFontIo(prefix + "_render_rsvp", face);
                Board::Display::gfx().flush();
            }
        } else {
            clearRenderArea();
            gText.clearBitmapCache();
            resetFontIo(face);
            ok &= runTimed(
                prefix + "_render_rsvp_cold", 1,
                [&] {
                    return gText.drawString(sample.word, static_cast<int16_t>(area.x + 12),
                                            static_cast<int16_t>(area.y + area.h / 2))
                        >= 0;
                },
                0, false);
            logFontIo(prefix + "_render_rsvp_cold", face);
            resetFontIo(face);
            ok &= runTimed(
                prefix + "_render_rsvp", kRenderIterations,
                [&] {
                    return gText.drawString(sample.word, static_cast<int16_t>(area.x + 12),
                                            static_cast<int16_t>(area.y + area.h / 2))
                        >= 0;
                },
                0, false);
            logFontIo(prefix + "_render_rsvp", face);
            if (!sample.nextWord.empty()) {
                resetFontIo(face);
                ok &= runTimed(
                    prefix + "_prefetch_next", 1,
                    [&] {
                        gText.prepare(sample.nextWord);
                        return true;
                    },
                    0, false);
                logFontIo(prefix + "_prefetch_next", face);
                clearRenderArea();
                resetFontIo(face);
                ok &= runTimed(
                    prefix + "_render_prefetched", kRenderIterations,
                    [&] {
                        return gText.drawString(sample.nextWord, static_cast<int16_t>(area.x + 12),
                                                static_cast<int16_t>(area.y + area.h / 2))
                            >= 0;
                    },
                    0, false);
                logFontIo(prefix + "_render_prefetched", face);
            }
            Board::Display::gfx().flush();
        }

        face = loadFaceTimed(familyIndex, RFont4::kCompactStrikeIndex, prefix + "_load_page");
        gText.setFont(face.raster.get());
        clearRenderArea();
        resetFontIo(face);
        ok &= runTimed(
            prefix + "_render_page", kRenderIterations,
            [&] {
                int16_t baseline = static_cast<int16_t>(area.y + 14);
                return renderParagraph(sample, area, RFont4::kCompactStrikeIndex, baseline, familyIndex);
            },
            0, false);
        logFontIo(prefix + "_render_page", face);
        Board::Display::gfx().flush();
        gFonts.clearLoaded();
        delay(1);
        return ok;
    }

    bool benchmarkMixedPage() {
        const ui::Rect area = renderArea();
        showRenderScreen("Multilingual page", "Latin / Hebrew / Arabic / CJK / Math");
        clearRenderArea();
        gFonts.clearLoaded();
        const bool ok = runTimed(
            "multilingual_page_pipeline", kRenderIterations,
            [&] {
                int16_t baseline = static_cast<int16_t>(area.y + 12);
                return renderParagraph(kLatinSample, area, RFont4::kCompactStrikeIndex, baseline)
                    && renderParagraph(kHebrewSample, area, RFont4::kCompactStrikeIndex, baseline)
                    && renderParagraph(kArabicSample, area, RFont4::kCompactStrikeIndex, baseline)
                    && renderParagraph(kCjkSample, area, RFont4::kCompactStrikeIndex, baseline)
                    && renderParagraph(kMathSample, area, RFont4::kCompactStrikeIndex, baseline);
            },
            0, false);
        Board::Display::gfx().flush();
        gFonts.clearLoaded();
        return ok;
    }

    bool benchmarkFonts() {
        if (!gText.begin())
            return false;
        const bool catalogOk = runTimed("font_catalog_load", [] {
            gFonts.loadFromSd();
            return !gFonts.families().empty();
        });
        if (!catalogOk)
            return false;

        ESP_LOGI("bench", "font_catalog families=%u", static_cast<unsigned>(gFonts.families().size()));
        bool ok = benchmarkBidi("hebrew", kHebrewSample) && benchmarkBidi("arabic", kArabicSample);
        const TextSample mixedBidi{"mixed", "en", kMixedParagraph, "עברית", {}, false};
        ok = benchmarkBidi("mixed", mixedBidi) && ok;
        for (size_t index = 0; index < gFonts.families().size(); ++index)
            ok = benchmarkFamily(index) && ok;
        return benchmarkMixedPage() && ok;
    }

    bool benchmarkReadingSimulation() {
        if (!StorageFiles::fileExistsWithBytes(kDraculaRsvpPath)
            || !StorageFiles::fileExistsWithBytes(kMultilingualRsvpPath)
            || !StorageFiles::fileExistsWithBytes(kVerticalRsvpPath)) {
            ESP_LOGE("bench", "reading fixtures missing dracula=%u multilingual=%u vertical=%u",
                     StorageFiles::fileExistsWithBytes(kDraculaRsvpPath) ? 1U : 0U,
                     StorageFiles::fileExistsWithBytes(kMultilingualRsvpPath) ? 1U : 0U,
                     StorageFiles::fileExistsWithBytes(kVerticalRsvpPath) ? 1U : 0U);
            return false;
        }

        if (auto created = StorageFiles::ensureDirectory(StoragePaths::kLibraryPath); !created) {
            ESP_LOGE("bench", "reading_books_directory_failed error=%s", created.error().message().c_str());
            return false;
        }

        settings::ReadingSettings settings;
        settings.wpm = 600;
        settings.phantomWords = true;
        auto reader = std::make_unique<screens::ReaderScreen>(Board::Display::gfx(), settings);
        reader->begin(gTheme);
        reader->fonts.loadFromSd();
        StorageManager storage;
        Board::Power::BatteryState battery{};
        Preferences preferences;

        bool ok = openReadingFixture(*reader, kDraculaRsvpPath, "reading_dracula_open");
        if (ok) {
            settings.mode = settings::ReadingMode::rsvp;
            runTimed("reading_latin_fonts_prepare", [&] {
                reader->refreshTypography();
                return true;
            });
            ok = benchmarkSequentialReading(*reader, settings, storage, battery, preferences, "reading_latin_rsvp",
                                            kReadingSessionWords);
        }

        ok = openReadingFixture(*reader, kMultilingualRsvpPath, "reading_multilingual_open") && ok;
        if (reader->store.isOpen()) {
            settings.mode = settings::ReadingMode::rsvp;
            runTimed("reading_multilingual_rsvp_fonts_prepare", [&] {
                reader->refreshTypography();
                return true;
            });
            ok = benchmarkSequentialReading(*reader, settings, storage, battery, preferences,
                                            "reading_multilingual_rsvp", ReadingLoop::wordCount(reader->session), true)
              && ok;

            ReadingLoop::seekTo(reader->session, 0);
            settings.mode = settings::ReadingMode::page;
            runTimed("reading_multilingual_page_fonts_prepare", [&] {
                reader->refreshTypography();
                return true;
            });
            ok = benchmarkSequentialReading(*reader, settings, storage, battery, preferences,
                                            "reading_multilingual_page", ReadingLoop::wordCount(reader->session))
              && ok;
        }

        ok = openReadingFixture(*reader, kVerticalRsvpPath, "reading_vertical_cjk_open") && ok;
        if (reader->store.isOpen()) {
            if (reader->session.metadata.writingMode != WritingMode::verticalRl) {
                ESP_LOGE("bench", "vertical fixture lost writing mode");
                ok = false;
            } else {
                settings.mode = settings::ReadingMode::rsvp;
                reader->refreshTypography();
                ok = benchmarkSequentialReading(*reader, settings, storage, battery, preferences,
                                                "reading_vertical_cjk_rsvp",
                                                ReadingLoop::wordCount(reader->session))
                  && ok;
                ReadingLoop::seekTo(reader->session, 0);
                settings.mode = settings::ReadingMode::page;
                reader->refreshTypography();
                ok = benchmarkSequentialReading(*reader, settings, storage, battery, preferences,
                                                "reading_vertical_cjk_page",
                                                ReadingLoop::wordCount(reader->session))
                  && ok;
            }
        }

        ok = openReadingFixture(*reader, kDraculaRsvpPath, "reading_dracula_reopen") && ok;
        if (reader->store.isOpen()) {
            settings.mode = settings::ReadingMode::page;
            runTimed("reading_latin_page_fonts_prepare", [&] {
                reader->refreshTypography();
                return true;
            });
            ok = benchmarkPageScrubbing(*reader, storage, battery, "reading_latin_scrub") && ok;
        }
        return ok;
    }

    bool beginDisplay() {
        gDisplayReady = Board::Display::begin();
        if (gDisplayReady)
            gDisplay.setOrientation(Board::Display::defaultUiOrientation());
        gDisplay.setTheme(gTheme);
        return gDisplayReady;
    }
    bool beginInput() {
        const bool started = Input::begin();
        gDisplay.setTouchSource({.surface = Board::Input::touchSurface(), .poll = &Input::pollTouch});
        return started;
    }
    bool beginAudio() {
        return Board::Audio::begin();
    }
    bool beepAudio() {
        return Board::Audio::beep();
    }

#if !RSVP_USB_MSC_ENABLED
    bool startButtonHeld() {
        const Input::PressActions actions = Board::Input::currentActions();
        return actions.shortPress != Input::ActionNone || actions.longPress != Input::ActionNone;
    }

    void waitForStartInput() {
        showStatus("Benchmark", "Tap or press button", "Auto-starts in 10 seconds");
        ESP_LOGW("bench", "waiting_for_start_input");

        const uint32_t settleStartMs = millis();
        while (millis() - settleStartMs < 500) {
            delay(10);
        }
        bool inputWasHeld = startButtonHeld();
        const uint32_t waitStartedMs = millis();
        uint32_t lastReminderMs = millis();
        while (true) {
            if (millis() - waitStartedMs >= 10000)
                break;
            Input::ActionMask event;
            if (gDisplay.pollTouch(millis())) {
                break;
            }
            Input::poll(event);

            const bool held = startButtonHeld();
            if (!inputWasHeld && held) {
                break;
            }
            inputWasHeld = held;

            if (millis() - lastReminderMs > 3000) {
                ESP_LOGW("bench", "still_waiting_for_start_input");
                lastReminderMs = millis();
            }
            delay(20);
        }

        ESP_LOGI("bench", "start_input_received");
        showStatus("Benchmark", "Starting", "");
        delay(300);
    }
#endif

} // namespace

namespace Benchmark {

    void run() {
        bool mounted = false;
        int mountedFrequencyKhz = 0;
        runTimed("display_begin", beginDisplay);
        runTimed("input_begin", beginInput);
        if (!prepareBenchmarkStorage())
            return;

        ESP_LOGI("bench", "start board=%s id=%s", Board::Config::BOARD_LABEL, Board::Config::BOARD_ID);
#if !RSVP_USB_MSC_ENABLED
        waitForStartInput();
#endif
        runTimed("display_push_full", benchmarkDisplayPush,
                 static_cast<size_t>(gDisplay.width()) * static_cast<size_t>(gDisplay.height()) * sizeof(uint16_t));
        if (Board::Audio::available()) {
            runTimed("audio_begin", beginAudio);
            runTimed("audio_beep", beepAudio);
        } else {
            logMetric("audio_begin", false, 0);
            logMetric("audio_beep", false, 0);
        }

        const uint32_t mountStartedUs = micros();
        mounted = SdCard::mount(mounted, &mountedFrequencyKhz);
        logMetric("sd_mount", mounted, micros() - mountStartedUs);
        ESP_LOGI("bench", "sd_frequency_khz=%d", mountedFrequencyKhz);
        if (mounted) {
            const bool sdProbeReady = runTimed("sd_write_read", benchmarkSdWriteRead, kSdProbeBytes * 2);
            if (sdProbeReady) {
                benchmarkSdRandomReads(1);
                benchmarkSdRandomReads(sizeof(RFont4::GlyphRecord));
                benchmarkSdRandomReads(512);
                benchmarkSdRandomReads(kSdChunkBytes);
            }
            Board::Storage::filesystem().remove(kSdWritePath);
            benchmarkFonts();
            const bool converted = runTimed("epub_dracula_convert", [] {
                return benchmarkEpubConversion(kDraculaEpubPath, kDraculaRsvpPath, "Dracula");
            });
            const bool verticalConverted = runTimed("epub_vertical_cjk_convert", [] {
                return benchmarkEpubConversion(kVerticalEpubPath, kVerticalRsvpPath, "Vertical CJK");
            });
            if (converted && verticalConverted)
                benchmarkReadingSimulation();
            else
                logMetric("reading_simulation", false, 0);
        } else {
            logMetric("sd_write_read", false, 0, kSdProbeBytes * 2);
            logMetric("epub_dracula_convert", false, 0);
            logMetric("reading_simulation", false, 0);
        }

        ESP_LOGI("bench", "done");
        showStatus("Benchmark", "Done", "Check serial log");
#if RSVP_USB_MSC_ENABLED
        delay(1500);
        ESP.restart();
#endif
    }

} // namespace Benchmark

void setup() {
    Serial.begin(115200);
    Logger::begin();
    delay(50);
    Board::System::begin();
    const uint32_t serialWaitStart = millis();
    while (!Serial && millis() - serialWaitStart < 2000)
        delay(10);
    Board::System::logStartupDiagnostics();
    if (!settings::initializeNvsEncryption()) {
        ESP_LOGE("main", "encrypted NVS initialization failed; restarting");
        delay(1000);
        ESP.restart();
        return;
    }
    ESP_LOGI("main", "benchmark setup");
    Benchmark::run();
}

void loop() {
    delay(1000);
}
