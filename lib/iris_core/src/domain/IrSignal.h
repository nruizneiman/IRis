// Signal model. A signal is NOT a flat list of pulses: it is a sequence of frames with an
// initial part, a repeat part (sent while a key is held / N times) and an optional untouched
// original RAW capture kept next to the decoded interpretation.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "domain/Protocol.h"

namespace iris {

// Mark/space durations in microseconds, first entry is a mark. carrierHz == 0 means unknown:
// a demodulating receiver (TSOP38238) does not report the original carrier.
struct RawFrame {
  std::vector<uint16_t> durationsUs;
  uint32_t carrierHz = 0;
};

struct DecodedFrame {
  Protocol protocol = Protocol::Unknown;
  uint32_t address = 0;
  uint32_t command = 0;
  uint16_t bits = 0;
  bool toggle = false;           // RC5-style alternating bit
  std::vector<uint8_t> state;    // reserved for state-based protocols (not used by current catalogue)
};

struct Frame {
  enum class Kind : uint8_t { Decoded, Raw };
  Kind kind = Kind::Decoded;
  DecodedFrame decoded;
  RawFrame raw;

  static Frame makeDecoded(DecodedFrame d) {
    Frame f;
    f.kind = Kind::Decoded;
    f.decoded = std::move(d);
    return f;
  }
  static Frame makeRaw(RawFrame r) {
    Frame f;
    f.kind = Kind::Raw;
    f.raw = std::move(r);
    return f;
  }
};

enum class Provenance : uint8_t { Learned, Manual, Synthetic };
enum class Recognition : uint8_t { Recognized, Partial, Unknown, Invalid };

struct QualityFlags {
  bool empty = false;
  bool truncated = false;
  bool possibleNoise = false;
};

struct IrSignal {
  Provenance provenance = Provenance::Manual;
  Recognition recognition = Recognition::Recognized;
  std::vector<Frame> initial;
  std::vector<Frame> repeat;
  uint8_t repeatCount = 0;      // how many times the repeat list is sent after the initial list
  uint32_t repeatGapUs = 0;     // pause between frames of the sequence
  std::optional<RawFrame> originalCapture;
  QualityFlags quality;
  std::string note;
};

}  // namespace iris
