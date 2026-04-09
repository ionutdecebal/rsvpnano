#include "BoardSupport.h"

#include "BoardConfig.h"

bool BoardSupport::begin() {
  pinMode(BoardConfig::PIN_BOOT_BUTTON, INPUT_PULLUP);

  if (!initDisplay()) {
    return false;
  }

  initTouch();
  return true;
}

void BoardSupport::update() {}

bool BoardSupport::bootPressed() const {
  return digitalRead(BoardConfig::PIN_BOOT_BUTTON) == LOW;
}

TouchEvent BoardSupport::readTouch() {
  TouchEvent e{};
  e.tsMs = millis();
  if (!touchReady_) {
    return e;
  }

  // Minimal CST816-ish read path; if unsupported on your touch IC,
  // replace with board vendor driver while keeping same TouchEvent contract.
  Wire.beginTransmission(BoardConfig::TOUCH_I2C_ADDR);
  Wire.write(0x02); // touch points register
  if (Wire.endTransmission(false) != 0) {
    return e;
  }
  if (Wire.requestFrom((int)BoardConfig::TOUCH_I2C_ADDR, 5) != 5) {
    return e;
  }

  uint8_t points = Wire.read();
  uint8_t xh = Wire.read();
  uint8_t xl = Wire.read();
  uint8_t yh = Wire.read();
  uint8_t yl = Wire.read();

  if ((points & 0x0F) == 0) {
    lastTouch_.pressed = false;
    lastTouch_.valid = true;
    lastTouch_.tsMs = e.tsMs;
    return lastTouch_;
  }

  int16_t x = ((xh & 0x0F) << 8) | xl;
  int16_t y = ((yh & 0x0F) << 8) | yl;
  x = constrain(x, 0, BoardConfig::SCREEN_W - 1);
  y = constrain(y, 0, BoardConfig::SCREEN_H - 1);

  lastTouch_.valid = true;
  lastTouch_.pressed = true;
  lastTouch_.x = x;
  lastTouch_.y = y;
  lastTouch_.tsMs = e.tsMs;
  return lastTouch_;
}

void BoardSupport::setBacklight(uint8_t level) {
  if (!pwmReady_) {
    ledcSetup(pwmChannel_, 20000, 8);
    ledcAttachPin(BoardConfig::PIN_TFT_BL, pwmChannel_);
    pwmReady_ = true;
  }
  ledcWrite(pwmChannel_, level);
}

void BoardSupport::powerDownDisplay() {
  setBacklight(0);
  if (tft_) {
    tft_->fillScreen(ST77XX_BLACK);
  }
}

bool BoardSupport::initDisplay() {
  spi_.begin(BoardConfig::PIN_TFT_SCLK, BoardConfig::PIN_TFT_MISO, BoardConfig::PIN_TFT_MOSI, BoardConfig::PIN_TFT_CS);

  tft_ = new Adafruit_ST7789(&spi_, BoardConfig::PIN_TFT_CS, BoardConfig::PIN_TFT_DC, BoardConfig::PIN_TFT_RST);
  if (!tft_) {
    return false;
  }

  tft_->init(BoardConfig::SCREEN_W, BoardConfig::SCREEN_H);
  tft_->setRotation(1);
  tft_->fillScreen(ST77XX_BLACK);

  setBacklight(220);
  return true;
}

bool BoardSupport::initTouch() {
  Wire.begin(BoardConfig::PIN_TOUCH_SDA, BoardConfig::PIN_TOUCH_SCL, 400000);

  pinMode(BoardConfig::PIN_TOUCH_RST, OUTPUT);
  digitalWrite(BoardConfig::PIN_TOUCH_RST, LOW);
  delay(10);
  digitalWrite(BoardConfig::PIN_TOUCH_RST, HIGH);
  delay(40);

  Wire.beginTransmission(BoardConfig::TOUCH_I2C_ADDR);
  touchReady_ = (Wire.endTransmission() == 0);
  return touchReady_;
}
