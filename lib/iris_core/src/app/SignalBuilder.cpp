#include "app/SignalBuilder.h"

#include "domain/SignalValidation.h"

namespace iris {

Result<IrSignal> buildDecodedSignal(Protocol protocol, uint32_t address, uint32_t command,
                                    uint16_t bits, bool toggle) {
  DecodedFrame d;
  d.protocol = protocol;
  d.address = address;
  d.command = command;
  d.bits = bits;
  d.toggle = toggle;
  Status st = validateDecodedFrame(d);
  if (!st.ok()) return st;

  IrSignal s;
  s.provenance = Provenance::Manual;
  s.recognition = Recognition::Recognized;
  s.initial.push_back(Frame::makeDecoded(std::move(d)));
  return s;
}

Result<IrSignal> buildRawSignal(const std::vector<uint16_t>& durationsUs, uint32_t carrierHz) {
  RawFrame r;
  r.durationsUs = durationsUs;
  r.carrierHz = carrierHz;
  Status st = validateRawFrame(r);
  if (!st.ok()) return st;

  IrSignal s;
  s.provenance = Provenance::Manual;
  s.recognition = Recognition::Unknown;  // raw data has no protocol interpretation
  s.initial.push_back(Frame::makeRaw(std::move(r)));
  return s;
}

}  // namespace iris
