#include <Arduino.h>
#include <esp_log.h>

#include "app/App.h"
#if RSVP_BENCHMARK_MODE
#include "benchmark/BenchmarkRunner.h"
#endif
#include "board/Board.h"

App app;

void setup() {
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);
  delay(50);
  Board::System::begin();
  const uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 2000) {
    delay(10);
  }
  Board::System::logStartupDiagnostics();
#if RSVP_BENCHMARK_MODE
  Serial.println("[main] benchmark setup");
  Benchmark::run();
#else
  Serial.println("[main] app setup");
  app.begin();
#endif
}

void loop() {
#if RSVP_BENCHMARK_MODE
  delay(1000);
#else
  const uint32_t now = millis();
  app.update(now);
#endif
}
