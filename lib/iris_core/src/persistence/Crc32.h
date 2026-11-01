#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace iris {

// Standard CRC-32 (IEEE 802.3, reflected, poly 0xEDB88320). crc32("123456789") == 0xCBF43926.
uint32_t crc32(const void* data, size_t size);
inline uint32_t crc32(std::string_view s) { return crc32(s.data(), s.size()); }

}  // namespace iris
