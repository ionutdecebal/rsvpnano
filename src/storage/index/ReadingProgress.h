#pragma once

#include <Arduino.h>
#include <cstdint>

namespace ReadingProgress {

    struct BookIdentity {
        uint32_t sourceSize = 0;
        uint32_t sourceFingerprint = 0;
        uint32_t wordCount = 0;
    };

    bool readPositionSidecar(const String& bookPath, const BookIdentity& identity, uint32_t& wordIndex);
    bool writePositionSidecar(const String& bookPath, const BookIdentity& identity, uint32_t wordIndex);

} // namespace ReadingProgress
