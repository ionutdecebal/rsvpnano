#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "board/BoardConfig.h"

class FocusTimer {
 public:
  enum class Genre : uint8_t {
    Chores = 0,
    RsvpNano,
    StrengthLabs,
    SelfCare,
    Other,
    None = 0xFF,
  };

  enum class State : uint8_t {
    Unavailable = 0,
    GenreSelect,
    WaitForTouchStart,
    TouchRunning,
    WaitAfterTouch,
    WorkRunning,
    BreakRunning,
    WaitAfterWork,
    WaitAfterBreak,
    Cancelled,
    Complete,
  };

  bool begin();
  void open();
  void update(uint32_t nowMs);
  void chooseGenre(Genre genre, uint32_t nowMs);
  void cancelActiveTimer(uint32_t nowMs);
  void abandon();

  bool available() const;
  bool isActiveTimerRunning() const;
  State state() const;
  Genre genre() const;
  BoardConfig::UiOrientation uiOrientation() const;
  uint32_t remainingMs(uint32_t nowMs) const;
  uint8_t progressPercent(uint32_t nowMs) const;
  uint8_t completedTouchBlocks() const;
  uint8_t completedWorkBlocks() const;
  uint8_t completedBreakBlocks() const;
  bool consumeCompletionCue();

  static const char *genreLabel(Genre genre);

 private:
  enum class TimerMode : uint8_t {
    None = 0,
    Touch,
    Work,
    Break,
  };

  enum class OrientationState : uint8_t {
    ShortSideA = 0,
    ShortSideB,
    LongSide,
    FlatBack,
    Unknown,
  };

  bool initImu();
  bool readRegister(uint8_t reg, uint8_t &value);
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegisters(uint8_t startReg, uint8_t *buffer, size_t len);
  bool updateRegister(uint8_t reg, uint8_t mask, uint8_t value);
  bool readAccelerometer(float &x, float &y, float &z);
  void updateOrientation(uint32_t nowMs);
  void resetOrientationStability();
  OrientationState classify(float x, float y, float z) const;
  bool orientationInputArmed(uint32_t nowMs) const;
  void transitionTo(State nextState, uint32_t nowMs);
  void clearSession();
  void startMode(TimerMode mode, uint32_t nowMs, uint32_t durationMs,
                 OrientationState startOrientation);
  void stopActiveTimer();
  void completeActiveTimer();
  bool timerExpired(uint32_t nowMs) const;
  static bool isShortSide(OrientationState orientation);
  static OrientationState oppositeShortSide(OrientationState orientation);
  static BoardConfig::UiOrientation portraitOrientationForShortSide(
      OrientationState orientation);

  bool imuAvailable_ = false;
  float accelScale_ = 4.0f / 32768.0f;
  OrientationState rawOrientation_ = OrientationState::Unknown;
  OrientationState stableOrientation_ = OrientationState::Unknown;
  OrientationState candidateOrientation_ = OrientationState::Unknown;
  OrientationState activeStartOrientation_ = OrientationState::Unknown;
  OrientationState lastShortSide_ = OrientationState::Unknown;
  uint32_t candidateSinceMs_ = 0;

  State state_ = State::Unavailable;
  Genre genre_ = Genre::None;
  TimerMode activeMode_ = TimerMode::None;
  uint32_t stateStartedMs_ = 0;
  uint32_t feedbackStartedMs_ = 0;
  uint32_t timerStartedMs_ = 0;
  uint32_t timerDurationMs_ = 0;
  bool timerRunning_ = false;
  bool completionCuePending_ = false;
  uint8_t completedTouchBlocks_ = 0;
  uint8_t completedWorkBlocks_ = 0;
  uint8_t completedBreakBlocks_ = 0;
};
