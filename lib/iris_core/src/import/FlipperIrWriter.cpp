#include "import/FlipperIrWriter.h"

#include <cstdio>

#include "domain/Limits.h"
#include "import/FlipperSynth.h"

namespace iris {

namespace {

std::string bytes4(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%02X %02X %02X %02X", static_cast<unsigned>(v & 0xFF), static_cast<unsigned>((v >> 8) & 0xFF),
                static_cast<unsigned>((v >> 16) & 0xFF), static_cast<unsigned>((v >> 24) & 0xFF));
  return b;
}

std::string rawBlock(const std::vector<uint16_t>& durations, uint32_t carrierHz) {
  std::string out = "type: raw\nfrequency: " + std::to_string(carrierHz ? carrierHz : 38000) + "\nduty_cycle: 0.330000\ndata:";
  for (uint16_t d : durations) out += " " + std::to_string(d);
  out += "\n";
  return out;
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
      case Protocol::Rc5:
        if (addr > 0x1F) return {};
        if (d.bits == 12 && cmd <= 0x3F) {
          protocol = "RC5";
        } else if (d.bits == 13 && cmd <= 0x7F) {
          protocol = (cmd & 0x40) ? "RC5X" : "RC5";  // without the seventh command bit it is a plain RC5 frame
          cmd &= 0x3F;
        } else {
          return {};
        }
        break;
      case Protocol::Jvc: {
        // Flipper has no JVC protocol: the frame is written as RAW.
        if (d.bits != 16 || addr > 0xFF || cmd > 0xFF) return {};
        std::vector<uint16_t> durations;
        uint32_t carrier = 0;
        synthesizeJvcFrame(static_cast<uint8_t>(addr), static_cast<uint8_t>(cmd), durations, carrier);
        return out + rawBlock(durations, carrier);
      }
      default:
        return {};
    }
    out += "type: parsed\nprotocol: " + protocol + "\naddress: " + bytes4(addr) + "\ncommand: " + bytes4(cmd) + "\n";
    return out;
  }
  if (f.raw.durationsUs.empty()) return {};
  return out + rawBlock(f.raw.durationsUs, f.raw.carrierHz);
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
