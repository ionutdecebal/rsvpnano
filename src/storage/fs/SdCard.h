#pragma once

#include <cstddef>
#include <string_view>

namespace SdCard {

    bool mount(bool& mounted, int* mountedFrequencyKhz = nullptr);
    bool probe(std::string_view path, size_t bytes, const char* tag);
    int mountedFrequencyKhz();

} // namespace SdCard
