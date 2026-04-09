#include <Arduino.h>

#include "app/App.h"

App app;

void setup() {
  Serial.begin(115200);
  app.begin();
}

void loop() {
  const uint32_t now = millis();
  app.update(now);
}
