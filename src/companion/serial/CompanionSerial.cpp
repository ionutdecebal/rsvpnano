#include "companion/serial/CompanionSerial.h"

#include <glaze/json.hpp>

#include <WiFi.h>

#include <algorithm>
#include <array>
#include <span>
#include <string_view>
#include <utility>

#include "companion/http/CompanionApi.h"
#include "companion/CompanionApiModels.h"
#include "companion/serial/CompanionBufferedRequest.h"
#include "board/BoardStorage.h"
#include "usb/UsbMassStorageManager.h"

namespace {

    constexpr std::string_view kHandshake = "RSVPNANO/COMPANION/1\n";
    constexpr uint32_t kSessionTimeoutMs = 15'000;
    constexpr size_t kMaximumJsonRequestBytes = 8 * 1024;
    constexpr uint64_t kMaximumRequestBytes = 256ULL * 1024ULL * 1024ULL;
    constexpr std::string_view kSpoolPath = "/.companion-usb-request.tmp";

    [[nodiscard]] bool resourcePath(std::string_view path, std::string_view prefix,
                                    std::string_view suffix = {}) {
        if (!path.starts_with(prefix) || !path.ends_with(suffix)
            || path.size() <= prefix.size() + suffix.size()) {
            return false;
        }
        return !path.substr(prefix.size(), path.size() - prefix.size() - suffix.size()).contains('/');
    }

    struct ErrorBody {
        std::string code;
        std::string message;
        std::optional<std::string> field;
    };

    struct MemoryReader {
        std::span<const uint8_t> bytes;
        size_t offset = 0;
    };

    int readMemory(void* context, std::span<uint8_t> destination) {
        auto& reader = *static_cast<MemoryReader*>(context);
        const size_t count = std::min(destination.size(), reader.bytes.size() - reader.offset);
        std::ranges::copy(reader.bytes.subspan(reader.offset, count), destination.begin());
        reader.offset += count;
        return static_cast<int>(count);
    }

    int readFile(void* context, std::span<uint8_t> destination) {
        auto& file = *static_cast<File*>(context);
        return static_cast<int>(file.read(destination.data(), destination.size()));
    }

    void updateCrc(uint32_t& crc, std::span<const uint8_t> bytes) {
        for (const uint8_t value: bytes) {
            crc ^= value;
            for (uint8_t bit = 0; bit < 8; ++bit)
                crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }

} // namespace

void CompanionSerial::update(uint32_t nowMs) {
    if (!active_) {
        readHandshake(nowMs);
        updateImprov(nowMs);
        return;
    }

    readFrames(nowMs);
    if (nowMs - lastTrafficMs_ >= kSessionTimeoutMs)
        close();
}

void CompanionSerial::close() {
    active_ = false;
    decoder_.clear();
    handshake_.clear();
    resetRequest();
    responseBody_.clear();
    responseRequestId_ = 0;
    responseSequence_ = 0;
    responseOffset_ = 0;
    Serial.setDebugOutput(true);
}

bool CompanionSerial::active() const noexcept {
    return active_;
}

void CompanionSerial::readHandshake(uint32_t nowMs) {
    while (Serial.available() > 0) {
        const uint8_t byte = static_cast<uint8_t>(Serial.read());
        readImprovByte(byte, nowMs);
        const char value = static_cast<char>(byte);
        handshake_.push_back(value);
        if (handshake_.size() > kHandshake.size())
            handshake_.erase(handshake_.begin());
        if (handshake_ != kHandshake)
            continue;

        handshake_.clear();
        if (massStorage_.active()) {
            Serial.print("RSVPNANO/COMPANION/1 BUSY MSC\n");
            return;
        }

        Serial.setDebugOutput(false);
        Serial.print("RSVPNANO/COMPANION/1 READY\n");
        Serial.flush();
        decoder_.clear();
        resetRequest();
        active_ = true;
        lastTrafficMs_ = nowMs;
        return;
    }
}

void CompanionSerial::updateImprov(uint32_t nowMs) {
    if (!improvBuffer_.empty() && nowMs - improvLastByteMs_ > 100)
        improvBuffer_.clear();
    if (improvState_ != improv::STATE_PROVISIONING)
        return;

    if (WiFi.status() == WL_CONNECTED) {
        auto stored = api_.updateNetwork({.ssid = provisioningSsid_, .password = provisioningPassword_});
        provisioningSsid_.clear();
        provisioningPassword_.clear();
        provisioningDeadlineMs_ = 0;
        if (!stored) {
            sendImprovError(improv::ERROR_UNKNOWN);
            improvState_ = improv::STATE_AUTHORIZED;
            sendImprovState(improvState_);
            return;
        }
        improvState_ = improv::STATE_PROVISIONED;
        sendImprovState(improvState_);
        sendImprovResponse(improv::WIFI_SETTINGS,
                           {std::string{"http://"} + WiFi.localIP().toString().c_str()});
        return;
    }

    if (static_cast<int32_t>(nowMs - provisioningDeadlineMs_) >= 0) {
        provisioningSsid_.clear();
        provisioningPassword_.clear();
        provisioningDeadlineMs_ = 0;
        sendImprovError(improv::ERROR_UNABLE_TO_CONNECT);
        improvState_ = improv::STATE_AUTHORIZED;
        sendImprovState(improvState_);
    }
}

void CompanionSerial::readImprovByte(uint8_t byte, uint32_t nowMs) {
    if (improvBuffer_.empty() && byte != 'I')
        return;

    const size_t position = improvBuffer_.size();
    improvBuffer_.push_back(byte);
    bool completed = false;
    const bool valid = improv::parse_improv_serial_byte(
        position, byte, improvBuffer_.data(),
        [this, nowMs, &completed](improv::ImprovCommand command) {
            completed = true;
            return handleImprovCommand(std::move(command), nowMs);
        },
        [this](improv::Error error) { sendImprovError(error); });
    improvLastByteMs_ = nowMs;
    if (!valid || completed)
        improvBuffer_.clear();
}

bool CompanionSerial::handleImprovCommand(improv::ImprovCommand command, uint32_t nowMs) {
    switch (command.command) {
    case improv::WIFI_SETTINGS:
        if (massStorage_.active()) {
            sendImprovError(improv::ERROR_UNABLE_TO_CONNECT);
            return true;
        }
        provisioningSsid_ = std::move(command.ssid);
        provisioningPassword_ = std::move(command.password);
        WiFi.persistent(false);
        WiFi.setAutoReconnect(false);
        if (!WiFi.mode(api_.active() ? WIFI_AP_STA : WIFI_STA)
            || WiFi.begin(provisioningSsid_.c_str(), provisioningPassword_.c_str()) == WL_CONNECT_FAILED) {
            sendImprovError(improv::ERROR_UNABLE_TO_CONNECT);
            provisioningSsid_.clear();
            provisioningPassword_.clear();
            return true;
        }
        improvState_ = improv::STATE_PROVISIONING;
        provisioningDeadlineMs_ = nowMs + 30'000;
        sendImprovState(improvState_);
        return true;
    case improv::GET_CURRENT_STATE:
        improvState_ = WiFi.status() == WL_CONNECTED ? improv::STATE_PROVISIONED : improv::STATE_AUTHORIZED;
        sendImprovState(improvState_);
        if (improvState_ == improv::STATE_PROVISIONED) {
            sendImprovResponse(improv::GET_CURRENT_STATE,
                               {std::string{"http://"} + WiFi.localIP().toString().c_str()});
        }
        return true;
    case improv::GET_DEVICE_INFO: {
        const auto info = api_.deviceInfo();
        sendImprovResponse(improv::GET_DEVICE_INFO,
                           {"RSVP Nano", info.firmwareVersion, ESP.getChipModel(), info.ssid});
        return true;
    }
    default:
        sendImprovError(improv::ERROR_UNKNOWN_RPC);
        return false;
    }
}

void CompanionSerial::sendImprov(uint8_t type, std::span<const uint8_t> data) {
    std::vector<uint8_t> frame{'I', 'M', 'P', 'R', 'O', 'V', improv::IMPROV_SERIAL_VERSION,
                               type, static_cast<uint8_t>(data.size())};
    frame.insert(frame.end(), data.begin(), data.end());
    uint8_t checksum = 0;
    for (const uint8_t byte: frame)
        checksum = static_cast<uint8_t>(checksum + byte);
    frame.push_back(checksum);
    frame.push_back('\n');
    Serial.write(frame.data(), frame.size());
}

void CompanionSerial::sendImprovState(improv::State state) {
    const uint8_t value = static_cast<uint8_t>(state);
    sendImprov(improv::TYPE_CURRENT_STATE, std::span{&value, 1});
}

void CompanionSerial::sendImprovError(improv::Error error) {
    const uint8_t value = static_cast<uint8_t>(error);
    sendImprov(improv::TYPE_ERROR_STATE, std::span{&value, 1});
}

void CompanionSerial::sendImprovResponse(improv::Command command, const std::vector<std::string>& values) {
    const auto data = improv::build_rpc_response(command, values, false);
    sendImprov(improv::TYPE_RPC_RESPONSE, data);
}

void CompanionSerial::readFrames(uint32_t nowMs) {
    std::array<uint8_t, 512> bytes{};
    while (Serial.available() > 0) {
        const size_t count = Serial.readBytes(bytes.data(), std::min<size_t>(Serial.available(), bytes.size()));
        if (count == 0)
            break;
        lastTrafficMs_ = nowMs;
        decoder_.append(std::span{bytes}.first(count));
    }
    for (auto& frame: decoder_.takeFrames())
        handleFrame(std::move(frame), nowMs);
}

void CompanionSerial::handleFrame(companion::serial::Frame frame, uint32_t nowMs) {
    using companion::serial::FrameType;
    lastTrafficMs_ = nowMs;
    switch (frame.type) {
    case FrameType::Ping:
        sendFrame({.type = FrameType::Pong});
        return;
    case FrameType::Pong:
        return;
    case FrameType::Acknowledgement:
        if (frame.requestId == responseRequestId_ && frame.sequence == responseSequence_) {
            ++responseSequence_;
            sendNextResponseChunk();
        }
        return;
    case FrameType::Close:
        close();
        return;
    case FrameType::Request: {
        if (requestId_ != 0) {
            sendProtocolError(frame.requestId, "Only one USB request may run at a time");
            return;
        }
        const std::string_view json{reinterpret_cast<const char*>(frame.payload.data()), frame.payload.size()};
        auto metadata = companion::api::decode<RequestMetadata>(json);
        if (!metadata || metadata->totalBytes > kMaximumRequestBytes) {
            sendProtocolError(frame.requestId, "USB request metadata is invalid");
            return;
        }
        request_ = std::move(*metadata);
        requestId_ = frame.requestId;
        expectedSequence_ = 0;
        receivedBytes_ = 0;
        requestCrc_ = 0xFFFFFFFFU;
        requestBody_.clear();
        requestSpooled_ = request_.totalBytes > kMaximumJsonRequestBytes;
        if (requestSpooled_) {
            auto& filesystem = Board::Storage::filesystem();
            if (filesystem.exists(kSpoolPath.data()))
                filesystem.remove(kSpoolPath.data());
            requestFile_ = filesystem.open(kSpoolPath.data(), FILE_WRITE);
            if (!requestFile_) {
                sendProtocolError(frame.requestId, "The USB upload temporary file could not be created");
                resetRequest();
                return;
            }
        } else {
            requestBody_.reserve(static_cast<size_t>(request_.totalBytes));
        }
        return;
    }
    case FrameType::Data:
        if (frame.requestId != requestId_ || frame.sequence != expectedSequence_
            || receivedBytes_ + frame.payload.size() > request_.totalBytes) {
            sendProtocolError(frame.requestId, "USB data frames arrived out of sequence");
            resetRequest();
            return;
        }
        if (requestSpooled_) {
            if (!requestFile_ || requestFile_.write(frame.payload.data(), frame.payload.size()) != frame.payload.size()) {
                sendProtocolError(frame.requestId, "The USB upload could not be written");
                resetRequest();
                return;
            }
        } else {
            requestBody_.insert(requestBody_.end(), frame.payload.begin(), frame.payload.end());
        }
        updateCrc(requestCrc_, frame.payload);
        receivedBytes_ += frame.payload.size();
        sendFrame({.type = FrameType::Acknowledgement, .requestId = requestId_, .sequence = expectedSequence_});
        ++expectedSequence_;
        return;
    case FrameType::End:
        handleRequestEnd(frame);
        return;
    case FrameType::Response:
    case FrameType::Error:
        sendProtocolError(frame.requestId, "Unexpected USB companion frame");
        return;
    }
}

void CompanionSerial::handleRequestEnd(const companion::serial::Frame& frame) {
    if (frame.requestId != requestId_) {
        sendProtocolError(frame.requestId, "USB request ID is not active");
        return;
    }
    const std::string_view json{reinterpret_cast<const char*>(frame.payload.data()), frame.payload.size()};
    auto end = companion::api::decode<TransferEnd>(json);
    if (!end || end->totalBytes != receivedBytes_ || end->totalBytes != request_.totalBytes
        || end->crc32 != ~requestCrc_) {
        sendError(requestId_, 400, "upload_interrupted", "USB request size or checksum did not match");
        resetRequest();
        return;
    }

    if (requestFile_) {
        requestFile_.flush();
        requestFile_.close();
    }

    MemoryReader memory{requestBody_};
    companion::BufferedRequest buffered{
        .query = request_.query,
        .read = requestSpooled_ ? &readFile : &readMemory,
        .readContext = requestSpooled_ ? static_cast<void*>(&requestFile_) : static_cast<void*>(&memory),
    };
    if (requestSpooled_)
        requestFile_ = Board::Storage::filesystem().open(kSpoolPath.data(), FILE_READ);

    dispatchRequest(buffered);
    resetRequest();
}

void CompanionSerial::dispatchRequest(companion::BufferedRequest& buffered) {
    namespace api = companion::api;

    httpd_req_t request{};
    request.handle = nullptr;
    request.content_len = static_cast<size_t>(request_.totalBytes);
    request.user_ctx = &buffered;
    buffered.path = request_.path;

    const auto failed = [this](api::HttpError error) {
        sendError(requestId_, error.status, std::move(error.error.code), std::move(error.error.message),
                  std::move(error.error.field));
    };
    const auto sendValue = [this, &failed]<typename T>(api::Result<T> result, int status = 200) {
        if (!result) {
            failed(std::move(result.error()));
            return;
        }
        auto json = api_.encodeResponse(*result);
        if (!json) {
            failed(std::move(json.error()));
            return;
        }
        sendResponse(requestId_, status, std::move(*json));
    };
    const auto sendLocated = [this, &failed]<typename T>(api::Result<api::Located<T>> result) {
        if (!result) {
            failed(std::move(result.error()));
            return;
        }
        auto json = api_.encodeResponse(result->value.get());
        if (!json) {
            failed(std::move(json.error()));
            return;
        }
        sendResponse(requestId_, 201, std::move(*json));
    };
    const auto sendEmpty = [this, &failed](api::Result<> result) {
        if (!result) {
            failed(std::move(result.error()));
            return;
        }
        sendResponse(requestId_, 204, {});
    };

    if (request_.totalBytes != 0 && (request_.method == "GET" || request_.method == "DELETE")) {
        failed(api::httpError(HTTP_CODE_BAD_REQUEST, "unexpected_body",
                              "This endpoint does not accept a request body"));
        return;
    }

    const std::lock_guard operationLock{api_.operationsMutex_};
    const std::string_view method = request_.method;
    const std::string_view path = request_.path;

    if (method == "GET" && path == "/api/v2/device") {
        sendValue(api_.getDevice(request));
        return;
    }
    if (method == "GET" && path == "/api/v2/library") {
        std::string json{"["};
        for (size_t index = 0; index < api_.storage_.books().size(); ++index) {
            auto book = api_.encodeBook(index);
            if (!book) {
                failed(std::move(book.error()));
                return;
            }
            if (index != 0)
                json += ',';
            json += *book;
        }
        json += ']';
        sendResponse(requestId_, 200, std::move(json));
        return;
    }
    if (method == "GET" && path == "/api/v2/themes") {
        sendValue(api_.getThemes(request));
        return;
    }
    if (method == "GET" && path == "/api/v2/fonts") {
        sendValue(api_.getFonts(request));
        return;
    }
    if (method == "GET" && path == "/api/v2/locales") {
        sendValue(api_.getLocales(request));
        return;
    }
    if (method == "GET" && path == "/api/v2/settings") {
        const auto& settings = api_.settingsStore_.settings();
        auto json = api_.encodeResponse(
            glz::obj{"reading", settings.reading, "interface", settings.interface, "updates", settings.updates});
        if (!json) {
            failed(std::move(json.error()));
            return;
        }
        sendResponse(requestId_, 200, std::move(*json));
        return;
    }
    if (method == "GET" && path == "/api/v2/network") {
        sendValue(api_.getNetwork(request));
        return;
    }
    if (method == "GET" && path == "/api/v2/feeds") {
        sendValue(api_.getFeeds(request));
        return;
    }
    if (method == "GET" && path == "/api/v2/focus-timers") {
        sendValue(api_.getFocusTimers(request));
        return;
    }

    if (method == "POST" && path == "/api/v2/library") {
        auto result = api_.installLibraryItem(request);
        if (!result) {
            failed(std::move(result.error()));
            return;
        }
        sendResponse(requestId_, 201, std::move(*result));
        return;
    }
    if (method == "POST" && path == "/api/v2/themes") {
        sendLocated(api_.postTheme(request));
        return;
    }
    if (method == "POST" && path == "/api/v2/fonts") {
        sendLocated(api_.postFont(request));
        return;
    }
    if (method == "POST" && path == "/api/v2/locales") {
        sendLocated(api_.postLocale(request));
        return;
    }
    if (method == "POST" && path == "/api/v2/storage/repair") {
        sendValue(api_.repairStorage(request));
        return;
    }

    if (method == "DELETE" && resourcePath(path, "/api/v2/library/"))
        sendEmpty(api_.deleteLibraryItem(request));
    else if (method == "PUT" && resourcePath(path, "/api/v2/library/", "/position"))
        sendEmpty(api_.putBookPosition(request));
    else if (method == "PUT" && resourcePath(path, "/api/v2/library/", "/language-fonts"))
        sendEmpty(api_.putBookLanguageFonts(request));
    else if (method == "DELETE" && resourcePath(path, "/api/v2/themes/"))
        sendEmpty(api_.deleteTheme(request));
    else if (method == "DELETE" && resourcePath(path, "/api/v2/fonts/"))
        sendEmpty(api_.deleteFont(request));
    else if (method == "DELETE" && resourcePath(path, "/api/v2/locales/"))
        sendEmpty(api_.deleteLocale(request));
    else if (method == "PUT" && path == "/api/v2/appearance/theme")
        sendEmpty(api_.putThemeSelection(request));
    else if (method == "PUT" && path == "/api/v2/appearance/font")
        sendEmpty(api_.putFontSelection(request));
    else if (method == "PUT" && path == "/api/v2/appearance/locale")
        sendEmpty(api_.putLocaleSelection(request));
    else if (method == "PATCH" && path == "/api/v2/settings/reading")
        sendEmpty(api_.patchReadingSettings(request));
    else if (method == "PATCH" && path == "/api/v2/settings/display")
        sendEmpty(api_.patchDisplaySettings(request));
    else if (method == "PATCH" && path == "/api/v2/settings/updates")
        sendEmpty(api_.patchUpdateSettings(request));
    else if (method == "PUT" && path == "/api/v2/network")
        sendEmpty(api_.putNetwork(request));
    else if (method == "DELETE" && path == "/api/v2/network")
        sendEmpty(api_.deleteNetwork(request));
    else if (method == "PUT" && path == "/api/v2/feeds")
        sendEmpty(api_.putFeeds(request));
    else if (method == "PUT" && path == "/api/v2/focus-timers")
        sendEmpty(api_.putFocusTimers(request));
    else
        failed(api::httpError(HTTP_CODE_NOT_FOUND, "not_found", "API endpoint not found"));
}

void CompanionSerial::sendResponse(uint32_t requestId, int status, std::string body) {
    ResponseMetadata metadata{.status = status, .totalBytes = body.size()};
    std::string responseJson;
    if (!companion::api::encode(metadata, responseJson))
        return;
    sendFrame({.type = companion::serial::FrameType::Response,
               .requestId = requestId,
               .payload = {responseJson.begin(), responseJson.end()}});
    responseRequestId_ = requestId;
    responseSequence_ = 0;
    responseOffset_ = 0;
    responseBody_.assign(body.begin(), body.end());
    sendNextResponseChunk();
}

void CompanionSerial::sendNextResponseChunk() {
    if (responseRequestId_ == 0)
        return;
    if (responseOffset_ >= responseBody_.size()) {
        sendFrame({.type = companion::serial::FrameType::End,
                   .requestId = responseRequestId_,
                   .sequence = responseSequence_});
        responseBody_.clear();
        responseRequestId_ = 0;
        responseSequence_ = 0;
        responseOffset_ = 0;
        return;
    }

    const size_t count = std::min(companion::serial::kChunkBytes, responseBody_.size() - responseOffset_);
    sendFrame({.type = companion::serial::FrameType::Data,
               .requestId = responseRequestId_,
               .sequence = responseSequence_,
               .payload = {responseBody_.begin() + static_cast<ptrdiff_t>(responseOffset_),
                           responseBody_.begin() + static_cast<ptrdiff_t>(responseOffset_ + count)}});
    responseOffset_ += count;
}

void CompanionSerial::sendError(uint32_t requestId, int status, std::string code, std::string message,
                                std::optional<std::string> field) {
    std::string body;
    if (!companion::api::encode(ErrorBody{std::move(code), std::move(message), std::move(field)}, body))
        body = R"({"code":"usb_error","message":"USB request failed"})";
    sendResponse(requestId, status, std::move(body));
}

void CompanionSerial::sendProtocolError(uint32_t requestId, std::string message) {
    sendFrame({.type = companion::serial::FrameType::Error,
               .requestId = requestId,
               .payload = {message.begin(), message.end()}});
}

void CompanionSerial::sendFrame(companion::serial::Frame frame) {
    const auto bytes = companion::serial::encode(frame);
    if (!bytes.empty())
        Serial.write(bytes.data(), bytes.size());
}

void CompanionSerial::resetRequest() {
    if (requestFile_)
        requestFile_.close();
    auto& filesystem = Board::Storage::filesystem();
    if (filesystem.exists(kSpoolPath.data()))
        filesystem.remove(kSpoolPath.data());
    request_ = {};
    requestBody_.clear();
    requestId_ = 0;
    expectedSequence_ = 0;
    receivedBytes_ = 0;
    requestCrc_ = 0xFFFFFFFFU;
    requestSpooled_ = false;
}
