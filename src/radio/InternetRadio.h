#pragma once

#include <Arduino.h>
#include <vector>

#include "board/BoardConfig.h"

class Audio;

class InternetRadio {
 public:
  enum class Band : uint8_t {
    FM = 0,
    AM = 1,
  };

  enum class State : uint8_t {
    Idle,
    WifiConnecting,
    Connecting,
    Playing,
    Buffering,
    WifiError,
    NoCredentials,
    StreamError,
  };

  struct Station {
    float frequency;
    const char *name;
    const char *url;
    Band band;
  };

  ~InternetRadio();

  void stop();
  void update();

  /* call with saved credentials; kicks off wifi + stream connection */
  void play(const String &ssid, const String &password);
  void pause();
  bool isPlaying() const;
  bool isConnectingWifi() const;

  void tuneNext();
  void tunePrevious();
  void toggleBand();
  void setVolume(uint8_t volume);
  void adjustVolume(int8_t delta);

  Band band() const;
  float frequency() const;
  const char *stationName() const;
  uint8_t volume() const;
  State state() const;
  size_t stationIndex() const;
  size_t stationCount() const;

  void setStationIndex(size_t index);
  void setBand(Band band);

  /* saved tuning state for preferences */
  uint8_t savedFmIndex() const;
  uint8_t savedAmIndex() const;
  void restoreTuning(Band band, uint8_t fmIndex, uint8_t amIndex, uint8_t vol);

 private:
  bool ensureAudio();
  void destroyAudio();
  void connectToStation();
  void disconnectWifi();
  const std::vector<Station> &currentStations() const;

  /* heap-allocated only when radio is active to avoid I2S conflict at boot */
  Audio *audio_ = nullptr;
  Band band_ = Band::FM;
  size_t fmStationIndex_ = 0;
  size_t amStationIndex_ = 0;
  uint8_t volume_ = 12;
  State state_ = State::Idle;
  bool wifiOwnedByRadio_ = false;
  uint32_t wifiConnectStartMs_ = 0;

  static const std::vector<Station> kFmStations;
  static const std::vector<Station> kAmStations;
};
