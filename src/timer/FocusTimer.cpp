#include "timer/FocusTimer.h"

#include <math.h>

#include "board/BoardImu.h"

namespace {

constexpr uint8_t kImuWhoAmIReg = 0x00;
constexpr uint8_t kImuCtrl1Reg = 0x02;
constexpr uint8_t kImuCtrl2Reg = 0x03;
constexpr uint8_t kImuCtrl5Reg = 0x06;
constexpr uint8_t kImuCtrl7Reg = 0x08;
constexpr uint8_t kImuCtrl8Reg = 0x09;
constexpr uint8_t kImuAccelStartReg = 0x35;
constexpr uint8_t kImuResetReg = 0x60;
constexpr uint8_t kImuResetValue = 0xB0;
constexpr uint8_t kImuResetResultReg = 0x4D;
constexpr uint8_t kImuResetResultValue = 0x80;
constexpr uint8_t kImuWhoAmIValue = 0x05;

constexpr uint32_t kOrientationStableMs = 700;
constexpr uint32_t kTouchStartArmDelayMs = 350;
constexpr uint32_t kPostTimerFlipGraceMs = 900;
constexpr uint32_t kFeedbackMs = 900;
constexpr uint32_t kTouchDurationMs = 2UL * 60UL * 1000UL;
constexpr uint32_t kWorkDurationMs = 20UL * 60UL * 1000UL;
constexpr uint32_t kBreakDurationMs = 5UL * 60UL * 1000UL;

constexpr float kSideAxisThreshold = 0.78f;
constexpr float kCrossAxisLimit = 0.42f;
constexpr float kFlatAxisThreshold = 0.84f;

}  // namespace

bool FocusTimer::begin() { return initImu(); }

void FocusTimer::open() {
  if (!imuAvailable_) {
    initImu();
  }

  clearSession();
  resetOrientationStability();
  state_ = imuAvailable_ ? State::GenreSelect : State::Unavailable;
  stateStartedMs_ = millis();
}

void FocusTimer::update(uint32_t nowMs) {
  if (imuAvailable_) {
    updateOrientation(nowMs);
  }

  switch (state_) {
    case State::Unavailable:
    case State::GenreSelect:
      break;

    case State::WaitForTouchStart:
      if (orientationInputArmed(nowMs) && isShortSide(stableOrientation_)) {
        startMode(TimerMode::Touch, nowMs, kTouchDurationMs, stableOrientation_);
        transitionTo(State::TouchRunning, nowMs);
      }
      break;

    case State::TouchRunning:
      if (timerExpired(nowMs)) {
        completeActiveTimer();
        resetOrientationStability();
        transitionTo(State::WaitAfterTouch, nowMs);
      }
      break;

    case State::WaitAfterTouch:
      if (!orientationInputArmed(nowMs)) {
        break;
      }
      if (stableOrientation_ == oppositeShortSide(lastShortSide_)) {
        startMode(TimerMode::Work, nowMs, kWorkDurationMs, stableOrientation_);
        transitionTo(State::WorkRunning, nowMs);
      } else if (stableOrientation_ == OrientationState::LongSide) {
        startMode(TimerMode::Break, nowMs, kBreakDurationMs, OrientationState::LongSide);
        transitionTo(State::BreakRunning, nowMs);
      }
      break;

    case State::WorkRunning:
      if (timerExpired(nowMs)) {
        completeActiveTimer();
        resetOrientationStability();
        transitionTo(State::WaitAfterWork, nowMs);
      }
      break;

    case State::WaitAfterWork:
      if (!orientationInputArmed(nowMs)) {
        break;
      }
      if (stableOrientation_ == oppositeShortSide(lastShortSide_)) {
        startMode(TimerMode::Work, nowMs, kWorkDurationMs, stableOrientation_);
        transitionTo(State::WorkRunning, nowMs);
      } else if (stableOrientation_ == OrientationState::LongSide) {
        startMode(TimerMode::Break, nowMs, kBreakDurationMs, OrientationState::LongSide);
        transitionTo(State::BreakRunning, nowMs);
      }
      break;

    case State::BreakRunning:
      if (timerExpired(nowMs)) {
        completeActiveTimer();
        resetOrientationStability();
        transitionTo(State::WaitAfterBreak, nowMs);
      }
      break;

    case State::WaitAfterBreak:
      if (orientationInputArmed(nowMs) && isShortSide(stableOrientation_)) {
        startMode(TimerMode::Work, nowMs, kWorkDurationMs, stableOrientation_);
        transitionTo(State::WorkRunning, nowMs);
      }
      break;

    case State::Cancelled:
      if (nowMs - feedbackStartedMs_ >= kFeedbackMs) {
        resetOrientationStability();
        transitionTo(State::WaitForTouchStart, nowMs);
      }
      break;

    case State::Complete:
      if (nowMs - feedbackStartedMs_ >= kFeedbackMs) {
        clearSession();
        resetOrientationStability();
        transitionTo(imuAvailable_ ? State::GenreSelect : State::Unavailable, nowMs);
      }
      break;
  }
}

void FocusTimer::chooseGenre(Genre genre, uint32_t nowMs) {
  if (genre == Genre::None) {
    return;
  }

  clearSession();
  genre_ = genre;
  resetOrientationStability();
  transitionTo(State::WaitForTouchStart, nowMs);
}

void FocusTimer::cancelActiveTimer(uint32_t nowMs) {
  if (!timerRunning_) {
    return;
  }

  stopActiveTimer();
  resetOrientationStability();
  feedbackStartedMs_ = nowMs;
  transitionTo(State::Cancelled, nowMs);
}

void FocusTimer::abandon() {
  clearSession();
  resetOrientationStability();
  state_ = imuAvailable_ ? State::GenreSelect : State::Unavailable;
  stateStartedMs_ = millis();
}

bool FocusTimer::available() const { return imuAvailable_; }

bool FocusTimer::isActiveTimerRunning() const { return timerRunning_; }

FocusTimer::State FocusTimer::state() const { return state_; }

FocusTimer::Genre FocusTimer::genre() const { return genre_; }

Board::UiOrientation FocusTimer::uiOrientation() const {
  switch (state_) {
    case State::GenreSelect:
    case State::Unavailable:
    case State::Complete:
      return Board::UiOrientation::Landscape;

    case State::WaitForTouchStart:
    case State::TouchRunning:
    case State::Cancelled:
      return portraitOrientationForShortSide(activeStartOrientation_);

    case State::WaitAfterTouch:
    case State::WorkRunning:
    case State::WaitAfterBreak:
      return portraitOrientationForShortSide(lastShortSide_);

    case State::BreakRunning:
    case State::WaitAfterWork:
      return Board::UiOrientation::Landscape;

    default:
      return Board::UiOrientation::Portrait;
  }
}

uint32_t FocusTimer::remainingMs(uint32_t nowMs) const {
  if (!timerRunning_) {
    return 0;
  }

  const uint32_t elapsed = nowMs - timerStartedMs_;
  return (elapsed >= timerDurationMs_) ? 0 : (timerDurationMs_ - elapsed);
}

uint8_t FocusTimer::progressPercent(uint32_t nowMs) const {
  if (!timerRunning_ || timerDurationMs_ == 0) {
    return 0;
  }

  const uint32_t elapsed = nowMs - timerStartedMs_;
  const uint32_t clamped = (elapsed >= timerDurationMs_) ? timerDurationMs_ : elapsed;
  return static_cast<uint8_t>((clamped * 100U) / timerDurationMs_);
}

uint8_t FocusTimer::completedTouchBlocks() const { return completedTouchBlocks_; }

uint8_t FocusTimer::completedWorkBlocks() const { return completedWorkBlocks_; }

uint8_t FocusTimer::completedBreakBlocks() const { return completedBreakBlocks_; }

bool FocusTimer::consumeCompletionCue() {
  const bool pending = completionCuePending_;
  completionCuePending_ = false;
  return pending;
}

const char *FocusTimer::genreLabel(Genre genre) {
  switch (genre) {
    case Genre::Chores:
      return "Chores";
    case Genre::RsvpNano:
      return "Work";
    case Genre::StrengthLabs:
      return "Fitness";
    case Genre::SelfCare:
      return "Self Care";
    case Genre::Other:
      return "Other";
    case Genre::None:
    default:
      return "";
  }
}

bool FocusTimer::initImu() {
  if (!Board::Imu::available()) {
    imuAvailable_ = false;
    Serial.println("[timer] IMU unavailable for this board profile");
    return false;
  }

  if (imuAvailable_) {
    return true;
  }

  const uint8_t candidateAddresses[] = {
      Board::Imu::address(),
      0x6B,
      0x6A,
  };
  bool sawRespondingAddress = false;

  for (uint8_t i = 0; i < sizeof(candidateAddresses); ++i) {
    const uint8_t candidateAddress = candidateAddresses[i];
    bool alreadyTried = false;
    for (uint8_t j = 0; j < i; ++j) {
      if (candidateAddresses[j] == candidateAddress) {
        alreadyTried = true;
        break;
      }
    }
    if (alreadyTried) {
      continue;
    }

    if (!Board::Imu::probeAddress(candidateAddress)) {
      continue;
    }
    sawRespondingAddress = true;
    imuAddress_ = candidateAddress;

    uint8_t whoAmI = 0;
    if (!Board::Imu::readRegister(imuAddress_, kImuWhoAmIReg, whoAmI) ||
        whoAmI != kImuWhoAmIValue) {
      Serial.printf("[timer] QMI8658 WHOAMI mismatch addr=0x%02X got=0x%02X expected=0x%02X\n",
                    candidateAddress, whoAmI, kImuWhoAmIValue);
      continue;
    }

    if (!Board::Imu::writeRegister(imuAddress_, kImuResetReg, kImuResetValue)) {
      Serial.printf("[timer] QMI8658 reset command failed addr=0x%02X\n", candidateAddress);
      continue;
    }

    const uint32_t waitStartedMs = millis();
    uint8_t resetResult = 0;
    bool resetReady = false;
    while (millis() - waitStartedMs < 500) {
      if (Board::Imu::readRegister(imuAddress_, kImuResetResultReg, resetResult) &&
          resetResult == kImuResetResultValue) {
        resetReady = true;
        break;
      }
      delay(10);
    }

    if (!resetReady) {
      Serial.printf("[timer] QMI8658 reset timeout addr=0x%02X last=0x%02X\n",
                    candidateAddress, resetResult);
      continue;
    }

    whoAmI = 0;
    if (!Board::Imu::readRegister(imuAddress_, kImuWhoAmIReg, whoAmI) ||
        whoAmI != kImuWhoAmIValue) {
      Serial.printf("[timer] QMI8658 WHOAMI mismatch after reset addr=0x%02X got=0x%02X "
                    "expected=0x%02X\n",
                    candidateAddress, whoAmI, kImuWhoAmIValue);
      continue;
    }

    if (!updateRegister(kImuCtrl1Reg, 0x40, 0x40) ||
        !Board::Imu::writeRegister(imuAddress_, kImuCtrl8Reg, 0x80) ||
        !Board::Imu::writeRegister(imuAddress_, kImuCtrl2Reg, 0x16) ||
        !updateRegister(kImuCtrl5Reg, 0x07, 0x07) ||
        !updateRegister(kImuCtrl7Reg, 0x01, 0x01)) {
      Serial.printf("[timer] QMI8658 configuration failed addr=0x%02X\n", candidateAddress);
      continue;
    }

    accelScale_ = 4.0f / 32768.0f;
    resetOrientationStability();
    imuAvailable_ = true;
    Serial.printf("[timer] QMI8658 initialized addr=0x%02X bus=%s\n", imuAddress_,
                  Board::Imu::wireName());
    return true;
  }

  imuAvailable_ = false;
  if (sawRespondingAddress) {
    Serial.printf("[timer] QMI8658 init failed bus=%s configured=0x%02X\n",
                  Board::Imu::wireName(), Board::Imu::address());
  } else {
    Serial.printf("[timer] QMI8658 not responding bus=%s configured=0x%02X fallback=0x6A\n",
                  Board::Imu::wireName(), Board::Imu::address());
  }
  return false;
}

bool FocusTimer::updateRegister(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current = 0;
  if (!Board::Imu::readRegister(imuAddress_, reg, current)) {
    return false;
  }

  current = static_cast<uint8_t>((current & static_cast<uint8_t>(~mask)) | (value & mask));
  return Board::Imu::writeRegister(imuAddress_, reg, current);
}

bool FocusTimer::readAccelerometer(float &x, float &y, float &z) {
  uint8_t buffer[6] = {0};
  if (!Board::Imu::readRegisters(imuAddress_, kImuAccelStartReg, buffer, sizeof(buffer))) {
    return false;
  }

  const int16_t rawX = static_cast<int16_t>((buffer[1] << 8) | buffer[0]);
  const int16_t rawY = static_cast<int16_t>((buffer[3] << 8) | buffer[2]);
  const int16_t rawZ = static_cast<int16_t>((buffer[5] << 8) | buffer[4]);

  x = rawX * accelScale_;
  y = rawY * accelScale_;
  z = rawZ * accelScale_;
  return true;
}

void FocusTimer::updateOrientation(uint32_t nowMs) {
  if (!imuAvailable_) {
    rawOrientation_ = OrientationState::Unknown;
    stableOrientation_ = OrientationState::Unknown;
    return;
  }

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  if (!readAccelerometer(x, y, z)) {
    return;
  }

  rawOrientation_ = classify(x, y, z);
  if (rawOrientation_ != candidateOrientation_) {
    candidateOrientation_ = rawOrientation_;
    candidateSinceMs_ = nowMs;
    return;
  }

  if ((nowMs - candidateSinceMs_) >= kOrientationStableMs) {
    stableOrientation_ = candidateOrientation_;
  }
}

void FocusTimer::resetOrientationStability() {
  rawOrientation_ = OrientationState::Unknown;
  stableOrientation_ = OrientationState::Unknown;
  candidateOrientation_ = OrientationState::Unknown;
  candidateSinceMs_ = 0;
}

FocusTimer::OrientationState FocusTimer::classify(float x, float y, float z) const {
  if (fabsf(z) >= kFlatAxisThreshold && fabsf(x) <= 0.30f && fabsf(y) <= 0.30f) {
    return OrientationState::FlatBack;
  }

  if (x >= kSideAxisThreshold && fabsf(y) <= kCrossAxisLimit &&
      fabsf(z) <= kCrossAxisLimit) {
    return OrientationState::ShortSideA;
  }

  if (x <= -kSideAxisThreshold && fabsf(y) <= kCrossAxisLimit &&
      fabsf(z) <= kCrossAxisLimit) {
    return OrientationState::ShortSideB;
  }

  if (fabsf(y) >= kSideAxisThreshold && fabsf(x) <= kCrossAxisLimit &&
      fabsf(z) <= kCrossAxisLimit) {
    return OrientationState::LongSide;
  }

  return OrientationState::Unknown;
}

bool FocusTimer::orientationInputArmed(uint32_t nowMs) const {
  switch (state_) {
    case State::WaitForTouchStart:
      return (nowMs - stateStartedMs_) >= kTouchStartArmDelayMs;
    case State::WaitAfterTouch:
    case State::WaitAfterWork:
    case State::WaitAfterBreak:
      return (nowMs - stateStartedMs_) >= kPostTimerFlipGraceMs;
    default:
      return true;
  }
}

void FocusTimer::transitionTo(State nextState, uint32_t nowMs) {
  state_ = nextState;
  stateStartedMs_ = nowMs;
}

void FocusTimer::clearSession() {
  genre_ = Genre::None;
  activeMode_ = TimerMode::None;
  activeStartOrientation_ = OrientationState::Unknown;
  lastShortSide_ = OrientationState::Unknown;
  timerStartedMs_ = 0;
  timerDurationMs_ = 0;
  timerRunning_ = false;
  feedbackStartedMs_ = 0;
  completionCuePending_ = false;
  completedTouchBlocks_ = 0;
  completedWorkBlocks_ = 0;
  completedBreakBlocks_ = 0;
}

void FocusTimer::startMode(TimerMode mode, uint32_t nowMs, uint32_t durationMs,
                           OrientationState startOrientation) {
  activeMode_ = mode;
  activeStartOrientation_ = startOrientation;
  timerStartedMs_ = nowMs;
  timerDurationMs_ = durationMs;
  timerRunning_ = true;

  if (isShortSide(startOrientation)) {
    lastShortSide_ = startOrientation;
  }
}

void FocusTimer::stopActiveTimer() {
  timerRunning_ = false;
  activeMode_ = TimerMode::None;
  activeStartOrientation_ = OrientationState::Unknown;
  timerStartedMs_ = 0;
  timerDurationMs_ = 0;
  lastShortSide_ = OrientationState::Unknown;
}

void FocusTimer::completeActiveTimer() {
  if (!timerRunning_) {
    return;
  }

  switch (activeMode_) {
    case TimerMode::Touch:
      ++completedTouchBlocks_;
      break;
    case TimerMode::Work:
      ++completedWorkBlocks_;
      break;
    case TimerMode::Break:
      ++completedBreakBlocks_;
      break;
    case TimerMode::None:
    default:
      break;
  }

  timerRunning_ = false;
  activeMode_ = TimerMode::None;
  activeStartOrientation_ = OrientationState::Unknown;
  timerStartedMs_ = 0;
  timerDurationMs_ = 0;
  completionCuePending_ = true;
}

bool FocusTimer::timerExpired(uint32_t nowMs) const {
  return timerRunning_ && (nowMs - timerStartedMs_ >= timerDurationMs_);
}

bool FocusTimer::isShortSide(OrientationState orientation) {
  return orientation == OrientationState::ShortSideA ||
         orientation == OrientationState::ShortSideB;
}

FocusTimer::OrientationState FocusTimer::oppositeShortSide(
    OrientationState orientation) {
  switch (orientation) {
    case OrientationState::ShortSideA:
      return OrientationState::ShortSideB;
    case OrientationState::ShortSideB:
      return OrientationState::ShortSideA;
    default:
      return OrientationState::Unknown;
  }
}

Board::UiOrientation FocusTimer::portraitOrientationForShortSide(OrientationState orientation) {
  return orientation == OrientationState::ShortSideB ? Board::UiOrientation::PortraitFlipped
                                                     : Board::UiOrientation::Portrait;
}
