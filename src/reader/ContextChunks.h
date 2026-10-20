#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace ContextChunks {
    // A bounded lookahead, independent of book or paragraph length.
    inline constexpr size_t kLookahead = 32;
    inline constexpr size_t kMaximumWordBytes = 128;
    using Fits = std::function<bool(std::string_view)>;
    bool boundary(std::string_view word);
    bool englishToken(std::string_view word);
    uint8_t count(std::span<const std::string> words, const Fits& fits);
}
