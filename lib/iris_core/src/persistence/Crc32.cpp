#include "persistence/Crc32.h"

namespace iris {

uint32_t crc32(const void* data, size_t size) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < size; ++i) {
    crc ^= p[i];
    for (int k = 0; k < 8; ++k) {
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}

}  // namespace iris
