// SYNTHETIC test data. None of these are captures from real hardware. NEC timings are generated
// from the nominal published NEC description (9 ms / 4.5 ms leader, 562.5 us unit), so they show
// what the code should handle, not what a TSOP38238 really reports (which differs by tens of us).
#pragma once

#include <cstdint>
#include <vector>

#include "domain/IrSignal.h"

namespace iris::testkit {

// Nominal NEC frame (32 bits, LSB first) as mark/space durations, including the final mark.
inline std::vector<uint16_t> syntheticNecFrame(uint32_t data32) {
  std::vector<uint16_t> d = {9000, 4500};
  for (int i = 0; i < 32; ++i) {
    d.push_back(560);
    d.push_back(((data32 >> i) & 1u) ? 1690 : 560);
  }
  d.push_back(560);
  return d;
}

// Nominal NEC "repeat" frame (leader + short space + one mark).
inline std::vector<uint16_t> syntheticNecRepeat() { return {9000, 2250, 560}; }

// A payload that does not look like any catalogue protocol. SYNTHETIC.
inline std::vector<uint16_t> syntheticUnknownPattern() {
  return {3100, 1550, 420, 1210, 380, 380, 400, 1190, 410, 390, 1500, 2600, 450};
}

inline std::vector<uint16_t> emptyCapture() { return {}; }

// Mostly sub-100us glitches: what a noisy environment might look like. SYNTHETIC.
inline std::vector<uint16_t> syntheticNoiseCapture() {
  return {40, 55, 30, 70, 45, 60, 35, 800, 50, 65, 25, 90};
}

inline IrSignal syntheticNecSignalWithRepeats() {
  IrSignal s;
  s.provenance = Provenance::Synthetic;
  s.recognition = Recognition::Recognized;
  DecodedFrame d;
  d.protocol = Protocol::Nec;
  d.address = 0x00;
  d.command = 0x08;
  d.bits = 32;
  s.initial.push_back(Frame::makeDecoded(d));
  RawFrame rep;
  rep.durationsUs = syntheticNecRepeat();
  rep.carrierHz = 38000;
  s.repeat.push_back(Frame::makeRaw(rep));
  s.repeatCount = 3;
  s.repeatGapUs = 40000;
  RawFrame orig;
  orig.durationsUs = syntheticNecFrame(0xF708FF00u);
  s.originalCapture = orig;
  s.note = "SYNTHETIC: generated from nominal NEC timings";
  return s;
}

}  // namespace iris::testkit
