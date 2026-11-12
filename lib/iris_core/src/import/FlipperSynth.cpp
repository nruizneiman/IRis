#include "import/FlipperSynth.h"

namespace iris {

namespace {

// Collects levels and merges neighbours of the same level, so the result always alternates mark, space, mark...
class Pulses {
 public:
  explicit Pulses(std::vector<uint16_t>& out) : out_(out) { out_.clear(); }

  void add(bool mark, uint32_t us) {
    if (out_.empty() && !mark) return;  // a frame starts with a mark
    const bool lastIsMark = out_.size() % 2 == 1;
    if (!out_.empty() && lastIsMark == mark) {
      const uint32_t sum = out_.back() + us;
      out_.back() = static_cast<uint16_t>(sum > 0xFFFF ? 0xFFFF : sum);
    } else {
      out_.push_back(static_cast<uint16_t>(us > 0xFFFF ? 0xFFFF : us));
    }
  }
  void mark(uint32_t us) { add(true, us); }
  void space(uint32_t us) { add(false, us); }
  void finish() {
    if (!out_.empty() && out_.size() % 2 == 0) out_.pop_back();  // never end on a space
  }

 private:
  std::vector<uint16_t>& out_;
};

struct PulseDistance {
  uint16_t headerMark, headerSpace, bitMark, oneSpace, zeroSpace;
};

constexpr PulseDistance kNec{9000, 4500, 560, 1690, 560};
constexpr PulseDistance kKaseikyo{3456, 1728, 432, 1296, 432};
constexpr PulseDistance kRca{4000, 4000, 500, 2000, 1000};
constexpr PulseDistance kPioneer{8500, 4225, 500, 1500, 500};
constexpr PulseDistance kJvc{8400, 4200, 525, 1725, 525};

// Header, `bits` data bits taken LSB first from `data`, and the closing mark.
void pulseDistance(Pulses& p, const PulseDistance& t, uint64_t data, unsigned bits) {
  p.mark(t.headerMark);
  p.space(t.headerSpace);
  for (unsigned i = 0; i < bits; ++i) {
    p.mark(t.bitMark);
    p.space(((data >> i) & 1) ? t.oneSpace : t.zeroSpace);
  }
  p.mark(t.bitMark);
}

uint32_t reverse8(uint32_t v) {
  uint32_t r = 0;
  for (int i = 0; i < 8; ++i) r |= ((v >> i) & 1u) << (7 - i);
  return r;
}

}  // namespace

SynthResult synthesizeFlipperFrame(std::string_view protocol, uint32_t address, uint32_t command,
                                   std::vector<uint16_t>& out, uint32_t& carrierHz) {
  Pulses p(out);
  carrierHz = 38000;
  if (protocol == "NECext") {
    if (address > 0xFFFF || command > 0xFFFF) return SynthResult::OutOfRange;
    pulseDistance(p, kNec, static_cast<uint64_t>(address) | (static_cast<uint64_t>(command) << 16), 32);
  } else if (protocol == "NEC42") {
    if (address > 0x1FFF || command > 0xFF) return SynthResult::OutOfRange;
    const uint64_t data = static_cast<uint64_t>(address) | (static_cast<uint64_t>(~address & 0x1FFF) << 13) |
                          (static_cast<uint64_t>(command) << 26) | (static_cast<uint64_t>(~command & 0xFF) << 34);
    pulseDistance(p, kNec, data, 42);
  } else if (protocol == "NEC42ext") {
    if (address > 0x3FFFFFF || command > 0xFFFF) return SynthResult::OutOfRange;
    pulseDistance(p, kNec, static_cast<uint64_t>(address) | (static_cast<uint64_t>(command) << 26), 42);
  } else if (protocol == "Kaseikyo") {
    if (address > 0x3FFFFFF || command > 0x3FF) return SynthResult::OutOfRange;
    const uint32_t id = (address >> 24) & 3, vendor = (address >> 8) & 0xFFFF;
    const uint32_t genre1 = (address >> 4) & 0xF, genre2 = address & 0xF;
    uint8_t b[6];
    b[0] = static_cast<uint8_t>(vendor & 0xFF);
    b[1] = static_cast<uint8_t>(vendor >> 8);
    uint8_t vendorParity = static_cast<uint8_t>(b[0] ^ b[1]);
    vendorParity = static_cast<uint8_t>((vendorParity & 0xF) ^ (vendorParity >> 4));
    b[2] = static_cast<uint8_t>((vendorParity & 0xF) | (genre1 << 4));
    b[3] = static_cast<uint8_t>(genre2 | ((command & 0xF) << 4));
    b[4] = static_cast<uint8_t>((id << 6) | (command >> 4));
    b[5] = static_cast<uint8_t>(b[2] ^ b[3] ^ b[4]);
    uint64_t data = 0;
    for (int i = 0; i < 6; ++i) data |= static_cast<uint64_t>(b[i]) << (8 * i);
    pulseDistance(p, kKaseikyo, data, 48);
  } else if (protocol == "RCA") {
    if (address > 0xF || command > 0xFF) return SynthResult::OutOfRange;
    const uint64_t data = address | (command << 4) | ((~address & 0xF) << 12) | ((~command & 0xFF) << 16);
    pulseDistance(p, kRca, data, 24);
  } else if (protocol == "Pioneer") {
    if (address > 0xFF || command > 0xFF) return SynthResult::OutOfRange;
    carrierHz = 40000;
    const uint64_t data = address | ((~address & 0xFF) << 8) | (command << 16) | (static_cast<uint64_t>(~command & 0xFF) << 24);
    pulseDistance(p, kPioneer, data, 32);
    p.space(kPioneer.zeroSpace + 26000u);  // Flipper sends the frame twice, 26 ms apart
    pulseDistance(p, kPioneer, data, 32);
  } else if (protocol == "RC6") {
    if (address > 0xFF || command > 0xFF) return SynthResult::OutOfRange;
    carrierHz = 36000;
    // Bit 0 start (1), bits 1-3 mode (0), bit 4 toggle (0, twice as long), then address and command MSB first.
    const uint32_t data = 1u | (reverse8(address) << 5) | (reverse8(command) << 13);
    p.mark(2666);
    p.space(889);
    for (unsigned i = 0; i < 21; ++i) {
      const bool one = ((data >> i) & 1) != 0;
      const uint32_t half = i == 4 ? 888 : 444;
      p.add(one, half);  // a one is mark then space, a zero is space then mark
      p.add(!one, half);
    }
  } else {
    out.clear();
    return SynthResult::UnknownProtocol;
  }
  p.finish();
  return SynthResult::Ok;
}

void synthesizeJvcFrame(uint8_t address, uint8_t command, std::vector<uint16_t>& out, uint32_t& carrierHz) {
  Pulses p(out);
  carrierHz = 38000;
  pulseDistance(p, kJvc, static_cast<uint64_t>(address) | (static_cast<uint64_t>(command) << 8), 16);
  p.finish();
}

}  // namespace iris
