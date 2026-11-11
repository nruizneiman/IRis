// Protocol catalogue used for validation and manual signal creation.
// Field widths are the app's own contract. The mapping to IRremoteESP8266 is cross-checked on the PC against the
// library sources (test/hostlib, docs/firmware/ir.md).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace iris {

enum class Protocol : uint8_t { Unknown = 0, Nec, Samsung, Sony, Rc5, Jvc };

struct ProtocolVariant {
  uint16_t bits;
  uint8_t addressBits;
  uint8_t commandBits;
};

struct ProtocolInfo {
  Protocol id;
  const char* key;  // stable identifier used in stored data; never change once released
  const char* displayName;
  uint32_t defaultCarrierHz;
  bool hasToggle;
  uint8_t variantCount;
  ProtocolVariant variants[3];
};

const ProtocolInfo* findProtocol(Protocol id);
const ProtocolInfo* findProtocolByKey(std::string_view key);
const ProtocolInfo* protocolAt(size_t index);  // nullptr past the end
size_t protocolCount();

const ProtocolVariant* findVariant(const ProtocolInfo& info, uint16_t bits);

constexpr uint32_t maskForBits(unsigned bits) {
  return bits >= 32 ? 0xFFFFFFFFu : ((1u << bits) - 1u);
}

}  // namespace iris
