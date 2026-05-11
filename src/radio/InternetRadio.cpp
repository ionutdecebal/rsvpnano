#include "radio/InternetRadio.h"

#include <Audio.h>
#include <WiFi.h>
#include <esp_log.h>

#include "board/BoardConfig.h"

namespace {
constexpr char kTag[] = "radio";
constexpr uint8_t kVolumeMax = 10;
constexpr uint8_t kVolumeDefault = 5;
constexpr uint32_t kWifiConnectTimeoutMs = 12000;
}  // namespace

/* nyc-area stations + national streams + somafm fallbacks */
const std::vector<InternetRadio::Station> InternetRadio::kFmStations = {
    {88.3f, "NPR News", "https://npr-ice.streamguys1.com/live.mp3", Band::FM},
    {93.9f, "WNYC FM", "https://fm939.wnyc.org/wnycfm", Band::FM},
    {97.1f, "HOT 97", "http://playerservices.streamtheworld.com/api/livestream-redirect/WQHTFMAAC_SC", Band::FM},
    {105.9f, "WQXR", "https://stream.wqxr.org/wqxr", Band::FM},
    {107.5f, "WBLS", "http://playerservices.streamtheworld.com/api/livestream-redirect/WBLSFMAAC_SC", Band::FM},
};

const std::vector<InternetRadio::Station> InternetRadio::kAmStations = {
    {1200.0f, "Groove Salad", "http://ice2.somafm.com/groovesalad-128-mp3", Band::AM},
    {1340.0f, "Indie Pop", "http://ice2.somafm.com/indiepop-128-mp3", Band::AM},
    {1450.0f, "DEF CON", "http://ice2.somafm.com/defcon-128-mp3", Band::AM},
    {1560.0f, "Boot Liquor", "http://ice2.somafm.com/bootliquor-128-mp3", Band::AM},
};

InternetRadio::~InternetRadio() {
  destroyAudio();
}

bool InternetRadio::ensureAudio() {
  if (audio_) {
    return true;
  }

  audio_ = new (std::nothrow) Audio();
  if (!audio_) {
    ESP_LOGE(kTag, "Failed to allocate Audio");
    return false;
  }

  audio_->setPinout(BoardConfig::PIN_AUDIO_BCLK, BoardConfig::PIN_AUDIO_WS,
                    BoardConfig::PIN_AUDIO_DOUT, BoardConfig::PIN_AUDIO_MCLK);
  audio_->setVolume(volume_);
  ESP_LOGI(kTag, "Audio engine created");
  return true;
}

void InternetRadio::destroyAudio() {
  if (!audio_) {
    return;
  }

  audio_->stopSong();
  delete audio_;
  audio_ = nullptr;
  ESP_LOGI(kTag, "Audio engine destroyed");
}

void InternetRadio::stop() {
  destroyAudio();
  disconnectWifi();
  state_ = State::Idle;
  ESP_LOGI(kTag, "Radio stopped");
}

void InternetRadio::update() {
  if (state_ == State::Idle) {
    return;
  }

  /* poll wifi connection progress */
  if (state_ == State::WifiConnecting) {
    if (WiFi.status() == WL_CONNECTED) {
      ESP_LOGI(kTag, "Wi-Fi connected, starting stream");
      connectToStation();
      return;
    }
    if (millis() - wifiConnectStartMs_ >= kWifiConnectTimeoutMs) {
      ESP_LOGW(kTag, "Wi-Fi connect timed out");
      disconnectWifi();
      state_ = State::WifiError;
      return;
    }
    return;
  }

  if (!audio_) {
    return;
  }

  audio_->loop();

  if (state_ == State::Connecting && audio_->isRunning()) {
    state_ = State::Playing;
  }
}

void InternetRadio::play(const String &ssid, const String &password) {
  if (ssid.isEmpty()) {
    state_ = State::NoCredentials;
    ESP_LOGW(kTag, "No Wi-Fi credentials configured");
    return;
  }

  /* already connected from a previous play */
  if (WiFi.status() == WL_CONNECTED) {
    connectToStation();
    return;
  }

  /* start async wifi connection */
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  wifiOwnedByRadio_ = true;
  wifiConnectStartMs_ = millis();
  state_ = State::WifiConnecting;
  ESP_LOGI(kTag, "Connecting to Wi-Fi: %s", ssid.c_str());
}

void InternetRadio::pause() {
  if (audio_) {
    audio_->stopSong();
  }
  state_ = State::Idle;
}

bool InternetRadio::isPlaying() const {
  return state_ == State::Playing || state_ == State::Connecting ||
         state_ == State::Buffering || state_ == State::WifiConnecting;
}

bool InternetRadio::isConnectingWifi() const {
  return state_ == State::WifiConnecting;
}

void InternetRadio::tuneNext() {
  const auto &stations = currentStations();
  if (stations.empty()) {
    return;
  }

  if (band_ == Band::FM) {
    fmStationIndex_ = (fmStationIndex_ + 1) % stations.size();
  } else {
    amStationIndex_ = (amStationIndex_ + 1) % stations.size();
  }

  if (isPlaying()) {
    connectToStation();
  }
}

void InternetRadio::tunePrevious() {
  const auto &stations = currentStations();
  if (stations.empty()) {
    return;
  }

  if (band_ == Band::FM) {
    fmStationIndex_ = (fmStationIndex_ == 0) ? stations.size() - 1 : fmStationIndex_ - 1;
  } else {
    amStationIndex_ = (amStationIndex_ == 0) ? stations.size() - 1 : amStationIndex_ - 1;
  }

  if (isPlaying()) {
    connectToStation();
  }
}

void InternetRadio::toggleBand() {
  band_ = (band_ == Band::FM) ? Band::AM : Band::FM;

  if (isPlaying()) {
    connectToStation();
  }
}

void InternetRadio::setVolume(uint8_t vol) {
  volume_ = (vol > kVolumeMax) ? kVolumeMax : vol;
  if (audio_) {
    audio_->setVolume(volume_);
  }
}

void InternetRadio::adjustVolume(int8_t delta) {
  int newVol = static_cast<int>(volume_) + delta;
  if (newVol < 0) newVol = 0;
  if (newVol > kVolumeMax) newVol = kVolumeMax;
  setVolume(static_cast<uint8_t>(newVol));
}

InternetRadio::Band InternetRadio::band() const { return band_; }

float InternetRadio::frequency() const {
  const auto &stations = currentStations();
  if (stations.empty()) {
    return 0.0f;
  }
  const size_t idx = (band_ == Band::FM) ? fmStationIndex_ : amStationIndex_;
  return stations[idx].frequency;
}

const char *InternetRadio::stationName() const {
  const auto &stations = currentStations();
  if (stations.empty()) {
    return "";
  }
  const size_t idx = (band_ == Band::FM) ? fmStationIndex_ : amStationIndex_;
  return stations[idx].name;
}

uint8_t InternetRadio::volume() const { return volume_; }

InternetRadio::State InternetRadio::state() const { return state_; }

size_t InternetRadio::stationIndex() const {
  return (band_ == Band::FM) ? fmStationIndex_ : amStationIndex_;
}

size_t InternetRadio::stationCount() const {
  return currentStations().size();
}

void InternetRadio::setStationIndex(size_t index) {
  const auto &stations = currentStations();
  if (index >= stations.size()) {
    return;
  }

  if (band_ == Band::FM) {
    fmStationIndex_ = index;
  } else {
    amStationIndex_ = index;
  }

  if (isPlaying()) {
    connectToStation();
  }
}

void InternetRadio::setBand(Band band) {
  band_ = band;
}

uint8_t InternetRadio::savedFmIndex() const {
  return static_cast<uint8_t>(fmStationIndex_);
}

uint8_t InternetRadio::savedAmIndex() const {
  return static_cast<uint8_t>(amStationIndex_);
}

void InternetRadio::restoreTuning(Band band, uint8_t fmIndex, uint8_t amIndex, uint8_t vol) {
  band_ = band;
  fmStationIndex_ = (fmIndex < kFmStations.size()) ? fmIndex : 0;
  amStationIndex_ = (amIndex < kAmStations.size()) ? amIndex : 0;
  volume_ = (vol > kVolumeMax) ? kVolumeDefault : vol;
}

void InternetRadio::connectToStation() {
  const auto &stations = currentStations();
  if (stations.empty()) {
    state_ = State::StreamError;
    return;
  }

  if (!ensureAudio()) {
    state_ = State::StreamError;
    return;
  }

  const size_t idx = (band_ == Band::FM) ? fmStationIndex_ : amStationIndex_;
  const Station &station = stations[idx];

  audio_->stopSong();
  state_ = State::Connecting;
  ESP_LOGI(kTag, "Tuning to %.1f %s: %s", station.frequency, station.name, station.url);

  if (!audio_->connecttohost(station.url)) {
    state_ = State::StreamError;
    ESP_LOGW(kTag, "Failed to connect to stream");
  }
}

void InternetRadio::disconnectWifi() {
  if (!wifiOwnedByRadio_) {
    return;
  }

  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  wifiOwnedByRadio_ = false;
  ESP_LOGI(kTag, "Wi-Fi disconnected");
}

const std::vector<InternetRadio::Station> &InternetRadio::currentStations() const {
  return (band_ == Band::FM) ? kFmStations : kAmStations;
}
