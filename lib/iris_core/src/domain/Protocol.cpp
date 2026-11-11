#include "domain/Protocol.h"

namespace iris {

namespace {

const ProtocolInfo kProtocols[] = {
    {Protocol::Nec, "nec", "NEC", 38000, false, 1, {{32, 16, 8}}},
    {Protocol::Samsung, "samsung", "Samsung", 38000, false, 1, {{32, 8, 8}}},
    // Sony 20 bit: 5 address bits + 8 "extended" bits, which IRremoteESP8266 reports as the upper bits
    // of the command (7 + 8 = 15 command bits). Confirmed by reading ir_Sony.cpp.
    {Protocol::Sony, "sony", "Sony SIRC", 40000, false, 3, {{12, 5, 7}, {15, 8, 7}, {20, 5, 15}}},
    {Protocol::Rc5, "rc5", "RC5", 36000, true, 2, {{12, 5, 6}, {13, 5, 7}}},
    {Protocol::Jvc, "jvc", "JVC", 38000, false, 1, {{16, 8, 8}}},
};

constexpr size_t kProtocolCount = sizeof(kProtocols) / sizeof(kProtocols[0]);

}  // namespace

size_t protocolCount() { return kProtocolCount; }

const ProtocolInfo* protocolAt(size_t index) {
  return index < kProtocolCount ? &kProtocols[index] : nullptr;
}

const ProtocolInfo* findProtocol(Protocol id) {
  for (const auto& p : kProtocols) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

const ProtocolInfo* findProtocolByKey(std::string_view key) {
  for (const auto& p : kProtocols) {
    if (key == p.key) return &p;
  }
  return nullptr;
}

const ProtocolVariant* findVariant(const ProtocolInfo& info, uint16_t bits) {
  for (uint8_t i = 0; i < info.variantCount; ++i) {
    if (info.variants[i].bits == bits) return &info.variants[i];
  }
  return nullptr;
}

}  // namespace iris
