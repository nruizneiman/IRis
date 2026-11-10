#include "import/FlipperIrWriter.h"

#include <cstdio>

#include "domain/Limits.h"

namespace iris {

namespace {

std::string bytes4(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%02X %02X %02X %02X", static_cast<unsigned>(v & 0xFF), static_cast<unsigned>((v >> 8) & 0xFF),
                static_cast<unsigned>((v >> 16) & 0xFF), static_cast<unsigned>((v >> 24) & 0xFF));
  return b;
}

// One block, or empty when the signal cannot be written.
std::string block(const std::string& name, const IrSignal& s) {
  if (s.initial.empty()) return {};
  const Frame& f = s.initial[0];
  std::string out = "#\nname: " + name + "\n";
  if (f.kind == Frame::Kind::Decoded) {
    const DecodedFrame& d = f.decoded;
    std::string protocol;
    uint32_t addr = d.address, cmd = d.command;
    switch (d.protocol) {
      case Protocol::Nec:
        if (d.bits != 32 || cmd > 0xFF || addr > 0xFFFF) return {};
        if (addr <= 0xFF) {
          protocol = "NEC";
        } else {
          protocol = "NECext";
          cmd = cmd | (((~cmd) & 0xFF) << 8);
        }
        break;
      case Protocol::Samsung:
        if (d.bits != 32 || cmd > 0xFF || addr > 0xFF) return {};
        protocol = "Samsung32";
        break;
      case Protocol::Sony:
        if (d.bits == 12) {
          if (addr > 0x1F || cmd > 0x7F) return {};
          protocol = "SIRC";
        } else if (d.bits == 15) {
          if (addr > 0xFF || cmd > 0x7F) return {};
          protocol = "SIRC15";
        } else if (d.bits == 20) {
          if (addr > 0x1F || cmd > 0x7FFF) return {};
          protocol = "SIRC20";
          addr = addr | ((cmd >> 7) << 5);  // the extended bits travel in the address field
          cmd = cmd & 0x7F;
        } else {
          return {};
        }
        break;
      default:
        return {};
    }
    out += "type: parsed\nprotocol: " + protocol + "\naddress: " + bytes4(addr) + "\ncommand: " + bytes4(cmd) + "\n";
    return out;
  }
  if (f.raw.durationsUs.empty()) return {};
  const uint32_t carrier = f.raw.carrierHz ? f.raw.carrierHz : 38000;
  out += "type: raw\nfrequency: " + std::to_string(carrier) + "\nduty_cycle: 0.330000\ndata:";
  for (uint16_t d : f.raw.durationsUs) out += " " + std::to_string(d);
  out += "\n";
  return out;
}

}  // namespace

WrittenIr writeFlipperIr(const std::vector<ViewButton>& buttons) {
  WrittenIr w;
  w.text = "Filetype: IR signals file\nVersion: 1\n";
  for (const ViewButton& b : buttons) {
    const std::string blk = b.signal ? block(b.name, *b.signal) : std::string();
    if (blk.empty()) {
      ++w.skipped;
      continue;
    }
    w.text += blk;
    ++w.written;
  }
  return w;
}

}  // namespace iris
