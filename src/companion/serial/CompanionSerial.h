#pragma once

#include <Arduino.h>
#include <FS.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <improv.h>

#include "companion/serial/CompanionSerialProtocol.h"

class CompanionApi;
class UsbMassStorageManager;
namespace companion {
    struct BufferedRequest;
}

class CompanionSerial {
public:
    CompanionSerial(CompanionApi& api, UsbMassStorageManager& massStorage) :
            api_(api), massStorage_(massStorage) {}

    void update(uint32_t nowMs);
    void close();
    [[nodiscard]] bool active() const noexcept;

private:
    struct RequestMetadata {
        std::string method;
        std::string path;
        std::map<std::string, std::string, std::less<>> query;
        uint64_t totalBytes = 0;
    };

    struct TransferEnd {
        uint64_t totalBytes = 0;
        uint32_t crc32 = 0;
    };

    struct ResponseMetadata {
        int status = 200;
        std::string contentType = "application/json";
        uint64_t totalBytes = 0;
    };

    void readHandshake(uint32_t nowMs);
    void updateImprov(uint32_t nowMs);
    void readImprovByte(uint8_t byte, uint32_t nowMs);
    bool handleImprovCommand(improv::ImprovCommand command, uint32_t nowMs);
    void sendImprov(uint8_t type, std::span<const uint8_t> data);
    void sendImprovState(improv::State state);
    void sendImprovError(improv::Error error);
    void sendImprovResponse(improv::Command command, const std::vector<std::string>& values);
    void readFrames(uint32_t nowMs);
    void handleFrame(companion::serial::Frame frame, uint32_t nowMs);
    void handleRequestEnd(const companion::serial::Frame& frame);
    void dispatchRequest(companion::BufferedRequest& buffered);
    void sendResponse(uint32_t requestId, int status, std::string body);
    void sendNextResponseChunk();
    void sendError(uint32_t requestId, int status, std::string code, std::string message,
                   std::optional<std::string> field = std::nullopt);
    void sendProtocolError(uint32_t requestId, std::string message);
    void sendFrame(companion::serial::Frame frame);
    void resetRequest();

    CompanionApi& api_;
    UsbMassStorageManager& massStorage_;
    companion::serial::Decoder decoder_;
    std::string handshake_;
    std::vector<uint8_t> improvBuffer_;
    std::string provisioningSsid_;
    std::string provisioningPassword_;
    RequestMetadata request_;
    std::vector<uint8_t> requestBody_;
    std::string responseBody_;
    File requestFile_;
    uint32_t requestId_ = 0;
    uint32_t expectedSequence_ = 0;
    uint64_t receivedBytes_ = 0;
    uint32_t requestCrc_ = 0xFFFFFFFFU;
    uint32_t responseRequestId_ = 0;
    uint32_t responseSequence_ = 0;
    size_t responseOffset_ = 0;
    uint32_t lastTrafficMs_ = 0;
    uint32_t improvLastByteMs_ = 0;
    uint32_t provisioningDeadlineMs_ = 0;
    improv::State improvState_ = improv::STATE_AUTHORIZED;
    bool active_ = false;
    bool writeFailed_ = false;
    bool requestSpooled_ = false;
};
