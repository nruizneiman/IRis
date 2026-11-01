#include "ir/IrCodec.h"

#include "domain/SignalValidation.h"

namespace iris {

std::optional<LibFrame> toLibraryFrame(IRsend& sender, const DecodedFrame& f) {
  if (!validateDecodedFrame(f).ok()) return std::nullopt;
  LibFrame out;
  out.nbits = f.bits;
  switch (f.protocol) {
    case Protocol::Nec:
      out.type = NEC;
      out.data = sender.encodeNEC(static_cast<uint16_t>(f.address), static_cast<uint16_t>(f.command));
      break;
    case Protocol::Samsung:
      out.type = SAMSUNG;
      out.data = sender.encodeSAMSUNG(static_cast<uint8_t>(f.address), static_cast<uint8_t>(f.command));
      break;
    case Protocol::Sony:
      out.type = SONY;
      // 12/15 bit: 7 command bits. 20 bit: the 8 "extended" bits are the upper part of our 15-bit command.
      out.data = sender.encodeSony(f.bits, static_cast<uint16_t>(f.command & 0x7F), static_cast<uint16_t>(f.address),
                                   static_cast<uint16_t>(f.command >> 7));
      break;
    case Protocol::Rc5:
      out.type = RC5;
      out.data = f.bits == 13 ? sender.encodeRC5X(static_cast<uint8_t>(f.address), static_cast<uint8_t>(f.command), f.toggle)
                              : sender.encodeRC5(static_cast<uint8_t>(f.address), static_cast<uint8_t>(f.command), f.toggle);
      break;
    case Protocol::Jvc:
      out.type = JVC;
      out.data = sender.encodeJVC(static_cast<uint8_t>(f.address), static_cast<uint8_t>(f.command));
      break;
    case Protocol::Unknown:
      return std::nullopt;
  }
  out.repeat = IRsend::minRepeats(out.type);
  return out;
}

std::optional<DecodedFrame> fromLibraryResult(const decode_results& r) {
  if (r.overflow || r.repeat) return std::nullopt;
  DecodedFrame d;
  d.address = r.address;
  d.command = r.command;
  d.bits = r.bits;
  switch (r.decode_type) {
    case NEC: d.protocol = Protocol::Nec; break;
    case SAMSUNG: d.protocol = Protocol::Samsung; break;
    case SONY: d.protocol = Protocol::Sony; break;
    case JVC: d.protocol = Protocol::Jvc; break;
    case RC5:
    case RC5X:
      d.protocol = Protocol::Rc5;
      d.toggle = ((r.value >> (kRC5Bits - 1)) & 1) != 0;  // bit 11 of the received word
      break;
    default:
      return std::nullopt;
  }
  if (!validateDecodedFrame(d).ok()) return std::nullopt;
  return d;
}

std::vector<uint16_t> rawDurationsUs(const decode_results& r) {
  std::vector<uint16_t> out;
  if (r.rawlen < 2) return out;
  out.reserve(static_cast<size_t>(r.rawlen) - 1);
  for (uint16_t i = 1; i < r.rawlen; ++i) {
    const uint32_t us = static_cast<uint32_t>(r.rawbuf[i]) * kRawTick;
    out.push_back(us > 0xFFFFu ? static_cast<uint16_t>(0xFFFFu) : static_cast<uint16_t>(us));
  }
  return out;
}

std::optional<ReceivedCapture> captureFromResult(const decode_results& r, uint8_t receiverIndex, uint32_t timestampMs) {
  // A held NEC/JVC key sends short repeat codes after the first full frame: not a new signal.
  if (r.repeat) return std::nullopt;
  ReceivedCapture c;
  c.raw.durationsUs = rawDurationsUs(r);
  if (c.raw.durationsUs.empty()) return std::nullopt;
  c.raw.carrierHz = 0;  // a demodulating receiver cannot tell
  c.bufferOverflow = r.overflow;
  c.decoded = fromLibraryResult(r);
  c.receiverIndex = receiverIndex;
  c.timestampMs = timestampMs;
  return c;
}

}  // namespace iris
