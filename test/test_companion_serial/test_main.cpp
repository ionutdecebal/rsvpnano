#include <unity.h>

#include <array>
#include <algorithm>
#include <string>
#include <vector>

#include "companion/serial/CompanionSerialProtocol.h"

namespace serial = companion::serial;

void setUp() {}
void tearDown() {}

void test_fragmented_and_coalesced_frames_resynchronize() {
    const serial::Frame first{.type = serial::FrameType::Request,
                              .requestId = 7,
                              .payload = {'{', '}'}};
    const serial::Frame second{.type = serial::FrameType::Ping};
    auto firstBytes = serial::encode(first);
    auto secondBytes = serial::encode(second);

    std::vector<uint8_t> stream{0x01, 0x02, 'R'};
    stream.insert(stream.end(), firstBytes.begin(), firstBytes.end());
    stream.insert(stream.end(), secondBytes.begin(), secondBytes.end());

    serial::Decoder decoder;
    decoder.append(std::span{stream}.first(9));
    TEST_ASSERT_TRUE(decoder.takeFrames().empty());
    decoder.append(std::span{stream}.subspan(9));
    const auto frames = decoder.takeFrames();
    TEST_ASSERT_EQUAL_UINT32(2, frames.size());
    TEST_ASSERT_EQUAL_UINT32(7, frames[0].requestId);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(serial::FrameType::Ping),
                            static_cast<uint8_t>(frames[1].type));
}

void test_corrupt_crc_is_skipped_before_next_frame() {
    auto corrupt = serial::encode({.type = serial::FrameType::Data,
                                   .requestId = 1,
                                   .payload = {1, 2, 3}});
    corrupt.back() ^= 0xFF;
    auto valid = serial::encode({.type = serial::FrameType::Pong});
    corrupt.insert(corrupt.end(), valid.begin(), valid.end());

    serial::Decoder decoder;
    decoder.append(corrupt);
    const auto frames = decoder.takeFrames();
    TEST_ASSERT_EQUAL_UINT32(1, frames.size());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(serial::FrameType::Pong),
                            static_cast<uint8_t>(frames[0].type));
}

void test_string_response_chunks_preserve_utf8_bytes_and_crc() {
    std::string body(serial::kChunkBytes - 1, 'x');
    body += "\xE2\x80\x94\xD7\x90\xE4\xB8\xAD"; // UTF-8 crosses the frame boundary.
    std::vector<uint8_t> received;
    serial::Decoder decoder;
    for (size_t offset = 0; offset < body.size(); offset += serial::kChunkBytes) {
        const size_t count = std::min(serial::kChunkBytes, body.size() - offset);
        decoder.append(serial::encode({
            .type = serial::FrameType::Data,
            .requestId = 1,
            .payload = {body.begin() + static_cast<ptrdiff_t>(offset),
                        body.begin() + static_cast<ptrdiff_t>(offset + count)},
        }));
        const auto frames = decoder.takeFrames();
        TEST_ASSERT_EQUAL_UINT32(1, frames.size());
        received.insert(received.end(), frames[0].payload.begin(), frames[0].payload.end());
    }
    TEST_ASSERT_EQUAL_UINT32(body.size(), received.size());
    TEST_ASSERT_EQUAL_MEMORY(body.data(), received.data(), body.size());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fragmented_and_coalesced_frames_resynchronize);
    RUN_TEST(test_corrupt_crc_is_skipped_before_next_frame);
    RUN_TEST(test_string_response_chunks_preserve_utf8_bytes_and_crc);
    return UNITY_END();
}
