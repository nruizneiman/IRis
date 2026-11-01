#pragma once

#include <cstdint>
#include <vector>

#include "Result.h"
#include "domain/IrSignal.h"

namespace iris {

Status validateDecodedFrame(const DecodedFrame& frame);
Status validateRawFrame(const RawFrame& frame);
Status validateFrame(const Frame& frame);
// A signal that passes can be stored and transmitted. Recognition::Invalid never passes.
Status validateSignal(const IrSignal& signal);

// Result of looking at a freshly captured duration list (heuristics, not proof).
struct CaptureAnalysis {
  Recognition verdict = Recognition::Unknown;
  QualityFlags quality;
  Status status;  // not ok when the capture must not be stored (empty / too short)
};

// Heuristics: empty -> Invalid; fewer than 4 entries -> Invalid; reaching the buffer limit ->
// truncated; more than 30% of entries shorter than 100 us -> possible noise.
CaptureAnalysis analyzeCapture(const std::vector<uint16_t>& durationsUs);

size_t rawEntryCount(const IrSignal& signal);

}  // namespace iris
