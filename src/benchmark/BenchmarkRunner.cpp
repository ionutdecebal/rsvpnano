#include "benchmark/BenchmarkRunner.h"

#include <Arduino.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>

#include "board/Board.h"
#include "board/BoardInput.h"
#include "converter/EpubConverter.h"
#include "display/DisplayManager.h"
#include "input/Input.h"
#include "storage/fs/SdDiagnostics.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"

namespace {

constexpr const char *kBenchmarkDir = "/benchmark";
constexpr const char *kSdWritePath = "/benchmark/sd-write.bin";
constexpr const char *kDraculaEpubPath = "/benchmark/Dracula-epub.epub";
constexpr const char *kDraculaRsvpPath = "/benchmark/Dracula-epub.rsvp";
constexpr size_t kDisplayRowsPerChunk = 16;
constexpr size_t kSdProbeBytes = 256UL * 1024UL;
constexpr size_t kSdChunkBytes = 4096;
constexpr uint16_t kDisplayColorA = 0x0000;
constexpr uint16_t kDisplayColorB = 0xFFFF;

DisplayManager gDisplay;
bool gDisplayReady = false;

void showStatus(const String &title, const String &line1 = "", const String &line2 = "") {
  Serial.printf("[bench] screen title=%s line1=%s line2=%s\n", title.c_str(), line1.c_str(),
                line2.c_str());
  if (gDisplayReady) {
    gDisplay.renderStatus(title, line1, line2);
  }
}

void logMetric(const char *name, bool ok, uint32_t elapsedMs, size_t bytes = 0) {
  const uint32_t rateKiBPerSecond =
      elapsedMs > 0 && bytes > 0
          ? static_cast<uint32_t>((static_cast<uint64_t>(bytes) * 1000ULL) /
                                  (static_cast<uint64_t>(elapsedMs) * 1024ULL))
          : 0;
  Serial.printf("[bench] metric=%s ok=%u ms=%lu bytes=%lu rate_kib_s=%lu\n", name,
                ok ? 1 : 0, static_cast<unsigned long>(elapsedMs),
                static_cast<unsigned long>(bytes), static_cast<unsigned long>(rateKiBPerSecond));
}

void fillBytes(uint8_t *buffer, size_t bytes, uint32_t offset) {
  for (size_t i = 0; i < bytes; ++i) {
    const uint32_t value = offset + static_cast<uint32_t>(i);
    buffer[i] = static_cast<uint8_t>((value * 33U) ^ (value >> 3) ^ 0xA5U);
  }
}

uint32_t checksumBytes(const uint8_t *buffer, size_t bytes) {
  uint32_t checksum = 2166136261UL;
  for (size_t i = 0; i < bytes; ++i) {
    checksum ^= buffer[i];
    checksum *= 16777619UL;
  }
  return checksum;
}

bool benchmarkDisplayPush() {
  const uint16_t width = Board::Display::nativeWidth();
  const uint16_t height = Board::Display::nativeHeight();
  const uint16_t rows = height < kDisplayRowsPerChunk ? height : kDisplayRowsPerChunk;
  const size_t pixels = static_cast<size_t>(width) * rows;

  uint16_t *buffer = static_cast<uint16_t *>(
      heap_caps_malloc(pixels * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  if (buffer == nullptr) {
    return false;
  }

  for (size_t i = 0; i < pixels; ++i) {
    buffer[i] = (i & 1U) == 0 ? kDisplayColorA : kDisplayColorB;
  }

  bool ok = true;
  for (uint16_t y = 0; y < height && ok; y = static_cast<uint16_t>(y + rows)) {
    const uint16_t chunkRows = static_cast<uint16_t>(min<uint16_t>(rows, height - y));
    ok = Board::Display::pushColors(0, y, width, chunkRows, buffer);
  }

  heap_caps_free(buffer);
  return ok;
}

bool benchmarkSdWriteRead() {
  if (!StorageFiles::ensureDirectory(kBenchmarkDir, "bench")) {
    return false;
  }

  uint8_t *buffer = static_cast<uint8_t *>(malloc(kSdChunkBytes));
  if (buffer == nullptr) {
    return false;
  }

  uint32_t expectedChecksum = 2166136261UL;
  File file = SD_MMC.open(kSdWritePath, FILE_WRITE);
  if (!file) {
    free(buffer);
    return false;
  }

  for (size_t offset = 0; offset < kSdProbeBytes; offset += kSdChunkBytes) {
    const size_t chunk = min(kSdChunkBytes, kSdProbeBytes - offset);
    fillBytes(buffer, chunk, static_cast<uint32_t>(offset));
    expectedChecksum = checksumBytes(buffer, chunk) ^ (expectedChecksum * 16777619UL);
    if (file.write(buffer, chunk) != chunk) {
      file.close();
      free(buffer);
      return false;
    }
  }
  file.flush();
  file.close();

  uint32_t actualChecksum = 2166136261UL;
  file = SD_MMC.open(kSdWritePath, FILE_READ);
  if (!file) {
    free(buffer);
    return false;
  }

  for (size_t offset = 0; offset < kSdProbeBytes; offset += kSdChunkBytes) {
    const size_t chunk = min(kSdChunkBytes, kSdProbeBytes - offset);
    if (file.read(buffer, chunk) != static_cast<int>(chunk)) {
      file.close();
      free(buffer);
      return false;
    }
    actualChecksum = checksumBytes(buffer, chunk) ^ (actualChecksum * 16777619UL);
  }
  file.close();
  SD_MMC.remove(kSdWritePath);
  free(buffer);
  return expectedChecksum == actualChecksum;
}

void reportEpubProgress(const EpubConverter::Options &, const char *line1, const char *line2,
                        int progressPercent) {
  String percentLine = String(progressPercent) + "%";
  if (line2 != nullptr && line2[0] != '\0') {
    percentLine += " ";
    percentLine += line2;
  }
  showStatus("EPUB", line1 == nullptr ? "" : line1, percentLine);
}

bool benchmarkDraculaConversion() {
  if (!StorageFiles::fileExistsWithBytes(kDraculaEpubPath)) {
    Serial.printf("[bench] missing_epub path=%s\n", kDraculaEpubPath);
    showStatus("EPUB missing", "Copy Dracula-epub.epub", "to /benchmark on SD");
    return false;
  }

  SD_MMC.remove(kDraculaRsvpPath);
  SD_MMC.remove(StoragePaths::siblingPathWithExtension(kDraculaEpubPath, StoragePaths::kTempExtension));
  SD_MMC.remove(
      StoragePaths::siblingPathWithExtension(kDraculaEpubPath, StoragePaths::kFailedExtension));

  EpubConverter::Options options;
  options.progressCallback = reportEpubProgress;
  options.progressTitle = "Benchmark";
  options.progressLabel = "Dracula";
  return EpubConverter::convertIfNeeded(kDraculaEpubPath, kDraculaRsvpPath, options);
}

void runTimed(const char *name, bool (*operation)(), size_t bytes = 0) {
  showStatus("Benchmark", name, "Running");
  const uint32_t startedMs = millis();
  const bool ok = operation();
  const uint32_t elapsedMs = millis() - startedMs;
  logMetric(name, ok, elapsedMs, bytes);
  showStatus(ok ? "Benchmark OK" : "Benchmark failed", name,
             String(static_cast<unsigned long>(elapsedMs)) + " ms");
  delay(250);
}

bool beginDisplay() {
  gDisplayReady = gDisplay.begin();
  return gDisplayReady;
}
bool beginInput() { return Input::begin(); }
bool beginAudio() { return Board::Audio::begin(); }
bool beepAudio() { return Board::Audio::beep(); }

bool startButtonHeld() {
  const Input::ControlMask controls = Board::Input::currentControls();
  return Input::hasControl(controls, Input::InputPrimary) ||
         Input::hasControl(controls, Input::InputPower);
}

void waitForStartInput() {
  showStatus("Benchmark", "Tap or press button", "SD data stays in /benchmark");
  Serial.println("[bench] waiting_for_start_input");

  const uint32_t settleStartMs = millis();
  while (millis() - settleStartMs < 500) {
    delay(10);
  }

  bool inputWasHeld = startButtonHeld();
  uint32_t lastReminderMs = millis();
  while (true) {
    Input::Event event;
    if (Input::poll(event, millis()) && Input::isTouchEvent(event) &&
        event.gesture == Input::Gesture::TouchStart) {
      break;
    }

    const bool held = startButtonHeld();
    if (!inputWasHeld && held) {
      break;
    }
    inputWasHeld = held;

    if (millis() - lastReminderMs > 3000) {
      Serial.println("[bench] still_waiting_for_start_input");
      lastReminderMs = millis();
    }
    delay(20);
  }

  Serial.println("[bench] start_input_received");
  showStatus("Benchmark", "Starting", "");
  delay(300);
}

}  // namespace

namespace Benchmark {

void run() {
  Serial.printf("[bench] start board=%s id=%s\n", Board::Config::BOARD_LABEL,
                Board::Config::BOARD_ID);

  bool mounted = false;
  int mountedFrequencyKhz = 0;
  runTimed("display_begin", beginDisplay);
  runTimed("input_begin", beginInput);
  waitForStartInput();
  runTimed("display_push_full", benchmarkDisplayPush,
           static_cast<size_t>(Board::Display::nativeWidth()) *
               static_cast<size_t>(Board::Display::nativeHeight()) * sizeof(uint16_t));
  if (Board::Audio::available()) {
    runTimed("audio_begin", beginAudio);
    runTimed("audio_beep", beepAudio);
  } else {
    logMetric("audio_begin", false, 0);
    logMetric("audio_beep", false, 0);
  }

  const uint32_t mountStartedMs = millis();
  mounted = SdDiagnostics::mountCard(mounted, &mountedFrequencyKhz);
  logMetric("sd_mount", mounted, millis() - mountStartedMs);
  Serial.printf("[bench] sd_frequency_khz=%d\n", mountedFrequencyKhz);
  if (mounted) {
    runTimed("sd_write_read", benchmarkSdWriteRead, kSdProbeBytes * 2);
    runTimed("epub_dracula_convert", benchmarkDraculaConversion);
  } else {
    logMetric("sd_write_read", false, 0, kSdProbeBytes * 2);
    logMetric("epub_dracula_convert", false, 0);
  }

  Serial.println("[bench] done");
  showStatus("Benchmark", "Done", "Check serial log");
}

}  // namespace Benchmark
